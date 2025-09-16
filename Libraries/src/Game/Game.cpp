#include "Game.h"

Game::Game() {
    window.Init(); 
}
Game::~Game() { 
    this->stop();
}

void Game::init() {
    int *w = window.GetWidthptr();
	int *h = window.GetHeightptr();
	this->camera.initialize(w, h, glm::vec3(0.0f, 1.0f, 0.0f));

    this->world.Init();
}

void Game::stop() {
    this->window.Close();
}

void Game::run() {
    glGetError();
    while (!window.ShouldClose()) {
        
        if (this->window.NewFrame()) {
            std::string title = "fps: " + std::to_string(window.GetFPS()) + 
                                ", Avg fps: " + std::to_string(window.GetAverageFPS()) +
                                ", Avg Elapsed Time: " + std::to_string(window.GetAverageElapseTimeMillisecond()) + "ms" +
                                ", Render Time: " + std::to_string(Profiler::GetAverageTime("Render").count() * 1e-6) + "ms" +
                                ", Upscale Time: " + std::to_string(Profiler::GetAverageTime("Upscale").count() * 1e-6) + "ms" +
                                ", Swap Buffers Time: " + std::to_string(Profiler::GetAverageTime("SwapBuffers").count() * 1e-6) + "ms";
            glfwSetWindowTitle(window.GetWindow(), title.c_str());
        }

        Profiler::ProfileGPU("Render", &Game::render, this);
        this->update();

        Profiler::ProfileGPU("SwapBuffers", &Window::SwapBuffers, window);
    }
}

void Game::processInput() {

}

void Game::update() {
    this->camera.Inputs(this->window.GetWindow(), 1.0 / this->window.GetFPS());
    this->camera.updateMatrix(75.0f, 0.1f, 1000.0f);

    this->world.Update();
}

void Game::render() {
    Profiler::ProfileGPU("Clear", &Window::Clear, window);
    this->camera.BindUBO();
    this->world.Render(this->camera);
}