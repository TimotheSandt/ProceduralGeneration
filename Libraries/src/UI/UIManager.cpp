#include "UIManager.h"
#include <iostream>

namespace UI {

UIManager& UIManager::Instance() {
    static UIManager instance;
    return instance;
}

void UIManager::Init() {
    // UI will be created on first Render when we have window size
}

void UIManager::CreateUI(int w, int h) {
    // Create root with actual window size (not percentage)
    rootContainer = Container(
        Bounds({static_cast<float>(w), ValueType::PIXEL}, {static_cast<float>(h), ValueType::PIXEL}),
        VBox(
            Bounds({200, ValueType::PIXEL}, {150, ValueType::PIXEL}, Anchor::CENTER),
            HAlign::CENTER,
            Box(Bounds({150, ValueType::PIXEL}, {50, ValueType::PIXEL}), {1.0f, 0.2f, 0.2f, 1.0f}),
            Box(Bounds({100, ValueType::PIXEL}, {50, ValueType::PIXEL}), {0.2f, 1.0f, 0.2f, 1.0f})
        )
    );

    // Set root's size
    rootContainer->SetPixelSize({static_cast<float>(w), static_cast<float>(h)});
    rootContainer->MarkDirty(DirtyType::ALL);
}

void UIManager::Shutdown() {
    rootContainer.reset();
}

void UIManager::Update(float dt, int w, int h) {
    (void)dt;

    // Recreate UI if size changed significantly
    if (!rootContainer || lastWidth != w || lastHeight != h) {
        if (!rootContainer) {
            CreateUI(w, h);
        } else {
            // Just update size
            rootContainer->SetPixelSize({static_cast<float>(w), static_cast<float>(h)});
            rootContainer->MarkDirty(DirtyType::ALL);
        }
        lastWidth = w;
        lastHeight = h;
    }
}

void UIManager::Render(int w, int h) {
    if (!active) return;

    try {
        // Ensure UI exists
        if (!rootContainer) {
            CreateUI(w, h);
            lastWidth = w;
            lastHeight = h;
        }

        // Draw root container with screen as container size
        glm::vec2 screenSize = {static_cast<float>(w), static_cast<float>(h)};
        rootContainer->Draw(screenSize, {0, 0});
    } catch (const std::exception& e) {
        std::cerr << "[UIManager] Render error: " << e.what() << std::endl;
    }
}

}
