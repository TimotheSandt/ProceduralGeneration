#include "FBO.h"

#include "Logger.h"
#include "utilities.h"

FBO::FBO() : width(0), height(0) {}
FBO::FBO(int width, int height)
{
    this->Init(width, height);
}

FBO::FBO(FBO&& other) noexcept {
    this->Swap(other);
}

FBO& FBO::operator=(FBO&& other) noexcept {
    if (this != &other) {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

FBO::~FBO() {
    this->Destroy();
}

void FBO::Swap(FBO& other) noexcept {
    std::swap(this->ID, other.ID);
    std::swap(this->width, other.width);
    std::swap(this->height, other.height);
    std::swap(this->depthBufferID, other.depthBufferID);
    std::swap(this->screenQuadShader, other.screenQuadShader);
    std::swap(this->screenQuadVAO, other.screenQuadVAO);
    std::swap(this->TextureColor, other.TextureColor);
}

void FBO::Destroy() {
    if (ID != 0) glDeleteFramebuffers(1, &ID);
    if (depthBufferID != 0) glDeleteRenderbuffers(1, &depthBufferID);
    
    ID = 0;
    depthBufferID = 0;
    
    this->screenQuadShader.Destroy();
    this->screenQuadVAO.Destroy();
    TextureColor.Destroy();
}

void FBO::Init(int width, int height) {
    this->width = width;
    this->height = height;

    glGenFramebuffers(1, &ID);
    GL_CHECK_ERROR_M("FBO gen");
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO bind init");
    
    TextureColor.SetFramebufferTexture("screenTexture", 0, width, height, this->ID);
    
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO rebind init");
    glGenRenderbuffers(1, &depthBufferID);
    GL_CHECK_ERROR_M("FBO depth gen");
    glBindRenderbuffer(GL_RENDERBUFFER, depthBufferID);
    GL_CHECK_ERROR_M("FBO depth bind");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    GL_CHECK_ERROR_M("FBO depth storage");
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBufferID);
    GL_CHECK_ERROR_M("FBO depth attach");

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GL_CHECK_ERROR_M("FBO status check");
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR(status, "FBO incomplete");
        return;
    }
    
    this->Unbind();
    
    this->Setup();
}

void FBO::Bind() {
    if (ID == 0) return;
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO bind");
    glViewport(0, 0, width, height);
    GL_CHECK_ERROR_M("FBO viewport");
}

void FBO::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GL_CHECK_ERROR_M("FBO unbind");
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
    glBindRenderbuffer(GL_RENDERBUFFER, depthBufferID);
    GL_CHECK_ERROR_M("FBO resize depth bind");
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    GL_CHECK_ERROR_M("FBO resize depth storage");
    
    // Verify the framebuffer is still complete
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO resize bind");
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GL_CHECK_ERROR_M("FBO resize status check");
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
    GL_CHECK_ERROR_M("FBO blit read bind");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO blit draw bind");
    
    glBlitFramebuffer(0, 0, oWidth, oHeight, 0, 0, width, height,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GL_CHECK_ERROR_M("FBO blit color");
    glBlitFramebuffer(0, 0, oWidth, oHeight, 0, 0, width, height,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    GL_CHECK_ERROR_M("FBO blit depth");
    
    this->Unbind();
}

void FBO::BlitToScreen(int sWidth, int sHeight) {
    if (ID == 0) {
        LOG_ERROR(1, "Invalid FBO ID");
        return;
    }
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO screen blit read bind");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    GL_CHECK_ERROR_M("FBO screen blit draw bind");
    
    glBlitFramebuffer(0, 0, width, height, 0, 0, sWidth, sHeight,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GL_CHECK_ERROR_M("FBO screen blit color");
    glBlitFramebuffer(0, 0, width, height, 0, 0, sWidth, sHeight,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    GL_CHECK_ERROR_M("FBO screen blit depth");
    
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


    this->screenQuadShader.SetShader(GET_RESOURCE_PATH("shader/upscaling/upscale.vert"), GET_RESOURCE_PATH("shader/upscaling/upscale.frag"));


    this->screenQuadVAO.Initialize();
    GL_CHECK_ERROR_M("FBO screen VAO init");
    this->screenQuadVAO.Generate();
    GL_CHECK_ERROR_M("FBO screen VAO gen");
    this->screenQuadVAO.Bind();
    GL_CHECK_ERROR_M("FBO screen VAO bind");

    VBO bVBO(vertices);
    EBO bEBO(indices);

    this->screenQuadVAO.LinkAttrib(bVBO, 0, 2, GL_FLOAT, 4 * sizeof(GLfloat), 0);
    this->screenQuadVAO.LinkAttrib(bVBO, 1, 2, GL_FLOAT, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    GL_CHECK_ERROR_M("FBO screen VAO link");

    this->screenQuadVAO.Unbind();
    bVBO.Unbind();
    bEBO.Unbind();
}


void FBO::RenderScreenQuad() {
    RenderScreenQuad(width, height);
}

void FBO::RenderScreenQuad(int fWidth, int fHeight) {
    glViewport(0, 0, fWidth, fHeight);
    GL_CHECK_ERROR_M("FBO screen viewport");
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GL_CHECK_ERROR_M("FBO screen fbo unbind");
    glDisable(GL_DEPTH_TEST);
    GL_CHECK_ERROR_M("FBO screen depth disable");

    TextureColor.texUnit(this->screenQuadShader);
    TextureColor.Bind();

    this->screenQuadShader.Bind();
    this->screenQuadVAO.Bind();

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    GL_CHECK_ERROR_M("FBO screen draw");

    this->screenQuadVAO.Unbind();
    this->screenQuadShader.Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_CHECK_ERROR_M("FBO screen tex unbind");
    
    

    glEnable(GL_DEPTH_TEST);
    GL_CHECK_ERROR_M("FBO screen depth enable");
}