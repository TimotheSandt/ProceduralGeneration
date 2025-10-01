#include "Texture.h"

Texture::Texture() 
    : ID(0), slot(0), format(GL_RGBA), pixelType(GL_UNSIGNED_BYTE), Width(0), Height(0), UniformName("")
{
    // Default constructor - creates empty texture object
}

Texture::Texture(Texture&& other) noexcept {
    this->Swap(other);
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

void Texture::Swap(Texture& other) noexcept {
    std::swap(this->ID, other.ID);
    std::swap(this->slot, other.slot);
    std::swap(this->format, other.format);
    std::swap(this->pixelType, other.pixelType);
    std::swap(this->Width, other.Width);
    std::swap(this->Height, other.Height);
    std::swap(this->UniformName, other.UniformName);
}

void Texture::Copy(Texture& texture)
{
    void* data = texture.GetTextureData(this->Width, this->Height, this->format, this->pixelType);
    this->SetTextureData(data, this->Width, this->Height, this->format, this->pixelType);
}

Texture Texture::Copy() const
{
    int w, h;
    GLenum f, p;
    void* data = this->GetTextureData(w, h, f, p);
    Texture texture(data, w, h, this->UniformName, this->slot, f, p);
    return texture;
}

Texture::Texture(std::string image, const char* name, GLuint slot, GLenum format, GLenum pixelType, GLenum filter) 
    : slot(slot), format(format), pixelType(pixelType), Width(0), Height(0), UniformName(name)
{
    stbi_set_flip_vertically_on_load(true);
    int numColCh;
    bool isLoaded = true;
    unsigned char* bytes = stbi_load(image.c_str(), &this->Width, &this->Height, &numColCh, 0);
    if (!bytes) {
        LOG_ERROR(1, "Failed to load image: ", stbi_failure_reason());
        isLoaded = false;

        this->Width = 2;
        this->Height = 2;
        numColCh = 4;
        static unsigned char bytesDefault[] = {
            238, 130, 238, 255,   // Violet pixel
            0, 0, 0, 255,   // Black pixel
            0, 0, 0, 255,   // Black pixel
            238, 130, 238, 255    // Violet pixel
        };
        bytes = bytesDefault;
        this->format = GL_RGBA;
        this->pixelType = GL_UNSIGNED_BYTE;
    }

    this->SetTextureData(bytes, this->Width, this->Height, this->format, pixelType, filter);

    if (isLoaded)
        stbi_image_free(bytes);
}

Texture::Texture(void* data, int width, int height, const char* name, GLuint slot, GLenum format, GLenum pixelType, GLenum filter) 
    : slot(slot), format(format), pixelType(pixelType), Width(width), Height(height), UniformName(name)
{
    this->SetTextureData(data, width, height, this->format, pixelType, filter);
}



Texture::~Texture() {
    this->Destroy();
}


void Texture::SetTextureData(void* data, int width, int height, GLenum format, GLenum pixelType, GLenum filter) {
    this->Width = width;
    this->Height = height;
    this->format = format;
    this->pixelType = pixelType;

    glGenTextures(1, &this->ID);
    this->Bind();
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, this->format, this->Width, this->Height, 0, this->format, pixelType, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    this->Unbind();
}

void* Texture::GetTextureData(int& width, int& height, GLenum& format, GLenum& pixelType) const {
    width = this->Width;
    height = this->Height;
    format = this->format;
    pixelType = this->pixelType;

    this->Bind();

    size_t dataSize = GetDataSize();
    void *data = malloc(dataSize);

    glReadPixels(0, 0, this->Width, this->Height, this->format, this->pixelType, data);
    
    return data;
}

size_t Texture::GetPixelTypeSize(GLenum pixelType) const {
    switch(pixelType) {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
            return 1;
        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
        case GL_HALF_FLOAT:
            return 2;
        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
            return 4;
        case GL_DOUBLE:
            return 8;
        default:
            return 1;
    }
}

size_t Texture::GetComponentCount(GLenum format) const {
    switch(format) {
        case GL_RED:
        case GL_DEPTH_COMPONENT:
            return 1;
        case GL_RG:
            return 2;
        case GL_RGB:
        case GL_BGR:
            return 3;
        case GL_RGBA:
        case GL_BGRA:
            return 4;
        default:
            return 4;
    }
}

size_t Texture::GetDataSize() const {
    return Width * Height * GetComponentCount(format) * GetPixelTypeSize(pixelType);
}

void Texture::SetFramebufferTexture(const char* uniformName, GLuint slot, int width, int height, GLuint FBO) {
    this->slot = slot;
    this->UniformName = uniformName;
    this->Width = width;
    this->Height = height;
    this->format = GL_RGBA;
    this->pixelType = GL_UNSIGNED_BYTE;

    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    glGenTextures(1, &this->ID);
    glBindTexture(GL_TEXTURE_2D, this->ID);
    glTexImage2D(GL_TEXTURE_2D, 0, this->format, this->Width, this->Height, 0, this->format, this->pixelType, NULL);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->ID, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Texture::ResizeFramebufferTexture(int width, int height) {
    this->Width = width;
    this->Height = height;
    glBindTexture(GL_TEXTURE_2D, this->ID);
    glTexImage2D(GL_TEXTURE_2D, 0, this->format, this->Width, this->Height, 0, this->format, GL_UNSIGNED_BYTE, NULL);
}

void Texture::texUnit(Shader &shader)
{
    shader.Bind();
    glUniform1i(glGetUniformLocation(shader.GetID(), this->UniformName), this->slot);
}

void Texture::Bind() const
{
    glActiveTexture(GL_TEXTURE0 + this->slot);
    glBindTexture(GL_TEXTURE_2D, this->ID);
}

void Texture::Unbind()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Destroy()
{
    if (this->ID == 0) return;
    glDeleteTextures(1, &this->ID);
    this->ID = 0;
}