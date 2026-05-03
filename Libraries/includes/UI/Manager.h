#pragma once
#include "Core/Container.h"
#include "Rendering/TextRenderer.h"
#include "Renderer2D.h"

class Window;

namespace UI
{

class Manager
{
    std::shared_ptr<Container> rootContainer;
    std::shared_ptr<TextRenderer> textRenderer;
    Renderer2D renderer2D;
    const Window *window = nullptr;
    bool active = true;

    int lastWidth = 0;
    int lastHeight = 0;

  public:
    static Manager &Instance();

    void Init(int w, int h, const Window *window = nullptr);
    void Shutdown();
    void CreateUI(int w, int h, const Window *window = nullptr);

    void Update(float dt, int w, int h);
    void Render(int w, int h);

    void SetRootContainer(std::shared_ptr<Container> root) { rootContainer = std::move(root); }
    std::shared_ptr<Container> GetRootContainer() { return rootContainer; }

    std::shared_ptr<TextRenderer> GetTextRenderer() { return textRenderer; }
    void SetRenderScale(float scale) { renderer2D.SetRenderScale(scale); }
    float GetRenderScale() const { return renderer2D.GetRenderScale(); }
    void SetUpscalingEnabled(bool enabled) { renderer2D.SetUpscalingEnabled(enabled); }
    bool IsUpscalingEnabled() const { return renderer2D.IsUpscalingEnabled(); }
    void ToggleUpscaling() { renderer2D.SetUpscalingEnabled(!renderer2D.IsUpscalingEnabled()); }

    void SetActive(bool a) { active = a; }
    bool IsActive() const { return active; }
};

} // namespace UI
