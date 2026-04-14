#pragma once

#include <memory>

#include "World.h"
#include "Camera.h"
#include "Window.h"
#include "Renderer2D.h"
#include "Renderer3D.h"
#include "Profiler.h"
#include "UIManager.h"
#include "Rendering/TextRenderer.h"
#include "InputManager.h"

class Game
{
  public:
    Game();
    ~Game();

    void init();
    void stop();

    void run();

  private:
    void processInput();
    void update();
    void render();

  private:
    Window window;
    Renderer3D renderer3D;
    Renderer2D rendererUI;
    
    Camera camera;
    std::unique_ptr<World> world = nullptr;
    std::unique_ptr<UI::TextRenderer> textRenderer;
    bool stopped = false;
};
