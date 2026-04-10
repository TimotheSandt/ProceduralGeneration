#pragma once
#include "Core/Container.h"
#include "Rendering/TextRenderer.h"

namespace UI
{

class Manager
{
    std::shared_ptr<Container> rootContainer;
    std::shared_ptr<TextRenderer> textRenderer;
    bool active = true;

    int lastWidth = 0;
    int lastHeight = 0;

  public:
    static Manager &Instance();

    void Init(int w, int h);
    void Shutdown();
    void CreateUI(int w, int h);

    void Update(float dt, int w, int h);
    void Render(int w, int h);

    void SetRootContainer(std::shared_ptr<Container> root) { rootContainer = std::move(root); }
    std::shared_ptr<Container> GetRootContainer() { return rootContainer; }

    std::shared_ptr<TextRenderer> GetTextRenderer() { return textRenderer; }

    void SetActive(bool a) { active = a; }
    bool IsActive() const { return active; }
};

} // namespace UI
