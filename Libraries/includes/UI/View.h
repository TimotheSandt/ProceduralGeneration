#pragma once

#include "Core/Container.h"

namespace UI
{

class View : public ContainerBase
{
  protected:
    explicit View(Bounds bounds)
        : ContainerBase(bounds)
    {
    }

    virtual std::shared_ptr<UI::ContainerBase> Build() = 0;

  public:
    void Initialize() override
    {
        if (!built)
        {
            AddChild(Build());
            built = true;
        }
        ContainerBase::Initialize();
    }

  private:
    bool built = false;
};

} // namespace UI

using View = UI::View;
