#pragma once

#include "View.h"
#include "Window.h"
#include "Widgets/Text.h"
#include "Profiler.h"

#include <chrono>

class Window;

namespace UI
{

class PerformanceView : public View
{
  public:
    explicit PerformanceView(Bounds bounds, const Window *window = nullptr) : View(bounds), window(window) {}

  protected:
    std::shared_ptr<UI::ContainerBase> Build() override {
      return CreateVBox(Bounds(260_px, 160_px, Anchor::TOP_LEFT), {
        CreateText(Bounds(), TextContent("FPS: ", Bind(window, &Window::GetAverageFPS)), 0.50f),
        CreateText(Bounds(), TextContent("Render: ", Bind(&Profiler::GetAverageTime, "Render")), 0.30f),
        CreateText(Bounds(), TextContent("Render World: ", Bind(&Profiler::GetAverageTime, "RenderWorld")), 0.30f),
        CreateText(Bounds(), TextContent("Upscale: ", Bind(&Profiler::GetAverageTime, "Upscale")), 0.30f),
        CreateText(Bounds(), TextContent("UI Upscale: ", Bind(&Profiler::GetAverageTime, "UIUpscale")), 0.30f),
        CreateText(Bounds(), TextContent("Swap Buffers: ", Bind(&Profiler::GetAverageTime, "SwapBuffers")), 0.30f),
      })
      ->SetPadding(12.0f)
      ->SetSpacing(4.0f)
      ->SetColor(glm::vec4{0.05f, 0.05f, 0.08f, 0.65f});
    }

  private:
    const Window *window = nullptr;
};

inline std::shared_ptr<PerformanceView> CreatePerformanceView(Bounds bounds = Bounds(), const Window *window = nullptr)
{
    return std::make_shared<PerformanceView>(bounds, window);
}

} // namespace UI
