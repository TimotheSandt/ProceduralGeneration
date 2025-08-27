#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "Shader.h"
#include "Texture.h"

class FBO
{
public:
    FBO();
    FBO(int width, int height);
    ~FBO();

    void Init(int width, int height);
    void Resize(int newWidth, int newHeight);
    void Destroy();
    void Bind();
    void Unbind();

    void BlitFBO(FBO& oFBO);
    void BlitToScreen(int sWidth, int sHeight);
    void RenderScreenQuad();
    void RenderScreenQuad(int fWidth, int fHeight);


    GLuint GetID() const { return ID; }
    Texture& GetTexture() { return TextureColor; }
    GLuint GetTextureID() const { return TextureColor.GetID(); }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

private:
    GLuint ID;
    GLuint depthBuffer;
    Texture TextureColor;
    int width, height;

    void Setup();
    VAO screenQuadVAO;
    Shader screenQuadShader;
};