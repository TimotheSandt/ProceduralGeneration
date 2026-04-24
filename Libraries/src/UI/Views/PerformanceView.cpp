#include "Views/PerformanceView.h"

#include "Profiler.h"
#include "Layout/VBox.h"
#include "Widgets.h"
#include "Window.h"

#include <chrono>
#include <memory>
#include <utility>

namespace UI
{

PerformanceView::PerformanceView(Bounds bounds, const Window *windowArg)
    : View(bounds), window(windowArg)
{
}

double PerformanceView::ToMilliseconds(std::chrono::nanoseconds duration)
{
    return static_cast<double>(duration.count()) * 1e-6;
}

double PerformanceView::GetAverageTimeMs(const char *name) const
{
    return ToMilliseconds(Profiler::GetAverageTime(name));
}

double PerformanceView::GetRenderTimeMs() const
{
    return GetAverageTimeMs("Render");
}

double PerformanceView::GetRenderWorldTimeMs() const
{
    return GetAverageTimeMs("RenderWorld");
}

double PerformanceView::GetUpscaleTimeMs() const
{
    return GetAverageTimeMs("Upscale");
}

double PerformanceView::GetUiUpscaleTimeMs() const
{
    return GetAverageTimeMs("UIUpscale");
}

double PerformanceView::GetSwapBuffersTimeMs() const
{
    return GetAverageTimeMs("SwapBuffers");
}

void PerformanceView::Build()
{
    color.ForceSet(glm::vec4{0.05f, 0.05f, 0.08f, 0.65f});

    auto content = CreateVBox(Bounds(100_pct, 100_pct, Anchor::TOP_LEFT));
    content->SetColor(glm::vec4{0.0f, 0.0f, 0.0f, 0.0f})
        ->SetPadding(12.0f)
        ->SetSpacing(4.0f)
        ->SetChildAlignment(HAlign::LEFT)
        ->SetJustifyContent(JustifyContent::START);

    TextContent fpsContent = window != nullptr ? TextContent("FPS: ", Bind(window, &Window::GetAverageFPS))
                                                : TextContent("FPS: --");
    content->AddChild(CreateText(Bounds(), std::move(fpsContent), 0.5f));

    const auto makeMetricLine = [this](const char *label, double (PerformanceView::*getter)() const)
    {
        TextContent content(label, Bind(this, getter), " ms");
        return CreateText(Bounds(), std::move(content), 1.0f / 3.0f);
    };

    content->AddChild(makeMetricLine("Render: ", &PerformanceView::GetRenderTimeMs));
    content->AddChild(makeMetricLine("Render World: ", &PerformanceView::GetRenderWorldTimeMs));
    content->AddChild(makeMetricLine("Upscale: ", &PerformanceView::GetUpscaleTimeMs));
    content->AddChild(makeMetricLine("UI Upscale: ", &PerformanceView::GetUiUpscaleTimeMs));
    content->AddChild(makeMetricLine("Swap Buffers: ", &PerformanceView::GetSwapBuffersTimeMs));

    AddChild(content);
}

} // namespace UI
