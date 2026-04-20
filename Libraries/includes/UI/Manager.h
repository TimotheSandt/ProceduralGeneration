#pragma once
#include "Core/Container.h"
#include "Rendering/TextRenderer.h"

class Renderer2D;
class Window;

namespace UI
{

class Manager
{
    std::shared_ptr<Container> rootContainer;
    std::shared_ptr<ContainerBase> performanceOverlay;
    std::shared_ptr<TextRenderer> textRenderer;
    const Window *window = nullptr;
    bool active = true;

    int lastWidth = 0;
    int lastHeight = 0;

  public:
    static Manager &Instance();

    void Init(int w, int h, const Window *window = nullptr);
    void Shutdown();
    void CreateUI(int w, int h, const Window *window = nullptr);
    void SetPerformanceStats(double renderTimeMs, double renderWorldTimeMs, double upscaleTimeMs, double uiUpscaleTimeMs,
                             double swapBuffersTimeMs);

    void Update(float dt, int w, int h);
    void Render(Renderer2D &renderer2D, int w, int h);

    void SetRootContainer(std::shared_ptr<Container> root) { rootContainer = std::move(root); }
    std::shared_ptr<Container> GetRootContainer() { return rootContainer; }

    std::shared_ptr<TextRenderer> GetTextRenderer() { return textRenderer; }

    void SetActive(bool a) { active = a; }
    bool IsActive() const { return active; }
};

} // namespace UI
