#pragma once
#include "UIComponent.h"
#include "UIContainer.h"
#include <stack>


class UINavigator {
    std::stack<std::unique_ptr<UIPage>> pageStack;
    std::vector<std::unique_ptr<UIOverlay>> overlays;

public:
    void PushPage(std::unique_ptr<UIPage> page);
    void PopPage();
    void ReplacePage(std::unique_ptr<UIPage> page);

    void ShowOverlay(std::unique_ptr<UIOverlay> overlay);
    void CloseOverlay(UIOverlay* overlay);
    void CloseAllOverlays();

    void Update(float dt);
    void HandleInput(InputManager* input, int w, int h);
    void Draw(int w, int h);
};
