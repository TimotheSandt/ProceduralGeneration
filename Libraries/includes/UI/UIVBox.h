#pragma once
#include "UIContainer.h"

namespace UI {

class UIVBoxBase : public UIContainerBase {
protected:
    HAlign childAlignment = HAlign::LEFT;
    JustifyContent justifyContent = JustifyContent::START;


public:
    using UIContainerBase::UIContainerBase;

    HAlign GetChildAlignment() const { return childAlignment; }
    JustifyContent GetJustifyContent() const { return justifyContent; }

    void DoSetChildAlignment(HAlign align);
    void DoSetJustifyContent(JustifyContent j);

    glm::vec2 GetAvailableSize() const override;

protected:
    void RecalculateChildBounds() override;
};

template<typename Base, typename Derived>
class ChainableVBox : public ChainableContainer<Base, Derived> {
public:
    using ChainableContainer<Base, Derived>::ChainableContainer;

    std::shared_ptr<Derived> SetChildAlignment(HAlign align) {
        this->DoSetChildAlignment(align);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetJustifyContent(JustifyContent align) {
        this->DoSetJustifyContent(align);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }
};

class UIVBox : public ChainableVBox<UIVBoxBase, UIVBox> {
public:
    using ChainableVBox<UIVBoxBase, UIVBox>::ChainableVBox;
};

// Factory for VBox
inline std::shared_ptr<UIVBox> VBox(Bounds bounds = Bounds(), std::vector<std::shared_ptr<UIComponentBase>> children = {}) {
    auto vbox = std::make_shared<UIVBox>(bounds);
    for (auto& child : children) {
        vbox->AddChild(child);
    }
    return vbox;
}

}
