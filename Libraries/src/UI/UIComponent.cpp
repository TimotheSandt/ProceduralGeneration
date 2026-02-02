#include "UIComponent.h"
#include "UIContainer.h"

#include "utilities.h"

namespace UI {

std::vector<GLfloat> UIComponent::GetVertices() const {
    std::array<glm::vec2, 4> bounds = localBounds.getPixelBounds();
    std::vector<GLfloat> vertices;
    for (int i = 0; i < 4; i++) {
        vertices.push_back(bounds[i].x);
        vertices.push_back(bounds[i].y);
    }
    return vertices;
}

UIComponent::UIComponent(
        Bounds bounds,
        std::weak_ptr<UITheme> theme, IdentifierKind kind)
            : localBounds(bounds),
            theme(theme), kind(kind) {
    std::vector<GLfloat> vertices = GetVertices();
    std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};
    this->mesh.Initialize(vertices, indices, {2});
    this->mesh.SetShader(GET_RESOURCE_PATH("shader/UI/default.vert"), GET_RESOURCE_PATH("shader/UI/default.frag"));
    this->color = theme.lock()->GetColor(kind);
}


glm::vec2 UIComponent::GetPixelSize() {
    if (auto p = parent.lock()) {
        this->localBounds.scale = localBounds.getPixelSize(p->GetPixelSize());
        mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.scale));
        return this->localBounds.scale;
    }
    return localBounds.getPixelSize();
}

void UIComponent::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (!visible) return;

    // First-time initialization
    if (needsInitialize) {
        Initialize();
        needsInitialize = false;
    }

    // Calculate anchor offset based on element's anchor within its drawable area
    glm::vec2 anchorOffset = localBounds.getAnchorOffset(containerSize);

    // Total offset = parent offset + anchor offset
    glm::vec2 totalOffset = offset + anchorOffset;

    mesh.InitUniform2f("offset", glm::value_ptr(totalOffset));
    mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color));
    mesh.Draw();
}

bool UIComponent::IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const {
    return localBounds.isHover(mousePos - offset);
}

void UIComponent::MarkDirty(DirtyType d) {
    dirty = d;
    // Propagate dirty state upward to parent
    NotifyParentDirty();
}

void UIComponent::NotifyParentDirty() {
    if (auto p = parent.lock()) {
        // Only notify parent with CONTENT dirty, not ALL
        // This way the parent knows a child changed but doesn't need to recalculate everything
        if (p->GetDirtyType() == DirtyType::NONE) {
            p->MarkDirty(DirtyType::CONTENT);
        }
    }
}

} // namespace UI
