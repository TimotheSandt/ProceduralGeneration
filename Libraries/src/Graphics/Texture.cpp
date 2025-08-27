#include "Texture.h"

Texture::Texture(const char* image, const char* name, GLuint slot, GLenum format, GLenum pixelType)
    : slot(slot), format(format), UniformName(name)
{
    int widthImg, heightImg, numColCh;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* bytes = stbi_load(image, &widthImg, &heightImg, &numColCh, 0);
    if (!bytes) {
        printf("Error loading image: %s\n", stbi_failure_reason());
        this->isLoaded = false;

        widthImg = 2;
        heightImg = 2;
        numColCh = 4;
        static unsigned char bytesDefault[] = {
            238, 130, 238, 255,   // Violet pixel
            0, 0, 0, 255,   // Black pixel
            0, 0, 0, 255,   // Black pixel
            238, 130, 238, 255    // Violet pixel
        };
        bytes = bytesDefault;
        this->format = GL_RGBA;
    }

    glGenTextures(1, &this->ID);
    this->Bind();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, widthImg, heightImg, 0, this->format, pixelType, bytes);
    glGenerateMipmap(GL_TEXTURE_2D);

    if (this->isLoaded)
        stbi_image_free(bytes);
    this->Unbind();
}


void Texture::texUnit(Shader &shader)
{
    shader.Bind();
    glUniform1i(glGetUniformLocation(shader.GetID(), this->UniformName), this->slot);
}

void Texture::Bind()
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
    if (this->ID != 0) glDeleteTextures(1, &this->ID);
    this->ID = 0;
}