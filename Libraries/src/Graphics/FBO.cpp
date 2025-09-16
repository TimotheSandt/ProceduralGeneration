#include "FBO.h"

#include "Logger.h"

FBO::FBO() : width(0), height(0) {}
FBO::FBO(int width, int height)
{
    this->Init(width, height);
}

FBO::~FBO() {
    this->Destroy();
}

void FBO::Destroy() {
    if (ID != 0) glDeleteFramebuffers(1, &ID);
    if (depthBuffer != 0) glDeleteRenderbuffers(1, &depthBuffer);
    
    ID = 0;
    depthBuffer = 0;
    
    this->screenQuadShader.Destroy();
    this->screenQuadVAO.Destroy();
    TextureColor.Destroy();
}

void FBO::Init(int width, int height) {
    this->width = width;
    this->height = height;

    glGenFramebuffers(1, &ID);
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    
    TextureColor.SetFramebufferTexture("screenTexture", 0, width, height, this->ID);
    
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR(status, "FBO incomplete");
        return;
    }
    
    this->Unbind();
    
    this->Setup();
}

void FBO::Bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    glViewport(0, 0, width, height);
}

void FBO::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FBO::Resize(int newWidth, int newHeight) {
    if (width == newWidth && height == newHeight) {
        return;
    }
    
    // Store the new dimensions
    width = newWidth;
    height = newHeight;
    
    // Resize the color texture
    TextureColor.ResizeFramebufferTexture(width, height);
    
    // Resize the depth buffer
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    
    // Verify the framebuffer is still complete
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR(status, "FBO incomplete after resize: ");
    }
    
    Unbind();
}

void FBO::BlitFBO(FBO& oFBO) {

    GLuint oID = oFBO.GetID();
    int oWidth = oFBO.GetWidth();
    int oHeight = oFBO.GetHeight();

    if (oID == 0 || ID == 0) {
        LOG_ERROR(1, "Invalid FBO IDs");
        return;
    }
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, oID);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ID);
    
    glBlitFramebuffer(0, 0, oWidth, oHeight, 0, 0, width, height, 
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBlitFramebuffer(0, 0, oWidth, oHeight, 0, 0, width, height, 
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
#ifdef DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR(error, "glBlitFramebuffer error");
    }
#endif
    
    this->Unbind();
}

void FBO::BlitToScreen(int sWidth, int sHeight) {
    if (ID == 0) {
        LOG_ERROR(1, "Invalid FBO ID");
        return;
    }
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, ID);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    
    glBlitFramebuffer(0, 0, width, height, 0, 0, sWidth, sHeight, 
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBlitFramebuffer(0, 0, width, height, 0, 0, sWidth, sHeight, 
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
#ifdef DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR(error, "glBlitFramebuffer to screen error");
    }
#endif
    
    this->Unbind();
}


void FBO::Setup() {
    std::vector<GLfloat> vertices = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    std::vector<GLuint> indices = {
        0, 1, 3,
        1, 2, 3
    };

    this->screenQuadShader.SetShader("res/shader/upscaling/upscale.vert", "res/shader/upscaling/upscale.frag");

    this->screenQuadVAO.Initialize();
    if (glGetError() != GL_NO_ERROR) {
        LOG_ERROR(1, "VAO initialization failed");
    }
    this->screenQuadVAO.Generate();
    this->screenQuadVAO.Bind();

    VBO bVBO(vertices);
    EBO EBO(indices);

    this->screenQuadVAO.LinkAttrib(bVBO, 0, 2, GL_FLOAT, 4 * sizeof(GLfloat), 0);
    this->screenQuadVAO.LinkAttrib(bVBO, 1, 2, GL_FLOAT, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    if (glGetError() != GL_NO_ERROR) { 
        LOG_ERROR(1, "VAO linking failed");
    }

    this->screenQuadVAO.Unbind();
    bVBO.Unbind();
    EBO.Unbind();
}


void FBO::RenderScreenQuad() {
    RenderScreenQuad(width, height);
}

void FBO::RenderScreenQuad(int fWidth, int fHeight) {
    glViewport(0, 0, fWidth, fHeight);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);

    TextureColor.texUnit(this->screenQuadShader);
    TextureColor.Bind();

    this->screenQuadShader.Bind();
    this->screenQuadVAO.Bind();

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    this->screenQuadVAO.Unbind();
    this->screenQuadShader.Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);
    

    glEnable(GL_DEPTH_TEST);
}