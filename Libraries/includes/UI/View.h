#pragma once

#include "Core/Container.h"

namespace UI
{

class View : public ContainerBase
{
  protected:
    explicit View(Bounds bounds, bool useRenderTarget = true)
        : ContainerBase(bounds, useRenderTarget)
    {
    }

    virtual void Build() = 0;

  public:
    void Initialize() override
    {
        if (!built)
        {
            Build();
            built = true;
        }
        ContainerBase::Initialize();
    }

  private:
    bool built = false;
};

} // namespace UI

using View = UI::View;
