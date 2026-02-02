#pragma once
#include "UIComponent.h"

#include "FBO.h"

namespace UI {

class UIContainer : public UIComponent, public std::enable_shared_from_this<UIContainer> {
protected:
    std::vector<std::shared_ptr<UIComponent>> children;

    // FBO pour cache
    FBO fbo;

public:

    UIContainer(
            Bounds bounds,
            std::weak_ptr<UITheme> theme = UITheme::GetTheme("default"),
            IdentifierKind kind = IdentifierKind::PRIMARY
        );
    
    template<typename T, typename... Args>
    UIContainer(
            Bounds bounds,
            std::weak_ptr<UITheme> theme = UITheme::GetTheme("default"),
            IdentifierKind kind = IdentifierKind::PRIMARY,
            Args&&... args
        ) : UIContainer(bounds, theme, kind) {
            AddChild(std::forward<Args>(args)...);
        }

    void AddChild(std::shared_ptr<UIComponent> child) {
        children.push_back(child);
        children.back()->SetParent(shared_from_this());
    }
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;

private:
    template<typename T, typename... Args>
    void AddChild(T child, Args&&... args) {
        AddChild(child);
        AddChild(std::forward<Args>(args)...);
    }
    void InitFBO();
    void RenderToFBO();

public:
    void MarkDirty(DirtyType d = DirtyType::ALL) override;
};



class UIHBox : public UIContainer {
public:
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;
};


class UIVBox : public UIContainer {
public:
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;
};


}
