#include "UIManager.h"
#include <iostream>
#include <glad/glad.h>

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
            Bounds(200, 200, Anchor::CENTER),
            UI::IdentifierKind::TRANSPARENT,
            HAlign::CENTER,
            Box(Bounds(150, 50), {1.0f, 0.2f, 0.2f, 1.0f}),
            Box(Bounds(100, 50), {0.2f, 1.0f, 0.2f, 1.0f})
        )->SetPadding(10.0f)->SetSpacing(5.0f)->SetColor({0.3f, 0.6f, 1.0f, 0.5f})
         ->SetJustifyContent(UI::JustifyContent::CENTER) // Center content vertically
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

    // Ensure UI exists
    if (!rootContainer) {
        CreateUI(w, h);
        lastWidth = w;
        lastHeight = h;
    }

    // Save GL state
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    GLint oldFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);
    GLboolean oldDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean oldBlend = glIsEnabled(GL_BLEND);

    // Setup GL state for UI rendering
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  // Ensure we render to screen
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, w, h);

    // Draw root container with screen as container size
    glm::vec2 screenSize = {static_cast<float>(w), static_cast<float>(h)};
    rootContainer->Draw(screenSize, {0, 0});

    // Restore GL state
    glBindFramebuffer(GL_FRAMEBUFFER, oldFBO);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    if (oldDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (oldBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
}

}
