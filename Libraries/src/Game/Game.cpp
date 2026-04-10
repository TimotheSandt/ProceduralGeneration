#include "Game.h"

#include <stdexcept>

#include "Graphics/Backends/OpenGL/OpenGLWindowContext.h"
#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"

Game::Game()
{
    if (!IsGraphicsAPIActive(GraphicsAPI::OpenGL))
    {
        throw std::runtime_error("Game currently requires the OpenGL runtime backend");
    }

    LOG_TRACE("Initializing window");
    if (window.Init() != 0)
    {
        throw std::runtime_error("Failed to initialize window");
    }
}
Game::~Game() { this->stop(); }

void Game::init()
{
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
    UI::UIManager::Instance().Init(*window.GetWidthptr(), *window.GetHeightptr());
}

void Game::stop()
{
    if (this->stopped)
    {
        return;
    }
    this->stopped = true;

    if (this->window.GetWindow() != nullptr)
    {
        OpenGLWindowContext::EnsureContextCurrent(this->window.GetWindow());
    }
    if (this->world)
    {
        this->world->Destroy();
        this->world.reset();
    }
    this->textRenderer.reset();
    UI::UIManager::Instance().Shutdown();
    this->camera.Destroy();
    this->window.Close();
}

void Game::run()
{
    while (!window.ShouldClose())
    {

        if (this->window.NewFrame())
        {
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

void Game::processInput() {}

void Game::update()
{
    const double fps = this->window.GetFPS();
    const float deltaTime = fps > 0.0 ? static_cast<float>(1.0 / fps) : 1.0f / 60.0f;
    this->camera.Inputs(this->window.GetWindow(), deltaTime);
    this->camera.UpdateMatrix();

    this->world->Update();

    UI::UIManager::Instance().Update(deltaTime, *window.GetWidthptr(), *window.GetHeightptr());
}

void Game::render()
{
    const auto averageTimeMs = [](const char *name) { return static_cast<double>(Profiler::GetAverageTime(name).count()) * 1e-6; };
    int renderWidth = 0;
    int renderHeight = 0;
    this->window.GetRenderResolution(renderWidth, renderHeight);
    if (renderWidth <= 0 || renderHeight <= 0)
    {
        renderWidth = *window.GetWidthptr();
        renderHeight = *window.GetHeightptr();
    }

    renderer3D.BeginPass(renderWidth, renderHeight);

    Profiler::ProfileGPU("Clear", &Renderer3D::Clear, &renderer3D, window.GetClearColor(), true);
    renderer3D.SetCamera(this->camera);
    Profiler::ProfileGPU("RenderWorld", &World::Render, this->world.get(), std::ref(renderer3D), std::ref(this->camera));
    renderer3D.EndPass();

    renderer2D.BeginPass(renderWidth, renderHeight);
    renderer2D.RenderText(*textRenderer, "fps: " + std::to_string(int(window.GetAverageFPS())), 10, 10, 0.5f, glm::vec3(1.0f, 0.8f, 1.0f),
                          UI::TextAnchor::TopLeft);
    renderer2D.RenderText(*textRenderer, std::format("Render: {:.3f}ms", averageTimeMs("Render")), 10, 50, 0.3f,
                          glm::vec3(1.0f, 0.8f, 1.0f), UI::TextAnchor::TopLeft);
    renderer2D.RenderText(*textRenderer, std::format("Render World: {:.3f}ms", averageTimeMs("RenderWorld")), 10, 70, 0.3f,
                          glm::vec3(1.0f, 0.8f, 1.0f), UI::TextAnchor::TopLeft);
    renderer2D.RenderText(*textRenderer, std::format("Upscale: {:.3f}ms", averageTimeMs("Upscale")), 10, 90, 0.3f,
                          glm::vec3(1.0f, 0.8f, 1.0f), UI::TextAnchor::TopLeft);
    renderer2D.RenderText(*textRenderer, std::format("Swap Buffers: {:.3f}ms", averageTimeMs("SwapBuffers")), 10, 110, 0.3f,
                          glm::vec3(1.0f, 0.8f, 1.0f), UI::TextAnchor::TopLeft);

    UI::UIManager::Instance().Render(renderer2D, *window.GetWidthptr(), *window.GetHeightptr());
    renderer2D.EndPass();
}
