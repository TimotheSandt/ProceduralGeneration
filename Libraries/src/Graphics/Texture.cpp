#include "Texture.h"

#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"
#include "Graphics/Core/GraphicsRuntime.h"

#include <cstring>

Texture::Texture()
    : ID(0), slot(0), format(TextureFormat::RGBA8), pixelType(TexturePixelType::UnsignedByte), Width(0), Height(0), UniformName("")
{
}

Texture::Texture(Texture &&other) noexcept
    : ID(0), slot(0), format(TextureFormat::RGBA8), pixelType(TexturePixelType::UnsignedByte), Width(0), Height(0), UniformName("")
{
    this->Swap(other);
}

Texture &Texture::operator=(Texture &&other) noexcept
{
    if (this != &other)
    {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

void Texture::Swap(Texture &other) noexcept
{
    std::swap(this->ID, other.ID);
    std::swap(this->backendResource, other.backendResource);
    std::swap(this->slot, other.slot);
    std::swap(this->format, other.format);
    std::swap(this->pixelType, other.pixelType);
    std::swap(this->Width, other.Width);
    std::swap(this->Height, other.Height);
    std::swap(this->UniformName, other.UniformName);
}

void Texture::Copy(const Texture &texture)
{
    int width = 0;
    int height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    TexturePixelType pixelType = TexturePixelType::UnsignedByte;
    void *data = texture.GetTextureData(width, height, format, pixelType);
    this->SetTextureData(data, width, height, format, pixelType);
    std::free(data);
}

Texture Texture::Copy() const
{
    int w, h;
    TextureFormat f;
    TexturePixelType p;
    void *data = this->GetTextureData(w, h, f, p);
    Texture texture(data, w, h, this->UniformName, this->slot, f, p);
    std::free(data);
    return texture;
}

Texture::Texture(const std::string &image, const char *name, std::uint32_t slot, TextureFormat format, TexturePixelType pixelType,
                 TextureFilterMode filter)
    : slot(slot), format(format), pixelType(pixelType), Width(0), Height(0), UniformName(name)
{
    stbi_set_flip_vertically_on_load(true);
    int numColCh;
    bool isLoaded = true;
    unsigned char *bytes = stbi_load(image.c_str(), &this->Width, &this->Height, &numColCh, 0);
    if (!bytes)
    {
        LOG_ERROR(1, "Failed to load image: ", stbi_failure_reason());
        isLoaded = false;

        this->Width = 2;
        this->Height = 2;
        numColCh = 4;
        static unsigned char bytesDefault[] = {
            238, 130, 238, 255, // Violet pixel
            0,   0,   0,   255, // Black pixel
            0,   0,   0,   255, // Black pixel
            238, 130, 238, 255  // Violet pixel
        };
        bytes = bytesDefault;
        this->format = TextureFormat::RGBA8;
        this->pixelType = TexturePixelType::UnsignedByte;
    }

    this->SetTextureData(bytes, this->Width, this->Height, this->format, pixelType, filter);

    if (isLoaded)
    {
        stbi_image_free(bytes);
    }
}

Texture::Texture(void *data, int width, int height, const char *name, std::uint32_t slot, TextureFormat format, TexturePixelType pixelType,
                 TextureFilterMode filter)
    : slot(slot), format(format), pixelType(pixelType), Width(width), Height(height), UniformName(name)
{
    this->SetTextureData(data, width, height, this->format, pixelType, filter);
}

Texture::~Texture() { this->Destroy(); }

void Texture::SetTextureData(void *data, int width, int height, TextureFormat format, TexturePixelType pixelType, TextureFilterMode filter)
{
    static_cast<void>(filter);
    this->Destroy();
    this->Width = width;
    this->Height = height;
    this->format = format;
    this->pixelType = pixelType;

    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr && device->GetAPI() == GraphicsAPI::OpenGL)
    {
        TextureCreateInfo createInfo;
        createInfo.desc.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        createInfo.desc.format = format;
        createInfo.desc.mipLevels = 1;
        createInfo.desc.renderTarget = false;
        createInfo.debugName = this->UniformName;
        createInfo.generateMipmaps = format != TextureFormat::R32UI;

        const size_t dataSize = data != nullptr ? static_cast<size_t>(width) * static_cast<size_t>(height) * GetComponentCount(format) *
                                                      GetPixelTypeSize(pixelType)
                                                : 0;
        if (dataSize > 0)
        {
            createInfo.initialData.resize(dataSize);
            std::memcpy(createInfo.initialData.data(), data, dataSize);
        }

        std::unique_ptr<ITextureResource> resource = device->CreateTexture(createInfo);
        if (auto *openGLResource = dynamic_cast<OpenGLTextureResource *>(resource.get()); openGLResource != nullptr)
        {
            this->ID = openGLResource->GetTextureID();
            this->backendResource = std::move(resource);
            if (this->ID != 0)
            {
                return;
            }
            this->backendResource.reset();
        }
    }
}

void *Texture::GetTextureData(int &width, int &height, TextureFormat &format, TexturePixelType &pixelType) const
{
    width = this->Width;
    height = this->Height;
    format = this->format;
    pixelType = this->pixelType;

    if (this->backendResource == nullptr)
    {
        return nullptr;
    }

    std::vector<std::byte> rawData;
    this->backendResource->Readback(rawData);
    void *data = std::malloc(rawData.size());
    if (data != nullptr && !rawData.empty())
    {
        std::memcpy(data, rawData.data(), rawData.size());
    }
    return data;
}

size_t Texture::GetPixelTypeSize(TexturePixelType pixelType) const
{
    switch (pixelType)
    {
        case TexturePixelType::UnsignedByte:
        case TexturePixelType::Byte:
            return 1;
        case TexturePixelType::UnsignedShort:
        case TexturePixelType::Short:
        case TexturePixelType::HalfFloat:
            return 2;
        case TexturePixelType::UnsignedInt:
        case TexturePixelType::Int:
        case TexturePixelType::Float:
            return 4;
        case TexturePixelType::Double:
            return 8;
        default:
            return 1;
    }
}

size_t Texture::GetComponentCount(TextureFormat format) const
{
    switch (format)
    {
        case TextureFormat::R8:
        case TextureFormat::R32UI:
        case TextureFormat::Depth32Float:
            return 1;
        case TextureFormat::Depth24Stencil8:
        case TextureFormat::RGBA8:
        case TextureFormat::BGRA8:
            return 4;
        default:
            return 4;
    }
}

size_t Texture::GetDataSize() const { return Width * Height * GetComponentCount(format) * GetPixelTypeSize(pixelType); }

static bool IsDepthFormat(TextureFormat format)
{
    return format == TextureFormat::Depth32Float || format == TextureFormat::Depth24Stencil8;
}

void Texture::SetFramebufferTexture(const char *uniformName, std::uint32_t slot, int width, int height,
                                    std::uint32_t renderTargetHandle, std::uint32_t colorIndex, TextureFormat format)
{
    this->Destroy();
    this->slot = slot;
    this->UniformName = uniformName;
    this->Width = width;
    this->Height = height;
    this->format = format;
    this->pixelType = IsDepthFormat(format) ? TexturePixelType::Float : TexturePixelType::UnsignedByte;

    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr && device->GetAPI() == GraphicsAPI::OpenGL)
    {
        TextureCreateInfo createInfo;
        createInfo.desc.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        createInfo.desc.format = format;
        createInfo.desc.mipLevels = 1;
        createInfo.desc.renderTarget = true;
        createInfo.debugName = this->UniformName;
        createInfo.generateMipmaps = false;

        std::unique_ptr<ITextureResource> resource = device->CreateTexture(createInfo);
        if (auto *openGLResource = dynamic_cast<OpenGLTextureResource *>(resource.get()); openGLResource != nullptr)
        {
            this->ID = openGLResource->GetTextureID();
            this->backendResource = std::move(resource);
            if (this->ID != 0)
            {
                if (IsDepthFormat(format))
                {
                    this->backendResource->AttachAsDepthToFramebuffer(renderTargetHandle);
                }
                else
                {
                    this->backendResource->AttachToFramebuffer(renderTargetHandle, colorIndex);
                }
                return;
            }
            this->backendResource.reset();
        }
    }
}

void Texture::ResizeFramebufferTexture(int width, int height)
{
    this->Width = width;
    this->Height = height;
    if (this->backendResource != nullptr)
    {
        this->backendResource->Resize(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
    }
}

void Texture::texUnit(const ShaderProgram &shaderProgram) const
{
    shaderProgram.Bind();
    const int location = shaderProgram.GetUniformLocation(this->UniformName);
    const int slotValue = static_cast<int>(this->slot);
    shaderProgram.SetUniformInts(location, &slotValue, 1);
}

void Texture::Bind() const
{
    if (this->backendResource != nullptr)
    {
        this->backendResource->Bind(this->slot);
    }
}

void Texture::Unbind() const
{
    if (this->backendResource != nullptr)
    {
        this->backendResource->Unbind();
    }
}

void Texture::Destroy()
{
    if (this->backendResource != nullptr)
    {
        this->backendResource.reset();
        this->ID = 0;
        return;
    }

    this->ID = 0;
}
