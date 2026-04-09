#include "UIComponent.h"

#include <utility>
#include "UIContainer.h"

#include "utilities.h"

namespace UI
{

UIComponentBase::UIComponentBase(Bounds bounds) : localBounds(bounds)
{
    this->sprite.SetShader(GET_RESOURCE_PATH("shader/UI/default.vert"), GET_RESOURCE_PATH("shader/UI/default.frag"));

    // Direct initialization with ForceSet to avoid deferred behavior
    this->theme = UITheme::GetTheme("default");
    this->kind.ForceSet(IdentifierKind::PRIMARY);
    UpdateTheme();
}

void UIComponentBase::Initialize()
{
    MarkSelfLayoutDirty();
    CalculatePixelSize();
}

void UIComponentBase::Update()
{
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

glm::vec2 UIComponentBase::CalculatePixelSize()
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

void UIComponentBase::Draw(glm::vec2 containerSize, glm::vec2 offset)
{
    if (!visible.Get())
    {
        return;
    }

    // offset already includes anchor offset from cachedBoundsInParent

    sprite.Draw({.offset = offset, .scale = this->localBounds.scale, .containerSize = containerSize, .color = this->color.Get()});

    ClearDirty();
}

bool UIComponentBase::IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const { return localBounds.isHover(mousePos - offset); }

void UIComponentBase::DoSetColor(glm::vec4 c)
{
    color.Set(c);
    MarkAppearanceDirty();
}

void UIComponentBase::DoSetTheme(std::weak_ptr<UITheme> t)
{
    theme = std::move(t);
    UpdateTheme();
    MarkAppearanceDirty();
}

void UIComponentBase::DoSetIdentifierKind(IdentifierKind k)
{
    kind.Set(k);
    UpdateTheme();
    MarkAppearanceDirty();
}

void UIComponentBase::DoSetAllowDeform(bool allow)
{
    allowDeform.Set(allow);
    if (!isDeformed && allowDeform.Get())
    {
        allowDeform.Apply();
    }
}

// Three-tier dirty system implementation
void UIComponentBase::MarkAppearanceDirty()
{
    dirtyAppearance = true;
    NotifyParentChildLayoutDirty();
}

void UIComponentBase::MarkChildLayoutDirty()
{
    dirtyChildLayout = true;
    dirtyAppearance = true; // Layout implies appearance
    NotifyParentChildLayoutDirty();
}

void UIComponentBase::MarkSelfLayoutDirty()
{
    dirtySelfLayout = true;
    dirtyChildLayout = true;
    dirtyAppearance = true; // Self layout implies all dirty
    NotifyParentChildLayoutDirty();
}

void UIComponentBase::MarkFullDirty() { MarkSelfLayoutDirty(); }

void UIComponentBase::NotifyParentChildLayoutDirty()
{
    if (auto p = parent.lock())
    {
        p->MarkChildLayoutDirty();
    }
}

void UIComponentBase::NotifyParentFullDirty()
{
    if (auto p = parent.lock())
    {
        p->MarkFullDirty();
    }
}

void UIComponentBase::UpdateTheme()
{
    if (auto t = theme.lock())
    {
        color.ForceSet(t->GetColor(kind.Get()));
    }
}

} // namespace UI
