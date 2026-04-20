#include "Manager.h"
#include "Renderer2D.h"
#include "Layout/HBox.h"
#include "Layout/VBox.h"
#include "Widgets.h"
#include "Window.h"

#include <utility>

namespace UI
{

Manager &Manager::Instance()
{
    static Manager instance;
    return instance;
}

void Manager::Init(int w, int h, const Window *windowArg)
{
    window = windowArg;
    textRenderer = std::make_shared<TextRenderer>();
    textRenderer->init(static_cast<unsigned int>(w), static_cast<unsigned int>(h));
    textRenderer->loadFont(GET_RESOURCE_PATH("fonts/Roboto-Regular.ttf"), "default", 48);

    CreateUI(w, h, window);
    lastWidth = w;
    lastHeight = h;
}

void Manager::CreateUI(int w, int h, const Window *windowArg)
{
    window = windowArg;

    const auto makeMetricLine = [](TextContent content, float scale)
    {
        return CreateText(Bounds(), std::move(content), scale);
    };

    performanceOverlay = CreateVBox(Bounds(260_px, 160_px, Anchor::TOP_LEFT),
                                    {
                                        makeMetricLine(std::move(fpsContent), 0.45f),
                                        makeMetricLine(TextContent("Render: ", Bind(window->profiler, &Window::averageTimeMs), " ms"), 0.30f),
                                        makeMetricLine(TextContent("Render World: ").AppendValue(&renderWorldTimeMs).AppendText(" ms"), 0.30f),
                                        makeMetricLine(TextContent("Upscale: ").AppendValue(&upscaleTimeMs).AppendText(" ms"), 0.30f),
                                        makeMetricLine(TextContent("UI Upscale: ").AppendValue(&uiUpscaleTimeMs).AppendText(" ms"), 0.30f),
                                        makeMetricLine(TextContent("Swap Buffers: ").AppendValue(&swapBuffersTimeMs).AppendText(" ms"), 0.30f),
                                    })
                              ->SetPadding(12.0f)
                              ->SetSpacing(4.0f)
                              ->SetColor(glm::vec4{0.05f, 0.05f, 0.08f, 0.65f});

    // Create root with actual window size (not percentage)
    rootContainer = CreateContainer(
        Bounds({static_cast<float>(w), ValueType::PIXEL}, {static_cast<float>(h), ValueType::PIXEL}),
        {
            CreateVBox(Bounds(200_px, 200_px, Anchor::CENTER), {
                CreateBox(Bounds(150_px, 50_px), {1.0f, 0.2f, 0.2f, 1.0f}),
                CreateHBox(Bounds(150_px, 75_px), {
                    CreateBox(Bounds(40_pct, 100_pct), {0.2f, 0.2f, 1.0f, 1.0f}),
                    CreateBox(Bounds(40_pct, 100_pct), {1.0f, 0.2f, 0.2f, 1.0f}),
                    CreateBox(Bounds(40_pct, 100_pct), {0.2f, 1.0f, 0.2f, 1.0f})})
                        ->SetColor(glm::vec4{0.3f, 0.9f, 0.4f, 1.0f})
                        ->SetPadding(10.0f)
                        ->SetJustifyContent(UI::JustifyContent::CENTER)
                        ->SetOverflowMode(UI::OverflowMode::WRAP)
                        ->SetChildrenDeform(true)
                        ->SetChildAlignment(UI::VAlign::CENTER),
                    CreateBox(Bounds(100_px, 50_px), {0.2f, 1.0f, 0.2f, 1.0f})
                })
             ->SetPadding(10.0f)
             ->SetSpacing(5.0f)
             ->SetColor(glm::vec4{0.3f, 0.6f, 1.0f, 0.5f})
             ->SetJustifyContent(UI::JustifyContent::CENTER)
             ->SetChildAlignment(UI::HAlign::CENTER),
            performanceOverlay});

    // Set root's size

    rootContainer->SetIdentifierKind(UI::IdentifierKind::TRANSPARENT);
    rootContainer->Initialize();
}

void Manager::SetPerformanceStats(double renderMs, double renderWorldMs, double upscaleMs, double uiUpscaleMs, double swapBuffersMs)
{
    renderTimeMs = renderMs;
    renderWorldTimeMs = renderWorldMs;
    upscaleTimeMs = upscaleMs;
    uiUpscaleTimeMs = uiUpscaleMs;
    swapBuffersTimeMs = swapBuffersMs;

    if (performanceOverlay)
    {
        performanceOverlay->Update();
    }
}

void Manager::Shutdown()
{
    rootContainer.reset();
    performanceOverlay.reset();
    textRenderer.reset();
    window = nullptr;
}

void Manager::Update(float dt, int w, int h)
{
    (void)dt;

    // Ensure UI is initialized
    if (!rootContainer)
    {
        // Fallback if Init wasn't called or failed
        Init(w, h, window);
    }

    // Only update layout if size changed
    if (lastWidth != w || lastHeight != h)
    {
        rootContainer->SetSize({static_cast<float>(w), static_cast<float>(h)});
        if (textRenderer)
            textRenderer->updateScreenSize(static_cast<unsigned int>(w), static_cast<unsigned int>(h));
        lastWidth = w;
        lastHeight = h;
    }

    // Update UI state (applies deferred values, recalculates layout)
    rootContainer->Update();
}

void Manager::Render(Renderer2D &renderer2D, int w, int h)
{
    if (!active)
    {
        return;
    }

    // Ensure UI exists
    if (!rootContainer)
    {
        Init(w, h, window);
    }

    renderer2D.BeginCanvasPass();

    // Draw root container with screen as container size
    glm::vec2 screenSize = {static_cast<float>(w), static_cast<float>(h)};
    rootContainer->Draw(screenSize, {0, 0});
    renderer2D.EndCanvasPass();
}

} // namespace UI
