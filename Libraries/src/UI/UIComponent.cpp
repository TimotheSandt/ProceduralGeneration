#include "UIComponent.h"

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
    if (parent.lock() != nullptr) {
        this->localBounds.scale = localBounds.getPixelSize(parent.lock()->GetPixelSize());
        mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.scale));
        return this->localBounds.scale;
    }
    return localBounds.getPixelSize();
}

void UIComponent::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    mesh.InitUniform2f("offset", glm::value_ptr(offset));
    mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color));
    mesh.Draw();
}

bool UIComponent::IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const {
    return localBounds.isHover(mousePos - offset);
}

} // namespace UI
