#include "Core/Component.h"

#include <utility>
#include "Core/Container.h"

#include "utilities.h"

namespace UI
{

std::vector<GLfloat> ComponentBase::GetVertices() const
{
    // Unit quad (0-1 range) - shader will multiply by scale and add offset
    return {
        0.0f, 0.0f, // bottom-left
        1.0f, 0.0f, // bottom-right
        1.0f, 1.0f, // top-right
        0.0f, 1.0f  // top-left
    };
}

ComponentBase::ComponentBase(Bounds bounds) : localBounds(bounds)
{
    std::vector<GLfloat> vertices = GetVertices();
    std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};
    this->mesh.Initialize(vertices, indices, {2});
    this->mesh.SetShader(GET_RESOURCE_PATH("shader/UI/default.vert"), GET_RESOURCE_PATH("shader/UI/default.frag"));

    // Direct initialization with ForceSet to avoid deferred behavior
    this->theme = Theme::GetTheme("default");
    this->kind.ForceSet(IdentifierKind::PRIMARY);
    UpdateTheme();
}

void ComponentBase::Initialize()
{
    MarkSelfLayoutDirty();
    CalculatePixelSize();
}

void ComponentBase::Update()
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

    mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.scale));
    return this->localBounds.scale;
}

void ComponentBase::Draw(glm::vec2 containerSize, glm::vec2 offset)
{
    if (!visible.Get())
    {
        return;
    }

    // offset already includes anchor offset from cachedBoundsInParent

    mesh.BindShader();
    mesh.BindVAO();

    mesh.InitUniform2f("offset", glm::value_ptr(offset));
    mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color.Get()));

    mesh.Draw();

    mesh.UnbindVAO();
    mesh.UnbindShader();

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
    NotifyParentChildLayoutDirty();
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
        p->MarkChildLayoutDirty();
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
