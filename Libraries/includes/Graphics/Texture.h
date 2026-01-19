#pragma once

#include <glad/glad.h>

#include <stb_image.h>

#include "Shader.h"

class Texture
{
public:
    Texture();
    Texture(void* data, int width, int height, const char* name, GLuint slot, GLenum format = GL_RGBA, GLenum pixelType = GL_UNSIGNED_BYTE, GLenum filter = GL_LINEAR);
    Texture(std::string image, const char* name, GLuint slot, GLenum format = GL_RGBA, GLenum pixelType = GL_UNSIGNED_BYTE, GLenum filter = GL_LINEAR);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;

    void Copy(Texture& texture);
    Texture Copy() const;

    void SetTextureData(void* data, int width, int height, GLenum format = GL_RGBA, GLenum pixelType = GL_UNSIGNED_BYTE, GLenum filter = GL_LINEAR);
    void* GetTextureData(int& width, int& height, GLenum& format, GLenum& pixelType) const;

    void SetFramebufferTexture(const char* uniformName, GLuint slot, int width, int height, GLuint FBO);
    void ResizeFramebufferTexture(int width, int height);

    void texUnit(const Shader &shader) const;
    void Bind() const;
    void Unbind() const;
    void Destroy();

    GLuint GetID() const { return this->ID; }
    GLuint GetSlot() const { return this->slot; }
    GLenum GetFormat() const { return this->format; }
    GLenum GetPixelType() const { return this->pixelType; }
    int GetWidth() const { return this->Width; }
    int GetHeight() const { return this->Height; }
    const char* GetUniformName() const { return this->UniformName; }

    size_t GetDataSize() const;

private:
    void Swap(Texture& other) noexcept;
    size_t GetPixelTypeSize(GLenum pixelType) const;
    size_t GetComponentCount(GLenum format) const;

private:
    GLuint ID = 0;
    GLuint slot;
    GLenum format;
    GLenum pixelType;

    int Width, Height;

    const char* UniformName;
};
