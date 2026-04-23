#include "Core/Container.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GraphicsDiagnostics.h"
#include "Logger.h"
#include "utilities.h"

namespace UI
{

ContainerBase::ContainerBase(Bounds bounds) : ComponentBase(bounds)
{
    contentSize = localBounds.scale;
    // Use container-specific shader with texture and scroll support
    this->sprite.SetShader(GET_RESOURCE_PATH("shader/UI/container.vert"), GET_RESOURCE_PATH("shader/UI/container.frag"));
    UpdateTheme();
}

void ContainerBase::AddChild(const std::shared_ptr<ComponentBase> &child)
{
    children.push_back(child);
    child->SetParent(std::static_pointer_cast<ContainerBase>(shared_from_this()));
}

void ContainerBase::Initialize()
{
    ComponentBase::Initialize();

    for (auto &child : children)
    {
        child->Initialize();
    }
    InitializeRenderTarget();
    RecalculateChildBounds();
}

void ContainerBase::Update()
{
    ComponentBase::Update();

    // Apply deferred layout properties
    bool layoutChanged = false;
    if (padding.Apply())
    {
        layoutChanged = true;
    }
    if (spacing.Apply())
    {
        layoutChanged = true;
    }
    bool wasWrap = overflowMode.Get() == OverflowMode::WRAP;
    if (overflowMode.Apply())
    {
        if (wasWrap || (overflowMode.Get() == OverflowMode::WRAP))
        {
            layoutChanged = true;
        }
    }

    if (layoutChanged)
    {
        MarkFullDirty();
    }

    for (auto &child : children)
    {
        child->Update();
    }
    if (dirtySelfLayout || dirtyChildLayout)
    {
        RecalculateChildBounds();
        InitializeRenderTarget();
    }
}

// Render target helper functions
void ContainerBase::InitializeRenderTarget()
{
    if (contentSize.x <= 0 || contentSize.y <= 0)
    {
        return;
    }

    if (renderTarget.GetWidth() != static_cast<int>(contentSize.x) || renderTarget.GetHeight() != static_cast<int>(contentSize.y))
    {

        renderTarget.Init(static_cast<int>(contentSize.x), static_cast<int>(contentSize.y));
        fboInitialized = true;
        GRAPHICS_CHECK_ERRORS_M("UIContainer RenderTarget Init");

        // Clear render target to transparent immediately after init
        const GraphicsRenderState::FramebufferState previousState = GraphicsRenderState::CaptureFramebufferState();

        renderTarget.Bind();
        GraphicsRenderState::ClearTransparentColorBuffer();
        renderTarget.Unbind();
        GraphicsRenderState::RestoreFramebufferState(previousState);

        // Force re-render on next frame
        MarkFullDirty();
    }
}

void SaveRenderTargetState(std::uint32_t &oldFramebuffer, int viewport[4])
{
    const GraphicsRenderState::FramebufferState state = GraphicsRenderState::CaptureFramebufferState();
    oldFramebuffer = state.framebuffer;
    for (int i = 0; i < 4; ++i)
    {
        viewport[i] = state.viewport[i];
    }
}

void RestoreRenderTargetState(std::uint32_t oldFramebuffer, int viewport[4])
{
    GraphicsRenderState::BindFramebuffer(oldFramebuffer);
    GraphicsRenderState::SetViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void ContainerBase::ClearZone(glm::vec4 bounds)
{
    GraphicsRenderState::SetScissorTest(true);
    // Flip Y for OpenGL (Bottom-Left origin)
    // Bounds are (x, y, w, h) in Top-Left origin
    const int yGl = static_cast<int>(contentSize.y - (bounds.y + bounds.w));

    GraphicsRenderState::SetScissor(static_cast<int>(bounds.x), yGl, static_cast<int>(bounds.z), static_cast<int>(bounds.w));
    GraphicsRenderState::ClearTransparentColorBuffer();
    GraphicsRenderState::SetScissorTest(false);
}

void ContainerBase::RenderChildren()
{
    std::uint32_t oldFramebuffer = 0;
    int viewport[4];
    SaveRenderTargetState(oldFramebuffer, viewport);

    renderTarget.Bind();
    GRAPHICS_CHECK_ERRORS_M("RenderDirtyChildren Bind");

    // Set viewport to render target size
    GraphicsRenderState::SetViewport(0, 0, static_cast<int>(contentSize.x), static_cast<int>(contentSize.y));

    // Determine dirty level: layout vs appearance only
    bool hasLayoutDirty = IsSelfLayoutDirty(); // If we resized, we must re-render all (anchors changed)
    bool hasAppearanceDirty = false;
    for (auto &child : children)
    {
        if (child->IsSelfLayoutDirty() || child->IsChildLayoutDirty())
        {
            hasLayoutDirty = true;
            break;
        }
        if (child->IsAppearanceDirty())
        {
            hasAppearanceDirty = true;
        }
    }

    if (hasLayoutDirty)
    {
        // Layout changed: clear the full render target and re-render all
        GraphicsRenderState::ClearTransparentColorBuffer();

        for (auto &child : children)
        {
            glm::vec4 childBounds = child->GetCachedBoundsInParent();
            child->Draw(contentSize, {childBounds.x, childBounds.y});
            child->ClearDirty();
        }
    }
    else if (hasAppearanceDirty)
    {
        // Appearance only: zone clear and re-render dirty children
        for (auto &child : children)
        {
            if (child->IsAppearanceDirty())
            {
                glm::vec4 childBounds = child->GetCachedBoundsInParent();
                ClearZone(childBounds);
                child->Draw(contentSize, {childBounds.x, childBounds.y});
                child->ClearDirty();
            }
        }
    }

    renderTarget.Unbind();
    GRAPHICS_CHECK_ERRORS_M("RenderDirtyChildren Unbind");

    RestoreRenderTargetState(oldFramebuffer, viewport);
    GRAPHICS_CHECK_ERRORS_M("RenderDirtyChildren Restore");
}

void ContainerBase::Draw(glm::vec2 containerSize, glm::vec2 offset)
{
    if (!visible.Get())
    {
        return;
    }

    // Update child positions
    if (dirtySelfLayout || dirtyChildLayout)
    {
        RecalculateChildBounds();
    }

    // offset already includes anchor offset from cachedBoundsInParent
    RenderChildren();

    sprite.Draw({.offset = offset,
                 .scale = localBounds.scale,
                 .containerSize = containerSize,
                 .scrollOffset = scrollOffset,
                 .contentSize = contentSize,
                 .color = this->color.Get()},
                &renderTarget.GetTexture());

    ClearDirty();
}

void ContainerBase::MarkFullDirty()
{
    MarkSelfLayoutDirty();
    for (auto &child : children)
    {
        child->MarkFullDirty();
    }

    NotifyParentChildLayoutDirty();
}

void ContainerBase::RecalculateChildBounds()
{
    CalculatePixelSize();
    float p = GetPadding();
    // Default implementation: stack children at origin + padding, with anchor offset
    for (auto &child : children)
    {
        glm::vec2 childSize = child->CalculatePixelSize();
        // Calculate anchor offset based on contentSize
        glm::vec2 anchorOffset = child->GetAnchorOffset(contentSize);
        float x = p + anchorOffset.x;
        float y = p + anchorOffset.y;
        child->SetCachedBoundsInParent({x, y, childSize.x, childSize.y});
    }

    contentSize.x = localBounds.scale.x;
    contentSize.y = localBounds.scale.y;
}

void ContainerBase::UpdateTheme()
{
    ComponentBase::UpdateTheme();
    if (auto t = theme.lock())
    {
        padding.ForceSet(t->GetPadding());
        spacing.ForceSet(t->GetSpacing());
    }
}

void ContainerBase::DoSetPadding(float p) { padding.Set(p); }

void ContainerBase::DoSetSpacing(float s) { spacing.Set(s); }

void ContainerBase::DoSetOverflowMode(OverflowMode mode) { overflowMode.Set(mode); }

void ContainerBase::DoSetChildrenAllowDeform(bool deform)
{
    for (auto &child : children)
    {
        child->DoSetAllowDeform(deform);
    }
}

} // namespace UI
