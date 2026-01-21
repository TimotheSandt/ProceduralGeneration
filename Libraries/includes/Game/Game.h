#pragma once

#include <memory>

#include "World.h"
#include "Camera.h"
#include "Window.h"
// #include "UIManager.h"
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
    Camera camera;
    std::unique_ptr<World> world = nullptr;
};
