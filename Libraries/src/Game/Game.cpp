#include "Game.h"

Game::Game() {
    LOG_TRACE("Initializing window");
    window.Init();
}
Game::~Game() {
    this->stop();
}

void Game::init() {
    int *w = window.GetWidthptr();
	int *h = window.GetHeightptr();

	this->camera.Initialize(w, h, glm::vec3(0.0f, 1.0f, 0.0f));
    this->camera.SetFOV(75.0f);
    this->camera.SetNearPlane(0.1f);
    this->camera.SetFarPlane(1000.0f);

    this->world = std::make_unique<World>();
    this->world->Init();

    this->textRenderer = std::make_unique<UI::TextRenderer>();
    textRenderer->init(*window.GetWidthptr(), *window.GetHeightptr());
    textRenderer->loadFont(GET_RESOURCE_PATH("fonts/Roboto-Regular.ttf"), "default", 48);

    // Initialize UI system
    UI::UIManager::Instance().Init();
}

void Game::stop() {
    glfwMakeContextCurrent(this->window.GetWindow());
    this->world->Destroy();
    this->world.reset();
    this->camera.Destroy();
    this->window.Close();
}

void Game::run() {
    glGetError();
    while (!window.ShouldClose()) {

        if (this->window.NewFrame()) {
            // std::string title = "fps: " + std::to_string(window.GetFPS()) +
            //                     ", Avg fps: " + std::to_string(window.GetAverageFPS()) +
            //                     ", Avg Elapsed Time: " + std::to_string(window.GetAverageElapseTimeMillisecond()) + "ms" +
            //                     ", Render Time: " + std::to_string(Profiler::GetAverageTime("Render").count() * 1e-6) + "ms" +
            //                     ", Upscale Time: " + std::to_string(Profiler::GetAverageTime("Upscale").count() * 1e-6) + "ms" +
            //                     ", Swap Buffers Time: " + std::to_string(Profiler::GetAverageTime("SwapBuffers").count() * 1e-6) + "ms";
            // glfwSetWindowTitle(window.GetWindow(), title.c_str());
        }

        this->update();
        Profiler::ProfileGPU("Render", &Game::render, this);

        Profiler::ProfileGPU("SwapBuffers", &Window::SwapBuffers, window);
    }
}

void Game::processInput() {

}

void Game::update() {
    this->camera.Inputs(this->window.GetWindow(), 1.0 / this->window.GetFPS());
    this->camera.UpdateMatrix();

    this->world->Update();

    UI::UIManager::Instance().Update(1.0f / this->window.GetFPS(), *window.GetWidthptr(), *window.GetHeightptr());
}

void Game::render() {
    Profiler::ProfileGPU("Clear", &Window::Clear, window);
    this->camera.BindUBO();
    Profiler::ProfileGPU("RenderWorld", &World::Render, this->world.get(), this->camera);

    textRenderer->updateScreenSize(*window.GetWidthptr(), *window.GetHeightptr());
    textRenderer->renderText("fps: " + std::to_string(int(window.GetAverageFPS())), 10, 10, 0.5f,
        glm::vec3(1.0f, 0.8f, 1.0f), UI::TextAnchor::TopLeft);
    textRenderer->renderText(std::format("Render: {:.3f}ms", Profiler::GetAverageTime("Render").count() * 1e-6), 10, 50, 0.3f,
        glm::vec3(1.0f, 0.8f, 1.0f), UI::TextAnchor::TopLeft);
    textRenderer->renderText(std::format("Render World: {:.3f}ms", Profiler::GetAverageTime("RenderWorld").count() * 1e-6), 10, 70, 0.3f,
        glm::vec3(1.0f, 0.8f, 1.0f), UI::TextAnchor::TopLeft);
    textRenderer->renderText(std::format("Upscale: {:.3f}ms", Profiler::GetAverageTime("Upscale").count() * 1e-6), 10, 90, 0.3f,
        glm::vec3(1.0f, 0.8f, 1.0f), UI::TextAnchor::TopLeft);
    textRenderer->renderText(std::format("Swap Buffers: {:.3f}ms", Profiler::GetAverageTime("SwapBuffers").count() * 1e-6), 10, 110, 0.3f,
        glm::vec3(1.0f, 0.8f, 1.0f), UI::TextAnchor::TopLeft);

    // Render UI
    UI::UIManager::Instance().Render(*window.GetWidthptr(), *window.GetHeightptr());
}
