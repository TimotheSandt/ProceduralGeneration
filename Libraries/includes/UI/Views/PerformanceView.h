#pragma once

#include "View.h"

#include <chrono>
#include <memory>

class Window;

namespace UI
{

class PerformanceView : public View
{
  public:
    explicit PerformanceView(Bounds bounds, const Window *window = nullptr);

  protected:
    void Build() override;

  private:
    const Window *window = nullptr;

    static double ToMilliseconds(std::chrono::nanoseconds duration);
    double GetAverageTimeMs(const char *name) const;
    double GetRenderTimeMs() const;
    double GetRenderWorldTimeMs() const;
    double GetUpscaleTimeMs() const;
    double GetUiUpscaleTimeMs() const;
    double GetSwapBuffersTimeMs() const;
};

inline std::shared_ptr<PerformanceView> CreatePerformanceView(Bounds bounds = Bounds(), const Window *window = nullptr)
{
    return std::make_shared<PerformanceView>(bounds, window);
}

} // namespace UI
