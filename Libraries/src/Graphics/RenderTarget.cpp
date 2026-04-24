#include "RenderTarget.h"

#include "Graphics/Backends/OpenGL/OpenGLGraphicsResources.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Logger.h"
#include "utilities.h"

RenderTarget::RenderTarget(int width, int height) { this->Init(width, height); }

RenderTarget::RenderTarget(const RenderTargetDesc &desc) { this->Init(desc); }

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
    std::swap(this->colorTextures, other.colorTextures);
    std::swap(this->depthTexture, other.depthTexture);
    std::swap(this->hasDepthTexture, other.hasDepthTexture);
    std::swap(this->currentDesc, other.currentDesc);
}

void RenderTarget::Destroy()
{
    if (backendRenderTarget != nullptr)
    {
        backendRenderTarget.reset();
    }

    ID = 0;
    depthBufferID = 0;
    hasDepthTexture = false;

    this->screenQuadGeometry.reset();
    this->screenQuadShaderProgram.Destroy();
    colorTextures.clear();
    depthTexture.Destroy();
    width = 0;
    height = 0;
    currentDesc = {};
}

void RenderTarget::Init(int width, int height)
{
    RenderTargetDesc desc;
    desc.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
    desc.colorAttachments = {TextureFormat::RGBA8};
    desc.hasDepthBuffer = true;
    desc.depthAsTexture = false;
    this->Init(desc);
}

bool RenderTarget::AttachmentsMatch(const RenderTargetDesc &a, const RenderTargetDesc &b) noexcept
{
    return a.colorAttachments == b.colorAttachments
        && a.hasDepthBuffer == b.hasDepthBuffer
        && a.depthAsTexture == b.depthAsTexture;
}

void RenderTarget::ResizeOrReconfigure(const RenderTargetDesc &desc)
{
    if (IsInitialized() && AttachmentsMatch(currentDesc, desc))
    {
        Resize(static_cast<int>(desc.extent.width), static_cast<int>(desc.extent.height));
    }
    else
    {
        Init(desc);
    }
}

void RenderTarget::Init(const RenderTargetDesc &desc)
{
    this->width = static_cast<int>(desc.extent.width);
    this->height = static_cast<int>(desc.extent.height);

    if (const IGraphicsDevice *device = TryGetActiveGraphicsDevice(); device != nullptr && device->GetAPI() == GraphicsAPI::OpenGL)
    {
        std::unique_ptr<IRenderTargetResource> renderTarget =
            device->CreateRenderTarget({.desc = desc, .debugName = "offscreen_render_target"});

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
    this->InitAttachments(desc);

    if (!backendRenderTarget->IsComplete())
    {
        LOG_ERROR(1, "RenderTarget incomplete");
        return;
    }

    currentDesc = desc;
    this->Unbind();
    this->Setup();
}

void RenderTarget::InitAttachments(const RenderTargetDesc &desc)
{
    const auto &formats = desc.colorAttachments;
    colorTextures.resize(formats.size());

    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(formats.size()); ++i)
    {
        // Slot i+1: reserve slot 0 for the depth texture when it exists, but
        // since depth isn't sampled via a numbered slot in most shaders, just use i directly.
        colorTextures[i].SetFramebufferTexture(
            ("attachment" + std::to_string(i)).c_str(),
            /*slot=*/i,
            width, height,
            this->ID,
            /*colorIndex=*/i,
            formats[i]);
    }

    // For single-attachment RTs created via Init(w,h), keep the old "screenTexture" name
    // so existing post-process / upscale shaders that use `screenTexture` still work.
    if (formats.size() == 1 && formats[0] == TextureFormat::RGBA8)
    {
        colorTextures[0].SetFramebufferTexture("screenTexture", /*slot=*/0, width, height,
                                               this->ID, /*colorIndex=*/0, TextureFormat::RGBA8);
    }

    if (desc.hasDepthBuffer && desc.depthAsTexture)
    {
        depthTexture.SetFramebufferTexture("depthTexture", /*slot=*/static_cast<std::uint32_t>(formats.size()),
                                           width, height, this->ID,
                                           /*colorIndex=*/0, TextureFormat::Depth32Float);
        hasDepthTexture = true;
    }

    // Tell the GPU which draw targets are active.
    backendRenderTarget->SetDrawBuffers(static_cast<std::uint32_t>(formats.size()));
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
    if (!IsInitialized())
    {
        Init(newWidth, newHeight);
        return;
    }

    if (width == newWidth && height == newHeight)
    {
        return;
    }

    width = newWidth;
    height = newHeight;

    for (auto &tex : colorTextures)
    {
        tex.ResizeFramebufferTexture(width, height);
    }

    if (hasDepthTexture)
    {
        depthTexture.ResizeFramebufferTexture(width, height);
    }

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

void RenderTarget::BlitToRenderTarget(RenderTarget &destination) const
{
    if (ID == 0 || destination.ID == 0)
    {
        LOG_ERROR(1, "Invalid render target IDs");
        return;
    }

    if (backendRenderTarget != nullptr && destination.backendRenderTarget != nullptr)
    {
        backendRenderTarget->BlitTo(*destination.backendRenderTarget,
                                    static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
                                    static_cast<std::uint32_t>(destination.width), static_cast<std::uint32_t>(destination.height));
    }

    destination.Unbind();
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

std::uint32_t RenderTarget::ReadPixelUInt(std::uint32_t attachmentIndex, int x, int y) const
{
    if (backendRenderTarget == nullptr)
    {
        return 0;
    }
    return backendRenderTarget->ReadPixelUInt(attachmentIndex, x, y, height);
}

glm::uvec4 RenderTarget::ReadPixelRGBA8(std::uint32_t attachmentIndex, int x, int y) const
{
    if (backendRenderTarget == nullptr)
    {
        return glm::uvec4(0);
    }
    return backendRenderTarget->ReadPixelRGBA8(attachmentIndex, x, y, height);
}

void RenderTarget::RenderScreenQuad() const { RenderScreenQuad(width, height); }

void RenderTarget::RenderScreenQuad(int fWidth, int fHeight) const
{
    if (this->screenQuadGeometry == nullptr || colorTextures.empty())
    {
        LOG_ERROR(1, "Screen quad geometry was not initialized");
        return;
    }

    const Texture &primary = colorTextures[0];

    GraphicsRenderState::SetViewport(0, 0, fWidth, fHeight);
    GraphicsRenderState::BindDefaultFramebuffer();
    GraphicsRenderState::SetDepthTest(false);

    primary.texUnit(this->screenQuadShaderProgram);
    primary.Bind();

    this->screenQuadShaderProgram.Bind();
    this->screenQuadGeometry->Bind();

    this->screenQuadGeometry->DrawIndexed();

    this->screenQuadGeometry->Unbind();
    this->screenQuadShaderProgram.Unbind();
    primary.Unbind();

    GraphicsRenderState::SetDepthTest(true);
}
