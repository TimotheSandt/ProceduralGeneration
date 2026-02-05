#include "UIComponent.h"
#include "UIContainer.h"

#include "utilities.h"

namespace UI {

std::vector<GLfloat> UIComponentBase::GetVertices() const {
    // Unit quad (0-1 range) - shader will multiply by scale and add offset
    return {
        0.0f, 0.0f,  // bottom-left
        1.0f, 0.0f,  // bottom-right
        1.0f, 1.0f,  // top-right
        0.0f, 1.0f   // top-left
    };
}

UIComponentBase::UIComponentBase(Bounds bounds) : localBounds(bounds) {
    std::vector<GLfloat> vertices = GetVertices();
    std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};
    this->mesh.Initialize(vertices, indices, {2});
    this->mesh.SetShader(
        GET_RESOURCE_PATH("shader/UI/default.vert"),
        GET_RESOURCE_PATH("shader/UI/default.frag")
    );

    // Direct initialization with ForceSet to avoid deferred behavior
    this->theme = UITheme::GetTheme("default");
    this->kind.ForceSet(IdentifierKind::PRIMARY);
    UpdateTheme();
}

void UIComponentBase::Initialize() {
    MarkDirty();
    GetPixelSize();
}

void UIComponentBase::Update() {
    // Apply deferred values and mark dirty if they changed
    if (kind.Apply()) {
        UpdateTheme();
        MarkDirty();
    }
    if (color.Apply()) {
        MarkDirty();
    }
    if (visible.Apply()) {
        MarkDirty();
    }
}
glm::vec2 UIComponentBase::GetPixelSize() {
    if (auto p = parent.lock()) {
        this->localBounds.scale = localBounds.getPixelSize(p->GetPixelSize(), p->GetPadding(), p->GetSpacing());
    } else {
        this->localBounds.scale = localBounds.getPixelSize();
    }

    mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.scale));
    return this->localBounds.scale;
}

void UIComponentBase::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (!visible.Get()) return;

    glm::vec2 anchorOffset = localBounds.getAnchorOffset(containerSize);
    glm::vec2 totalOffset = offset + anchorOffset;

    mesh.BindShader();
    mesh.BindVAO();

    mesh.InitUniform2f("offset", glm::value_ptr(totalOffset));
    mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color.Get()));

    mesh.Draw();

    mesh.UnbindVAO();
    mesh.UnbindShader();
}

bool UIComponentBase::IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const {
    return localBounds.isHover(mousePos - offset);
}

void UIComponentBase::DoSetColor(glm::vec4 c) {
    color.ForceSet(c);
    MarkDirty();
}

void UIComponentBase::DoSetTheme(std::weak_ptr<UITheme> t) {
    theme = t;
    UpdateTheme();
    MarkDirty();
}

void UIComponentBase::DoSetIdentifierKind(IdentifierKind k) {
    kind.ForceSet(k);
    UpdateTheme();
    MarkDirty();
}

void UIComponentBase::MarkDirty() {
    dirty = true;
    NotifyParentDirty();
}

void UIComponentBase::MarkFullDirty() {
    MarkDirty();
}

void UIComponentBase::NotifyParentDirty() {
    if (auto p = parent.lock()) {
        p->MarkDirty();
    }
}

void UIComponentBase::NotifyParentFullDirty() {
    if (auto p = parent.lock()) {
        p->MarkFullDirty();
    }
}

void UIComponentBase::UpdateTheme() {
    if (auto t = theme.lock()) {
        color.ForceSet(t->GetColor(kind.Get()));
    }
}

} // namespace UI
