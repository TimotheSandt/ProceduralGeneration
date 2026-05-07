#include "Core/Component.h"

#include <utility>
#include "Core/Container.h"

#include "utilities.h"

namespace UI
{

ComponentBase::ComponentBase(Bounds bounds) : localBounds(bounds)
{
    this->sprite.SetShader(GET_RESOURCE_PATH("shader/UI/default.vert"), GET_RESOURCE_PATH("shader/UI/default.frag"));

    // Direct initialization with ForceSet to avoid deferred behavior
    this->theme = Theme::GetTheme("default");
    this->kind.ForceSet(IdentifierKind::PRIMARY);
    UpdateTheme();
}

void ComponentBase::Initialize()
{
    initialized = true;
    MarkSelfLayoutDirty();
    CalculatePixelSize();
}

void ComponentBase::Update()
{
    // Throttle gate: skip update if period hasn't elapsed
    if (throttlePeriod.count() > 0)
    {
        auto now = std::chrono::high_resolution_clock::now();
        if (now - lastThrottleUpdate < throttlePeriod)
        {
            throttledThisFrame = true;
            return;
        }
        lastThrottleUpdate = now;
    }
    throttledThisFrame = false;

    // Apply deferred values and mark dirty if they changed
    if (kind.Apply())
    {
        UpdateTheme();
        MarkAppearanceDirty();
    }
    if (color.Apply())
    {
        MarkAppearanceDirty();
    }
    if (visible.Apply())
    {
        MarkAppearanceDirty();
    }
    if (allowDeform.Apply())
    {
        MarkSelfLayoutDirty();
    }

    if (dirtyChildLayout || dirtySelfLayout)
    {
        CalculatePixelSize();
    }
}

glm::vec2 ComponentBase::CalculatePixelSize()
{
    if (auto p = parent.lock())
    {
        this->localBounds.scale = localBounds.getPixelSize(p->GetAvailableSize());
    }
    else
    {
        this->localBounds.scale = localBounds.getPixelSize();
    }

    return this->localBounds.scale;
}

void ComponentBase::Draw(glm::vec2 containerSize, glm::vec2 offset)
{
    if (!visible.Get())
    {
        return;
    }

    // offset already includes anchor offset from cachedBoundsInParent

    sprite.Draw({.offset = offset, .scale = this->localBounds.scale, .containerSize = containerSize, .color = this->color.Get()});

    ClearDirty();
}

bool ComponentBase::IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const { return localBounds.isHover(mousePos - offset); }

void ComponentBase::DoSetColor(glm::vec4 c)
{
    color.Set(c);
    MarkAppearanceDirty();
}

void ComponentBase::DoSetTheme(std::weak_ptr<Theme> t)
{
    theme = std::move(t);
    UpdateTheme();
    MarkAppearanceDirty();
}

void ComponentBase::DoSetIdentifierKind(IdentifierKind k)
{
    kind.Set(k);
    UpdateTheme();
    MarkAppearanceDirty();
}

void ComponentBase::DoSetThrottlePeriod(std::chrono::duration<double> period)
{
    throttlePeriod = period;
    lastThrottleUpdate = {};
}

void ComponentBase::DoSetAllowDeform(bool allow)
{
    allowDeform.Set(allow);
    if (!isDeformed && allowDeform.Get())
    {
        allowDeform.Apply();
    }
}

// Three-tier dirty system implementation
void ComponentBase::MarkAppearanceDirty()
{
    dirtyAppearance = true;
    NotifyParentChildAppearanceDirty();
}

void ComponentBase::MarkChildLayoutDirty()
{
    dirtyChildLayout = true;
    dirtyAppearance = true; // Layout implies appearance
    NotifyParentChildLayoutDirty();
}

void ComponentBase::MarkSelfLayoutDirty()
{
    dirtySelfLayout = true;
    dirtyChildLayout = true;
    dirtyAppearance = true; // Self layout implies all dirty
    NotifyParentChildLayoutDirty();
}

void ComponentBase::MarkFullDirty() { MarkSelfLayoutDirty(); }

void ComponentBase::NotifyParentChildLayoutDirty()
{
    if (auto p = parent.lock())
    {
        p->OnChildLayoutDirty();
    }
}

void ComponentBase::NotifyParentChildAppearanceDirty()
{
    if (auto p = parent.lock())
    {
        p->OnChildAppearanceDirty();
    }
}

void ComponentBase::NotifyParentFullDirty()
{
    if (auto p = parent.lock())
    {
        p->MarkFullDirty();
    }
}

void ComponentBase::UpdateTheme()
{
    if (auto t = theme.lock())
    {
        color.ForceSet(t->GetColor(kind.Get()));
    }
}

} // namespace UI
