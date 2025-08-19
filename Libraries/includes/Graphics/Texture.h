#pragma once

#include <glad/glad.h>
#include <stb/stb_image.h>

#include "Shader.h"

class Texture
{
public:
    Texture(const char* image, const char* name, GLuint slot, GLenum format, GLenum pixelType);

    void texUnit(Shader &shader);
    void Bind();
    void Unbind();
    void Destroy();
    GLuint GetID() { return this->ID; }

private:
    GLuint ID;
    GLuint slot;
    GLenum format;

    const char* UniformName;

    bool isLoaded = false;
};