#include "FBO.h"

#include <bit>

#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Logger.h"
#include "utilities.h"

FBO::FBO(int width, int height) { this->Init(width, height); }

FBO::FBO(FBO &&other) noexcept { this->Swap(other); }

FBO &FBO::operator=(FBO &&other) noexcept
{
    if (this != &other)
    {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

FBO::~FBO() { this->Destroy(); }

void FBO::Swap(FBO &other) noexcept
{
    std::swap(this->ID, other.ID);
    std::swap(this->width, other.width);
    std::swap(this->height, other.height);
    std::swap(this->depthBufferID, other.depthBufferID);
    std::swap(this->backendRenderTarget, other.backendRenderTarget);
    std::swap(this->screenQuadShader, other.screenQuadShader);
    std::swap(this->screenQuadVAO, other.screenQuadVAO);
    std::swap(this->TextureColor, other.TextureColor);
}

void FBO::Destroy()
{
    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget.reset();
    }
    else
    {
        if (ID != 0)
        {
            glDeleteFramebuffers(1, &ID);
        }
        if (depthBufferID != 0)
        {
            glDeleteRenderbuffers(1, &depthBufferID);
        }
    }

    ID = 0;
    depthBufferID = 0;

    this->screenQuadShader.Destroy();
    this->screenQuadVAO.Destroy();
    TextureColor.Destroy();
}

void FBO::Init(int width, int height)
{
    this->width = width;
    this->height = height;

    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr && device->GetAPI() == GraphicsAPI::OpenGL)
    {
        std::unique_ptr<IRenderTargetResource> renderTarget =
            device->CreateRenderTarget({.desc = {.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)},
                                                 .colorFormat = TextureFormat::RGBA8,
                                                 .hasDepthBuffer = true},
                                        .debugName = "offscreen_render_target"});
        if (auto *openGLRenderTarget = dynamic_cast<OpenGLRenderTargetResource *>(renderTarget.get()); openGLRenderTarget != nullptr)
        {
            this->ID = openGLRenderTarget->GetFramebufferID();
            this->depthBufferID = openGLRenderTarget->GetDepthBufferID();
            this->backendRenderTarget = std::move(renderTarget);
        }
    }

    if (ID == 0)
    {
        glGenFramebuffers(1, &ID);
        GL_CHECK_ERROR_M("FBO gen");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO bind init");

    TextureColor.SetFramebufferTexture("screenTexture", 0, width, height, this->ID);

    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO rebind init");
    if (depthBufferID == 0)
    {
        glGenRenderbuffers(1, &depthBufferID);
        GL_CHECK_ERROR_M("FBO depth gen");
        glBindRenderbuffer(GL_RENDERBUFFER, depthBufferID);
        GL_CHECK_ERROR_M("FBO depth bind");
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        GL_CHECK_ERROR_M("FBO depth storage");
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBufferID);
        GL_CHECK_ERROR_M("FBO depth attach");
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GL_CHECK_ERROR_M("FBO status check");
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR(status, "FBO incomplete");
        return;
    }

    this->Unbind();

    this->Setup();
}

void FBO::Bind() const
{
    if (ID == 0)
    {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO bind");
    OpenGLRenderState::SetViewport(0, 0, width, height);
}

void FBO::Unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GL_CHECK_ERROR_M("FBO unbind");
}

void FBO::Resize(int newWidth, int newHeight)
{
    if (width == newWidth && height == newHeight)
    {
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
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR(status, "FBO incomplete after resize: ");
    }

    Unbind();
}

void FBO::BlitFBO(FBO &oFBO) const
{
    GLuint oID = oFBO.GetID();
    int oWidth = oFBO.GetWidth();
    int oHeight = oFBO.GetHeight();

    if (oID == 0 || ID == 0)
    {
        LOG_ERROR(1, "Invalid FBO IDs");
        return;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, oID);
    GL_CHECK_ERROR_M("FBO blit read bind");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO blit draw bind");

    glBlitFramebuffer(0, 0, oWidth, oHeight, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GL_CHECK_ERROR_M("FBO blit color");
    glBlitFramebuffer(0, 0, oWidth, oHeight, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    GL_CHECK_ERROR_M("FBO blit depth");

    this->Unbind();
}

void FBO::BlitToScreen(int sWidth, int sHeight) const
{
    if (ID == 0)
    {
        LOG_ERROR(1, "Invalid FBO ID");
        return;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, ID);
    GL_CHECK_ERROR_M("FBO screen blit read bind");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    GL_CHECK_ERROR_M("FBO screen blit draw bind");

    glBlitFramebuffer(0, 0, width, height, 0, 0, sWidth, sHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GL_CHECK_ERROR_M("FBO screen blit color");
    glBlitFramebuffer(0, 0, width, height, 0, 0, sWidth, sHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    GL_CHECK_ERROR_M("FBO screen blit depth");

    this->Unbind();
}

void FBO::Setup()
{
    std::vector<GLfloat> vertices = {-1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<GLuint> indices = {0, 1, 3, 1, 2, 3};

    this->screenQuadShader.SetShader(GET_RESOURCE_PATH("shader/upscaling/upscale.vert"),
                                     GET_RESOURCE_PATH("shader/upscaling/upscale.frag"));

    this->screenQuadVAO.Initialize();
    GL_CHECK_ERROR_M("FBO screen VAO init");
    this->screenQuadVAO.Initialize();
    GL_CHECK_ERROR_M("FBO screen VAO gen");
    this->screenQuadVAO.Bind();
    GL_CHECK_ERROR_M("FBO screen VAO bind");

    VBO bVBO(vertices);
    EBO bEBO(indices);

    this->screenQuadVAO.LinkAttrib(bVBO, 0, 2, GL_FLOAT, 4 * sizeof(GLfloat), nullptr);
    this->screenQuadVAO.LinkAttrib(bVBO, 1, 2, GL_FLOAT, 4 * sizeof(GLfloat), std::bit_cast<void *>(std::uintptr_t(2 * sizeof(GLfloat))));

    GL_CHECK_ERROR_M("FBO screen VAO link");

    this->screenQuadVAO.Unbind();
    bVBO.Unbind();
    bEBO.Unbind();
}

void FBO::RenderScreenQuad() const { RenderScreenQuad(width, height); }

void FBO::RenderScreenQuad(int fWidth, int fHeight) const
{
    OpenGLRenderState::SetViewport(0, 0, fWidth, fHeight);

    OpenGLRenderState::BindFramebuffer(GL_FRAMEBUFFER, 0);
    OpenGLRenderState::SetDepthTest(false);

    TextureColor.texUnit(this->screenQuadShader);
    TextureColor.Bind();

    this->screenQuadShader.Bind();
    this->screenQuadVAO.Bind();

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    GL_CHECK_ERROR_M("FBO screen draw");

    this->screenQuadVAO.Unbind();
    this->screenQuadShader.Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_CHECK_ERROR_M("FBO screen tex unbind");

    OpenGLRenderState::SetDepthTest(true);
}
