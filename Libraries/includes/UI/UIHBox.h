#pragma once
#include "UIContainer.h"

namespace UI {

class UIHBoxBase : public UIContainerBase {
protected:
    VAlign childAlignment = VAlign::TOP;
    JustifyContent justifyContent = JustifyContent::START;

public:
    using UIContainerBase::UIContainerBase;

    VAlign GetChildAlignment() const { return childAlignment; }
    JustifyContent GetJustifyContent() const { return justifyContent; }

    void DoSetChildAlignment(VAlign align);
    void DoSetJustifyContent(JustifyContent j);

protected:
    void RecalculateChildBounds() override;
    void CalculateContentSize();
};

template<typename Base, typename Derived>
class ChainableHBox : public ChainableContainer<Base, Derived> {
public:
    using ChainableContainer<Base, Derived>::ChainableContainer;

    std::shared_ptr<Derived> SetChildAlignment(VAlign align) {
        this->DoSetChildAlignment(align);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetJustifyContent(JustifyContent align) {
        this->DoSetJustifyContent(align);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }
};

class UIHBox : public ChainableHBox<UIHBoxBase, UIHBox> {
public:
    using ChainableHBox<UIHBoxBase, UIHBox>::ChainableHBox;
};

// Factory for HBox
inline std::shared_ptr<UIHBox> HBox(Bounds bounds = Bounds(), std::vector<std::shared_ptr<UIComponentBase>> children = {}) {
    auto hbox = std::make_shared<UIHBox>(bounds);
    for (auto& child : children) {
        hbox->AddChild(child);
    }
    return hbox;
}

}
