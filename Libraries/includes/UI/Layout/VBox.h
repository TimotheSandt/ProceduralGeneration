#pragma once
#include "../Core/Container.h"

namespace UI
{

class VBoxBase : public ContainerBase
{
  protected:
    HAlign childAlignment = HAlign::LEFT;
    JustifyContent justifyContent = JustifyContent::START;

  public:
    using ContainerBase::ContainerBase;

    HAlign GetChildAlignment() const { return childAlignment; }
    JustifyContent GetJustifyContent() const { return justifyContent; }

    void DoSetChildAlignment(HAlign align);
    void DoSetJustifyContent(JustifyContent j);

    glm::vec2 GetAvailableSize() const override;

  protected:
    void RecalculateChildBounds() override;
};

template <typename Base, typename Derived> class ChainableVBox : public ChainableContainer<Base, Derived>
{
  public:
    using ChainableContainer<Base, Derived>::ChainableContainer;

    std::shared_ptr<Derived> SetChildAlignment(HAlign align)
    {
        this->DoSetChildAlignment(align);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetJustifyContent(JustifyContent align)
    {
        this->DoSetJustifyContent(align);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }
};

class VBox : public ChainableVBox<VBoxBase, VBox>
{
  public:
    using ChainableVBox<VBoxBase, VBox>::ChainableVBox;
};

// Factory for VBox
inline std::shared_ptr<VBox> CreateVBox(Bounds bounds = Bounds(), const std::vector<std::shared_ptr<ComponentBase>> &children = {})
{
    auto vbox = std::make_shared<VBox>(bounds);
    for (auto &child : children)
    {
        vbox->AddChild(child);
    }
    return vbox;
}

} // namespace UI
