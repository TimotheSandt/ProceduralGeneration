#include "UIManager.h"
#include "UIHBox.h"
#include "UIVBox.h"
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
    rootContainer = Container( Bounds({static_cast<float>(w), ValueType::PIXEL}, {static_cast<float>(h), ValueType::PIXEL}),
    {
        VBox(Bounds(200_px, 200_px, Anchor::CENTER),
        {
            Box(Bounds(150_px, 50_px), {1.0f, 0.2f, 0.2f, 1.0f}),
            HBox(Bounds(150_px, 75_px),
            {
                Box(Bounds(40_pct, 100_pct), {0.2f, 0.2f, 1.0f, 1.0f}),
                Box(Bounds(40_pct, 100_pct), {1.0f, 0.2f, 0.2f, 1.0f})
            })
                ->SetColor(glm::vec4{0.3f, 0.9f, 0.4f, 1.0f})
                ->SetJustifyContent(UI::JustifyContent::CENTER)
                ->SetChildAlignment(UI::VAlign::CENTER),
            Box(Bounds(100_px, 50_px), {0.2f, 1.0f, 0.2f, 1.0f})
        })  ->SetPadding(10.0f)
            ->SetSpacing(5.0f)
            ->SetColor(glm::vec4{0.3f, 0.6f, 1.0f, 0.5f})
            ->SetJustifyContent(UI::JustifyContent::CENTER)
            ->SetChildAlignment(UI::HAlign::CENTER)
    });

    // Set root's size

    rootContainer->SetIdentifierKind(UI::IdentifierKind::TRANSPARENT);
    rootContainer->SetPixelSize({static_cast<float>(w), static_cast<float>(h)});
    rootContainer->Initialize();
    rootContainer->Update();
}

void UIManager::Shutdown() {
    rootContainer.reset();
}

void UIManager::Update(float dt, int w, int h) {
    (void)dt;

    if (!rootContainer) {
        CreateUI(w, h);
    }

    if (w <= 0 || h <= 0) return;

    if (lastWidth != w || lastHeight != h) {
        rootContainer->SetPixelSize({static_cast<float>(w), static_cast<float>(h)});
        lastWidth = w;
        lastHeight = h;
    }

    rootContainer->Update();
}

void UIManager::Render(int w, int h) {
    if (!active) return;
    if (w <= 0 || h <= 0) return;

    // DEBUG: Checks
    GLint vpBefore[4];
    glGetIntegerv(GL_VIEWPORT, vpBefore);
    // LOG_INFO("Viewport BEFORE UI: ", vpBefore[0], ", ", vpBefore[1], ", ", vpBefore[2], ", ", vpBefore[3]);

    // Save GL state
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    GLint oldFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);
    GLboolean oldDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLboolean oldCullFace = glIsEnabled(GL_CULL_FACE);

    // Setup GL state for UI rendering
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  // Ensure we render to screen
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // FORCE Viewport for UI
    glViewport(0, 0, w, h);

    // Draw root container with screen as container size
    rootContainer->Draw({0, 0});

    // Restore GL state
    glBindFramebuffer(GL_FRAMEBUFFER, oldFBO);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    if (oldDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (oldBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (oldCullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

    // DEBUG: Verify restoration
    GLint vpAfter[4];
    glGetIntegerv(GL_VIEWPORT, vpAfter);
    // LOG_INFO("Viewport AFTER UI: ", vpAfter[0], ", ", vpAfter[1], ", ", vpAfter[2], ", ", vpAfter[3]);
}

}
