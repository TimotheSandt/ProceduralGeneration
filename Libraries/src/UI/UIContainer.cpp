#include "UIContainer.h"

namespace UI {


UIContainer::UIContainer(Bounds bounds, std::weak_ptr<UITheme> theme, IdentifierKind kind) : UIComponent(bounds, theme, kind) {}


void UIContainer::InitFBO() {
    fbo.Init(localBounds.scale.x, localBounds.scale.y);
}


void UIContainer::RenderToFBO() {
    fbo.Bind();
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glViewport(0, 0, localBounds.scale.x, localBounds.scale.y);
    for (auto& child : children) {
        child->Draw(localBounds.scale);
    }
    fbo.Unbind();

    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void UIContainer::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (dirty != DirtyType::NONE) {
        RenderToFBO();
        mesh.InitUniform2f("offset", glm::value_ptr(offset));
        mesh.InitUniform2f("scale", glm::value_ptr(this->localBounds.scale));
        mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
        mesh.InitUniform4f("color", glm::value_ptr(this->color));
        mesh.BindTexture(fbo.GetTextureID(), 0);
        mesh.Draw();
    }
}


void UIContainer::MarkDirty(DirtyType d) {
    switch (d) {
        case DirtyType::ALL:
        case DirtyType::TRANSFORM:
        case DirtyType::VISIBILITY:
            for (auto& child : children) {
                child->MarkDirty(d);
            }
            UIComponent::MarkDirty(DirtyType::ALL);
            break;
        case DirtyType::ANIMATION:
        case DirtyType::CONTENT:
        case DirtyType::THEME:
            UIComponent::MarkDirty(DirtyType::CONTENT);
        case DirtyType::NONE:
        default:
            UIComponent::MarkDirty(DirtyType::NONE);
            break;
    }
};

}
