#pragma once
#include "../Core/Container.h"

namespace UI
{

class HBoxBase : public ContainerBase
{
  protected:
    VAlign childAlignment = VAlign::TOP;
    JustifyContent justifyContent = JustifyContent::START;

  public:
    using ContainerBase::ContainerBase;

    VAlign GetChildAlignment() const { return childAlignment; }
    JustifyContent GetJustifyContent() const { return justifyContent; }

    void DoSetChildAlignment(VAlign align);
    void DoSetJustifyContent(JustifyContent j);

    glm::vec2 GetAvailableSize() const override;

  protected:
    void RecalculateChildBounds() override;
};

template <typename Base, typename Derived> class ChainableHBox : public ChainableContainer<Base, Derived>
{
  public:
    using ChainableContainer<Base, Derived>::ChainableContainer;

    std::shared_ptr<Derived> SetChildAlignment(VAlign align)
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

class HBox : public ChainableHBox<HBoxBase, HBox>
{
  public:
    using ChainableHBox<HBoxBase, HBox>::ChainableHBox;
};

// Factory for HBox
inline std::shared_ptr<HBox> CreateHBox(Bounds bounds = Bounds(), const std::vector<std::shared_ptr<ComponentBase>> &children = {})
{
    auto hbox = std::make_shared<HBox>(bounds);
    for (auto &child : children)
    {
        hbox->AddChild(child);
    }
    return hbox;
}

} // namespace UI
