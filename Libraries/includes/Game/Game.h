#pragma once

#include "World.h"
#include "Camera.h"
#include "Window.h"



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
    World world;
};