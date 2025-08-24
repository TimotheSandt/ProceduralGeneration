#include "Game.h"

Game::Game() {
    window.Init(); 
}
Game::~Game() { 
    window.Close();
}

void Game::init() {
    int *w = window.GetWidthptr();
	int *h = window.GetHeightptr();
	this->camera = Camera(w, h, glm::vec3(0.0f, 1.0f, 0.0f));

    this->world.Init();
}

void Game::run() {
    while (!window.ShouldClose()) {
        this->window.NewFrame();

        std::string title = "fps: " + std::to_string(window.GetFPS()) + 
							", min fps: " + std::to_string(window.GetMinFPS()) +
							", max fps: " + std::to_string(window.GetMaxFPS()) +
							", Avg fps: " + std::to_string(window.GetAverageFPS()) +
							", Elapsed Time: " + std::to_string(window.GetElapseTimeMillisecond()) + "ms" +
							", Avg Elapsed Time: " + std::to_string(window.GetAverageElapseTimeMillisecond()) + "ms";
		glfwSetWindowTitle(window.GetWindow(), title.c_str());

        update();
        render();


        this->window.SwapBuffers();
    }
}

void Game::processInput() {

}

void Game::update() {
    this->camera.Inputs(this->window.GetWindow(), 1.0 / this->window.GetFPS());
    this->camera.updateMatrix(75.0f, 0.1f, 1000.0f);

    this->world.Update();
    this->window.Update();
}

void Game::render() {

    this->world.Render(this->camera);

}