#include "UIComponent.h"
#include "UIContainer.h"

#include "utilities.h"

namespace UI {

std::vector<GLfloat> UIComponentBase::GetVertices() const {
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

    this->theme.Set(UITheme::GetTheme("default"));
    this->kind.Set(IdentifierKind::PRIMARY);

    if (auto t = theme.Get().lock()) {
         color.Set(t->GetColor(kind.Get()));
    }
}

void UIComponentBase::Initialize() {
    parent.Apply();
    GetPixelSize();
    localBounds.Apply();
    UpdateTheme();

    MarkFullDirty(false);
}

void UIComponentBase::Update() {
    bool themeChanged = theme.Apply();
    bool kindChanged = kind.Apply();
    if (themeChanged || kindChanged) {
        MarkDirty();
        UpdateTheme();
    }

    if (color.Apply()) {
        MarkDirty();
    }

    dirtyLayout = dirtyLayout || parent.Apply();
    dirtyLayout = dirtyLayout || localBounds.Apply();
    dirtyLayout = dirtyLayout || cachedBoundsInParent.Apply();
    dirtyLayout = dirtyLayout || visible.Apply();

    if (dirtyLayout) UpdateLayout();
    dirty = false;
}

void UIComponentBase::UpdateLayout() {
    MarkFullDirty();
    GetPixelSize();
    dirtyLayout = false;
}

void UIComponentBase::UpdateTheme() {
    if (auto t = theme.Get().lock()) {
        color.ForceSet(t->GetColor(kind.Get()));
    }
}


glm::vec2 UIComponentBase::GetPixelSize() {
    glm::vec2 size;
    if (auto p = parent.Get().lock()) {
        this->localBounds.ModifyForce([&](Bounds& b) {
            size = b.getPixelSize(p->GetPixelSize(), p->GetPadding(), p->GetSpacing());
            b.scale = size;
        });
    } else {
        this->localBounds.ModifyForce([&](Bounds& b) {
            size = b.getPixelSize();
            b.scale = size;
        });
    }

    mesh.InitUniform2f("scale", glm::value_ptr(size));
    return size;
}

void UIComponentBase::Draw(glm::vec2 offset) {
    if (!visible.Get()) return;

    // FIX 5: Vérifier que le parent existe avant de l'utiliser
    glm::vec2 containerSize;
    if (auto p = parent.Get().lock()) {
        containerSize = p->localBounds.Get().scale;
    } else {
        // Si pas de parent, utiliser notre propre taille comme container
        containerSize = this->localBounds.Get().scale;
    }

    glm::vec2 anchorOffset = localBounds.Get().getAnchorOffset(containerSize);
    glm::vec2 totalOffset = offset + anchorOffset;

    mesh.BindShader();
    mesh.BindVAO();

    mesh.InitUniform2f("offset", glm::value_ptr(totalOffset));
    mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.Get().scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color.Get()));

    mesh.Draw();

    mesh.UnbindVAO();
    mesh.UnbindShader();
}

bool UIComponentBase::IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const {
    return localBounds.Get().isHover(mousePos - offset);
}

void UIComponentBase::DoSetColor(glm::vec4 c) {
    color.Set(c);
}

void UIComponentBase::DoSetTheme(std::weak_ptr<UITheme> t) {
    theme.Set(t);
}

void UIComponentBase::DoSetIdentifierKind(IdentifierKind k) {
    kind.Set(k);
}

void UIComponentBase::MarkDirty(bool propagate) {
    dirty = true;
    if (propagate) NotifyParentDirty();
}

void UIComponentBase::MarkFullDirty(bool propagate) {
    MarkDirty(propagate);
    dirtyLayout = true;
}

void UIComponentBase::NotifyParentDirty() {
    if (auto p = parent.Get().lock()) {
        p->MarkDirty();
    }
}

void UIComponentBase::NotifyParentFullDirty() {
    if (auto p = parent.Get().lock()) {
        p->MarkFullDirty();
    }
}

} // namespace UI
