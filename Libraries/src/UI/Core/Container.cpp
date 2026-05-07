#include "Core/Container.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GraphicsDiagnostics.h"
#include "Logger.h"
#include "utilities.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace UI
{

namespace
{

struct ClipRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

std::optional<ClipRect> ComputeClipRect(const GraphicsRenderState::FramebufferState &state, glm::vec2 containerSize, glm::vec2 offset,
                                        glm::vec2 scale)
{
    const int viewportLeft = state.viewport[0];
    const int viewportBottom = state.viewport[1];
    const int viewportRight = viewportLeft + state.viewport[2];
    const int viewportTop = viewportBottom + state.viewport[3];

    if (containerSize.x <= 0.0f || containerSize.y <= 0.0f || state.viewport[2] <= 0 || state.viewport[3] <= 0)
    {
        return std::nullopt;
    }

    const float scaleX = static_cast<float>(state.viewport[2]) / containerSize.x;
    const float scaleY = static_cast<float>(state.viewport[3]) / containerSize.y;

    int left = viewportLeft + static_cast<int>(std::floor(offset.x * scaleX));
    int bottom = viewportBottom + static_cast<int>(std::floor((containerSize.y - (offset.y + scale.y)) * scaleY));
    int right = viewportLeft + static_cast<int>(std::ceil((offset.x + scale.x) * scaleX));
    int top = viewportBottom + static_cast<int>(std::ceil((containerSize.y - offset.y) * scaleY));

    left = std::max(left, viewportLeft);
    bottom = std::max(bottom, viewportBottom);
    right = std::min(right, viewportRight);
    top = std::min(top, viewportTop);

    if (state.scissorTest)
    {
        const int scissorLeft = state.scissorBox[0];
        const int scissorBottom = state.scissorBox[1];
        const int scissorRight = scissorLeft + state.scissorBox[2];
        const int scissorTop = scissorBottom + state.scissorBox[3];

        left = std::max(left, scissorLeft);
        bottom = std::max(bottom, scissorBottom);
        right = std::min(right, scissorRight);
        top = std::min(top, scissorTop);
    }

    if (right <= left || top <= bottom)
    {
        return std::nullopt;
    }

    return ClipRect{left, bottom, right - left, top - bottom};
}

} // namespace

ContainerBase::ContainerBase(Bounds bounds, bool useRenderTarget) : ComponentBase(bounds)
{
    contentSize = localBounds.scale;
    renderToTexture.ForceSet(useRenderTarget);
    RefreshSpriteShader();
    UpdateTheme();
}

void ContainerBase::AddChild(const std::shared_ptr<ComponentBase> &child)
{
    children.push_back(child);
    child->SetParent(std::static_pointer_cast<ContainerBase>(shared_from_this()));
    if (initialized)
    {
        child->Initialize();
    }
}

void ContainerBase::Initialize()
{
    ComponentBase::Initialize();

    for (auto &child : children)
    {
        child->Initialize();
    }
    RecalculateChildBounds();
    InitializeRenderTarget();
}

void ContainerBase::Update()
{
    ComponentBase::Update();
    if (WasThrottled())
        return;

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
    bool renderModeChanged = false;
    if (renderToTexture.Apply())
    {
        renderModeChanged = true;
        RefreshSpriteShader();
        if (!UsesOwnRenderTarget())
        {
            renderTarget.Destroy();
            fboInitialized = false;
        }
        dirtySelfLayout = true;
        dirtyChildLayout = true;
        dirtyAppearance = true;
        NotifyParentChildAppearanceDirty();
    }

    if (layoutChanged)
    {
        MarkFullDirty();
    }

    for (auto &child : children)
    {
        child->Update();
    }
    if (dirtySelfLayout || dirtyChildLayout || renderModeChanged)
    {
        RecalculateChildBounds();
        InitializeRenderTarget();
    }
}

void ContainerBase::RefreshSpriteShader()
{
    if (UsesOwnRenderTarget())
    {
        sprite.SetShader(GET_RESOURCE_PATH("shader/UI/container.vert"), GET_RESOURCE_PATH("shader/UI/container.frag"));
    }
    else
    {
        sprite.SetShader(GET_RESOURCE_PATH("shader/UI/default.vert"), GET_RESOURCE_PATH("shader/UI/default.frag"));
    }
}

void ContainerBase::InitializeRenderTarget()
{
    if (!UsesOwnRenderTarget())
    {
        if (renderTarget.IsInitialized())
        {
            renderTarget.Destroy();
        }
        fboInitialized = false;
        return;
    }

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
    if (!UsesOwnRenderTarget())
    {
        return;
    }

    const GraphicsRenderState::FramebufferState previousState = GraphicsRenderState::CaptureFramebufferState();

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

    GraphicsRenderState::RestoreFramebufferState(previousState);
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
        InitializeRenderTarget();
    }

    if (UsesOwnRenderTarget())
    {
        RenderChildren();

        sprite.Draw({.offset = offset,
                     .scale = localBounds.scale,
                     .containerSize = containerSize,
                     .scrollOffset = scrollOffset,
                     .contentSize = contentSize,
                     .color = this->color.Get()},
                    &renderTarget.GetTexture());

        ClearDirty();
        return;
    }

    if (GetColor().a > 0.0f)
    {
        sprite.Draw({.offset = offset, .scale = localBounds.scale, .containerSize = containerSize, .color = this->color.Get()});
    }

    const bool shouldClipChildren = overflowMode.Get() == OverflowMode::HIDDEN || overflowMode.Get() == OverflowMode::SCROLL;
    std::optional<GraphicsRenderState::FramebufferState> clipState;
    if (shouldClipChildren)
    {
        clipState = GraphicsRenderState::CaptureFramebufferState();
        const std::optional<ClipRect> clipRect = ComputeClipRect(*clipState, containerSize, offset, localBounds.scale);
        if (!clipRect.has_value())
        {
            GraphicsRenderState::RestoreFramebufferState(*clipState);
            ClearDirty();
            return;
        }

        GraphicsRenderState::SetScissorTest(true);
        GraphicsRenderState::SetScissor(clipRect->x, clipRect->y, clipRect->width, clipRect->height);
    }

    const glm::vec2 childBaseOffset = offset - scrollOffset;
    for (auto &child : children)
    {
        const glm::vec4 childBounds = child->GetCachedBoundsInParent();
        child->Draw(containerSize, {childBaseOffset.x + childBounds.x, childBaseOffset.y + childBounds.y});
    }

    if (clipState.has_value())
    {
        GraphicsRenderState::RestoreFramebufferState(*clipState);
    }

    ClearDirty();
}

void ContainerBase::MarkFullDirty()
{
    MarkSelfLayoutDirty();
    for (auto &child : children)
    {
        child->MarkFullDirty();
    }
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

void ContainerBase::DoSetRenderToTexture(bool enabled) { renderToTexture.Set(enabled); }

void ContainerBase::DoSetChildrenAllowDeform(bool deform)
{
    for (auto &child : children)
    {
        child->DoSetAllowDeform(deform);
    }
}

void ContainerBase::OnChildAppearanceDirty()
{
    dirtyAppearance = true;
    NotifyParentChildAppearanceDirty();
}

void ContainerBase::OnChildLayoutDirty()
{
    dirtyChildLayout = true;
    dirtyAppearance = true;
    NotifyParentChildLayoutDirty();
}

} // namespace UI
