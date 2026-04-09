#pragma once

#include <stb_image.h>

#include "Graphics/Core/GraphicsResources.h"
#include "ShaderProgram.h"

#include <cstdint>

enum class TexturePixelType : std::uint8_t
{
    UnsignedByte = 0,
    Byte,
    UnsignedShort,
    Short,
    UnsignedInt,
    Int,
    HalfFloat,
    Float,
    Double
};

enum class TextureFilterMode : std::uint8_t
{
    Linear = 0,
    Nearest
};

class Texture
{
  public:
    Texture();
    Texture(void *data, int width, int height, const char *name, std::uint32_t slot, TextureFormat format = TextureFormat::RGBA8,
            TexturePixelType pixelType = TexturePixelType::UnsignedByte, TextureFilterMode filter = TextureFilterMode::Linear);
    Texture(const std::string &image, const char *name, std::uint32_t slot, TextureFormat format = TextureFormat::RGBA8,
            TexturePixelType pixelType = TexturePixelType::UnsignedByte, TextureFilterMode filter = TextureFilterMode::Linear);
    ~Texture();

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

    Texture(Texture &&) noexcept;
    Texture &operator=(Texture &&) noexcept;

    void Copy(const Texture &texture);
    Texture Copy() const;

    void SetTextureData(void *data, int width, int height, TextureFormat format = TextureFormat::RGBA8,
                        TexturePixelType pixelType = TexturePixelType::UnsignedByte, TextureFilterMode filter = TextureFilterMode::Linear);
    void *GetTextureData(int &width, int &height, TextureFormat &format, TexturePixelType &pixelType) const;

    void SetFramebufferTexture(const char *uniformName, std::uint32_t slot, int width, int height, std::uint32_t renderTargetHandle);
    void ResizeFramebufferTexture(int width, int height);

    void texUnit(const ShaderProgram &shaderProgram) const;
    void Bind() const;
    void Unbind() const;
    void Destroy();

    std::uint32_t GetID() const { return this->ID; }
    std::uint32_t GetSlot() const { return this->slot; }
    TextureFormat GetFormat() const { return this->format; }
    TexturePixelType GetPixelType() const { return this->pixelType; }
    int GetWidth() const { return this->Width; }
    int GetHeight() const { return this->Height; }
    const char *GetUniformName() const { return this->UniformName; }

    size_t GetDataSize() const;

  private:
    void Swap(Texture &other) noexcept;
    size_t GetPixelTypeSize(TexturePixelType pixelType) const;
    size_t GetComponentCount(TextureFormat format) const;

  private:
    std::uint32_t ID = 0;
    std::unique_ptr<ITextureResource> backendResource;
    std::uint32_t slot;
    TextureFormat format;
    TexturePixelType pixelType;

    int Width, Height;

    const char *UniformName;
};
