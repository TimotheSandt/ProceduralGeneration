#pragma once
#include "UIContainer.h"

namespace UI {

class UIManager {
    std::shared_ptr<UIContainer> rootContainer;
    bool active = true;

    int lastWidth = 0;
    int lastHeight = 0;

public:
    static UIManager& Instance();

    void Init(int w, int h);
    void Shutdown();
    void CreateUI(int w, int h);

    void Update(float dt, int w, int h);
    void Render(int w, int h);

    void SetRootContainer(std::shared_ptr<UIContainer> root) { rootContainer = root; }
    std::shared_ptr<UIContainer> GetRootContainer() { return rootContainer; }

    void SetActive(bool a) { active = a; }
    bool IsActive() const { return active; }
};

}
