#include "FBO.h"

#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"
#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
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
    std::swap(this->screenQuadGeometry, other.screenQuadGeometry);
    std::swap(this->TextureColor, other.TextureColor);
}

void FBO::Destroy()
{
    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget.reset();
    }

    ID = 0;
    depthBufferID = 0;

    this->screenQuadGeometry.reset();
    this->screenQuadShader.Destroy();
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

    if (backendRenderTarget == nullptr)
    {
        LOG_ERROR(1, "OpenGL render target backend resource is required");
        return;
    }

    backendRenderTarget->Bind();

    TextureColor.SetFramebufferTexture("screenTexture", 0, width, height, this->ID);

    if (!backendRenderTarget->IsComplete())
    {
        LOG_ERROR(1, "FBO incomplete");
        return;
    }

    this->Unbind();

    this->Setup();
}

void FBO::Bind() const
{
    if (backendRenderTarget == nullptr)
    {
        return;
    }
    backendRenderTarget->Bind();
    OpenGLRenderState::SetViewport(0, 0, width, height);
}

void FBO::Unbind() const
{
    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget->Unbind();
    }
}

void FBO::Resize(int newWidth, int newHeight)
{
    if (width == newWidth && height == newHeight)
    {
        return;
    }

    width = newWidth;
    height = newHeight;

    TextureColor.ResizeFramebufferTexture(width, height);
    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget->Resize(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
        if (!backendRenderTarget->IsComplete())
        {
            LOG_ERROR(1, "FBO incomplete after resize");
        }
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

    if (backendRenderTarget != nullptr && oFBO.backendRenderTarget != nullptr)
    {
        oFBO.backendRenderTarget->BlitTo(*backendRenderTarget, static_cast<std::uint32_t>(oWidth), static_cast<std::uint32_t>(oHeight),
                                         static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
    }

    this->Unbind();
}

void FBO::BlitToScreen(int sWidth, int sHeight) const
{
    if (ID == 0)
    {
        LOG_ERROR(1, "Invalid FBO ID");
        return;
    }

    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget->BlitToDefault(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
                                           static_cast<std::uint32_t>(sWidth), static_cast<std::uint32_t>(sHeight));
    }

    this->Unbind();
}

void FBO::Setup()
{
    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr && device->GetAPI() == GraphicsAPI::OpenGL)
    {
        GeometryCreateInfo createInfo{};
        createInfo.layout.vertexAttributes = {2, 2};
        createInfo.vertexData = {
            -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        };
        createInfo.indexData = {0, 1, 3, 1, 2, 3};
        createInfo.debugName = "fbo_screen_quad";
        this->screenQuadGeometry = device->CreateGeometry(createInfo);
    }

    if (this->screenQuadGeometry == nullptr)
    {
        LOG_ERROR(1, "Failed to create backend screen quad geometry for FBO rendering");
        return;
    }

    this->screenQuadShader.SetShader(GET_RESOURCE_PATH("shader/upscaling/upscale.vert"),
                                     GET_RESOURCE_PATH("shader/upscaling/upscale.frag"));
}

void FBO::RenderScreenQuad() const { RenderScreenQuad(width, height); }

void FBO::RenderScreenQuad(int fWidth, int fHeight) const
{
    if (this->screenQuadGeometry == nullptr)
    {
        LOG_ERROR(1, "Screen quad geometry was not initialized");
        return;
    }

    OpenGLRenderState::SetViewport(0, 0, fWidth, fHeight);

    OpenGLRenderState::BindFramebuffer(GL_FRAMEBUFFER, 0);
    OpenGLRenderState::SetDepthTest(false);

    TextureColor.texUnit(this->screenQuadShader);
    TextureColor.Bind();

    this->screenQuadShader.Bind();
    this->screenQuadGeometry->Bind();

    this->screenQuadGeometry->DrawIndexed();

    this->screenQuadGeometry->Unbind();
    this->screenQuadShader.Unbind();
    TextureColor.Unbind();

    OpenGLRenderState::SetDepthTest(true);
}
