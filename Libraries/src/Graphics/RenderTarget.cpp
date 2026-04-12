#include "RenderTarget.h"

#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Logger.h"
#include "utilities.h"

RenderTarget::RenderTarget(int width, int height) { this->Init(width, height); }

RenderTarget::RenderTarget(RenderTarget &&other) noexcept { this->Swap(other); }

RenderTarget &RenderTarget::operator=(RenderTarget &&other) noexcept
{
    if (this != &other)
    {
        this->Destroy();
        this->Swap(other);
    }
    return *this;
}

RenderTarget::~RenderTarget() { this->Destroy(); }

void RenderTarget::Swap(RenderTarget &other) noexcept
{
    std::swap(this->ID, other.ID);
    std::swap(this->width, other.width);
    std::swap(this->height, other.height);
    std::swap(this->depthBufferID, other.depthBufferID);
    std::swap(this->backendRenderTarget, other.backendRenderTarget);
    std::swap(this->screenQuadShaderProgram, other.screenQuadShaderProgram);
    std::swap(this->screenQuadGeometry, other.screenQuadGeometry);
    std::swap(this->colorTexture, other.colorTexture);
}

void RenderTarget::Destroy()
{
    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget.reset();
    }

    ID = 0;
    depthBufferID = 0;

    this->screenQuadGeometry.reset();
    this->screenQuadShaderProgram.Destroy();
    colorTexture.Destroy();
}

void RenderTarget::Init(int width, int height)
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

    colorTexture.SetFramebufferTexture("screenTexture", 0, width, height, this->ID);

    if (!backendRenderTarget->IsComplete())
    {
        LOG_ERROR(1, "RenderTarget incomplete");
        return;
    }

    this->Unbind();

    this->Setup();
}

void RenderTarget::Bind() const
{
    if (backendRenderTarget == nullptr)
    {
        return;
    }
    backendRenderTarget->Bind();
    GraphicsRenderState::SetViewport(0, 0, width, height);
}

void RenderTarget::Unbind() const
{
    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget->Unbind();
    }
}

void RenderTarget::Resize(int newWidth, int newHeight)
{
    if (width == newWidth && height == newHeight)
    {
        return;
    }

    width = newWidth;
    height = newHeight;

    colorTexture.ResizeFramebufferTexture(width, height);
    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget->Resize(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
        if (!backendRenderTarget->IsComplete())
        {
            LOG_ERROR(1, "RenderTarget incomplete after resize");
        }
    }

    Unbind();
}

void RenderTarget::CopyFromScreen(int srcWidth, int srcHeight) const
{
    if (ID == 0)
    {
        LOG_ERROR(1, "Invalid render target ID");
        return;
    }

    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget->BlitFromDefault(static_cast<std::uint32_t>(srcWidth), static_cast<std::uint32_t>(srcHeight),
                                             static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
    }

    this->Bind();
}

void RenderTarget::BlitToRenderTarget(RenderTarget &source) const
{
    const std::uint32_t sourceID = source.GetID();
    const int sourceWidth = source.GetWidth();
    const int sourceHeight = source.GetHeight();

    if (sourceID == 0 || ID == 0)
    {
        LOG_ERROR(1, "Invalid render target IDs");
        return;
    }

    if (backendRenderTarget != nullptr && source.backendRenderTarget != nullptr)
    {
        source.backendRenderTarget->BlitTo(*backendRenderTarget, static_cast<std::uint32_t>(sourceWidth),
                                           static_cast<std::uint32_t>(sourceHeight), static_cast<std::uint32_t>(width),
                                           static_cast<std::uint32_t>(height));
    }

    this->Unbind();
}

void RenderTarget::BlitToScreen(int sWidth, int sHeight) const
{
    if (ID == 0)
    {
        LOG_ERROR(1, "Invalid render target ID");
        return;
    }

    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget->BlitToDefault(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
                                           static_cast<std::uint32_t>(sWidth), static_cast<std::uint32_t>(sHeight));
    }

    this->Unbind();
}

void RenderTarget::Setup()
{
    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr && device->GetAPI() == GraphicsAPI::OpenGL)
    {
        GeometryCreateInfo createInfo{};
        createInfo.layout.vertexAttributes = {2, 2};
        createInfo.vertexData = {
            -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        };
        createInfo.indexData = {0, 1, 3, 1, 2, 3};
        createInfo.debugName = "render_target_screen_quad";
        this->screenQuadGeometry = device->CreateGeometry(createInfo);
    }

    if (this->screenQuadGeometry == nullptr)
    {
        LOG_ERROR(1, "Failed to create backend screen quad geometry for RenderTarget rendering");
        return;
    }

    this->screenQuadShaderProgram.SetShader(GET_RESOURCE_PATH("shader/upscaling/upscale.vert"),
                                            GET_RESOURCE_PATH("shader/upscaling/upscale.frag"));
}

void RenderTarget::RenderScreenQuad() const { RenderScreenQuad(width, height); }

void RenderTarget::RenderScreenQuad(int fWidth, int fHeight) const
{
    if (this->screenQuadGeometry == nullptr)
    {
        LOG_ERROR(1, "Screen quad geometry was not initialized");
        return;
    }

    GraphicsRenderState::SetViewport(0, 0, fWidth, fHeight);
    GraphicsRenderState::BindDefaultFramebuffer();
    GraphicsRenderState::SetDepthTest(false);

    colorTexture.texUnit(this->screenQuadShaderProgram);
    colorTexture.Bind();

    this->screenQuadShaderProgram.Bind();
    this->screenQuadGeometry->Bind();

    this->screenQuadGeometry->DrawIndexed();

    this->screenQuadGeometry->Unbind();
    this->screenQuadShaderProgram.Unbind();
    colorTexture.Unbind();

    GraphicsRenderState::SetDepthTest(true);
}
