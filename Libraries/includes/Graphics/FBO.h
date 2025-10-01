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
    FBO() = default;
    FBO(int width, int height);

    FBO(const FBO&) = delete;
    FBO& operator=(const FBO&) = delete;
    
    FBO(FBO&&) noexcept;
    FBO& operator=(FBO&&) noexcept;
    
    ~FBO();
    
    void Init(int width, int height);
    void Resize(int newWidth, int newHeight);
    void Destroy();
    void Bind() const;
    void Unbind() const;
    
    void BlitFBO(FBO& oFBO) const;
    void BlitToScreen(int sWidth, int sHeight) const;
    void RenderScreenQuad() const;
    void RenderScreenQuad(int fWidth, int fHeight) const;
    
    
    GLuint GetID() const { return ID; }
    Texture& GetTexture() { return TextureColor; }
    GLuint GetTextureID() const { return TextureColor.GetID(); }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    
private:
    void Swap(FBO& other) noexcept;
    void Setup();

private:
    GLuint ID = 0;
    GLuint depthBufferID = 0;
    Texture TextureColor;
    int width = 0, height = 0;

    VAO screenQuadVAO;
    Shader screenQuadShader;
};