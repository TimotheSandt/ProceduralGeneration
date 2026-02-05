#pragma once
#include "UIContainer.h"

namespace UI {

class UIHBoxBase : public UIContainerBase {
protected:
    DeferredValue<VAlign> childAlignment = VAlign::TOP;
    DeferredValue<JustifyContent> justifyContent = JustifyContent::START;

public:
    using UIContainerBase::UIContainerBase;

    void Initialize() override;

    VAlign GetChildAlignment() const { return childAlignment.Get(); }
    JustifyContent GetJustifyContent() const { return justifyContent.Get(); }

    void DoSetChildAlignment(VAlign align);
    void DoSetJustifyContent(JustifyContent j);

    void Update() override;

protected:
    void RecalculateChildBounds() override;
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
