#pragma once
#include "UINavigator.h"

class UIManager {
    UINavigator navigator;
    bool active = true;

public:
    static UIManager& Instance();

    void Init();
    void Shutdown();

    void Update(float dt, InputManager* input, int w, int h);
    void Render(int w, int h);

    UINavigator& GetNavigator() { return navigator; }

    void SetActive(bool a) { active = a; }
    bool IsActive() const { return active; }
};
