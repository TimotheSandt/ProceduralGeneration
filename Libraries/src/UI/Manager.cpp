#include "Manager.h"
#include "Layout/HBox.h"
#include "Layout/VBox.h"
#include "Utilities.h"
#include <iostream>
#include <glad/glad.h>

namespace UI
{

Manager &Manager::Instance()
{
    static Manager instance;
    return instance;
}

void Manager::Init(int w, int h)
{
    textRenderer = std::make_shared<TextRenderer>();
    textRenderer->init(static_cast<unsigned int>(w), static_cast<unsigned int>(h));
    textRenderer->loadFont(GET_RESOURCE_PATH("fonts/Roboto-Regular.ttf"), "default", 48);

    CreateUI(w, h);
    lastWidth = w;
    lastHeight = h;
}

void Manager::CreateUI(int w, int h)
{
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
             ->SetChildAlignment(UI::HAlign::CENTER)});

    // Set root's size

    rootContainer->SetIdentifierKind(UI::IdentifierKind::TRANSPARENT);
    rootContainer->Initialize();
}

void Manager::Shutdown() { rootContainer.reset(); }

void Manager::Update(float dt, int w, int h)
{
    (void)dt;

    // Ensure UI is initialized
    if (!rootContainer)
    {
        // Fallback if Init wasn't called or failed
        Init(w, h);
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

void Manager::Render(int w, int h)
{
    if (!active)
    {
        return;
    }

    // Ensure UI exists
    if (!rootContainer)
    {
        Init(w, h);
    }

    // Save GL state
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    GLint oldFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);
    GLboolean oldDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean oldBlend = glIsEnabled(GL_BLEND);

    // Setup GL state for UI rendering
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Ensure we render to screen
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
    if (oldDepthTest)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    if (oldBlend)
    {
        glEnable(GL_BLEND);
    }
    else
    {
        glDisable(GL_BLEND);
    }
}

} // namespace UI
