#include "Game.h"

#include <stdexcept>

#include "Graphics/Backends/OpenGL/OpenGLWindowContext.h"
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

    // Initialize UI system
    UI::Manager::Instance().Init(*window.GetWidthptr(), *window.GetHeightptr(), &window);
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
    UI::Manager::Instance().Shutdown();
    this->camera.Destroy();
    this->window.Close();
}

void Game::run()
{
    while (!window.ShouldClose())
    {
        if (!this->window.NewFrame())
        {
            continue;
        }

        this->update();
        Profiler::ProfileGPU("Render", &Game::render, this);

        Profiler::ProfileGPU("SwapBuffers", &Window::SwapBuffers, window);
    }
}

void Game::processInput()
{
    InputManager &inputManager = InputManager::GetInstance(this->window.GetWindow());
    if (inputManager.IsKeyJustPressed(KeyButton::ESCAPE))
    {
        glfwSetWindowShouldClose(this->window.GetWindow(), true);
    }
    if (inputManager.IsKeyJustPressed(KeyButton::F11))
    {
        this->window.ToggleBorderless();
    }
#ifdef DEBUG
    if (inputManager.IsKeyJustPressed(KeyButton::F12))
    {
        this->window.ToggleFullscreen();
    }
    if (inputManager.IsKeyJustPressed(KeyButton::NUM_1))
    {
        renderer3D.SetRenderScale(0.25f);
        renderer3D.SetUpscalingEnabled(true);
    }
    if (inputManager.IsKeyJustPressed(KeyButton::NUM_2))
    {
        renderer3D.SetRenderScale(0.5f);
        renderer3D.SetUpscalingEnabled(true);
    }
    if (inputManager.IsKeyJustPressed(KeyButton::NUM_3))
    {
        renderer3D.SetRenderScale(0.75f);
        renderer3D.SetUpscalingEnabled(true);
    }
    if (inputManager.IsKeyJustPressed(KeyButton::NUM_4))
    {
        renderer3D.SetUpscalingEnabled(!renderer3D.IsUpscalingEnabled());
    }
    if (inputManager.IsKeyJustPressed(KeyButton::NUM_5))
    {
        rendererUI.SetRenderScale(0.25f);
    }
    if (inputManager.IsKeyJustPressed(KeyButton::NUM_6))
    {
        rendererUI.SetRenderScale(0.5f);
    }
    if (inputManager.IsKeyJustPressed(KeyButton::NUM_7))
    {
        rendererUI.SetRenderScale(0.75f);
    }
    if (inputManager.IsKeyJustPressed(KeyButton::NUM_8))
    {
        rendererUI.SetUpscalingEnabled(!rendererUI.IsUpscalingEnabled());
    }
#endif
}

void Game::update()
{
    this->processInput();

    const double fps = this->window.GetFPS();
    const float deltaTime = fps > 0.0 ? static_cast<float>(1.0 / fps) : 1.0f / 60.0f;
    this->camera.Inputs(this->window.GetWindow(), deltaTime);
    this->camera.UpdateMatrix();

    this->world->Update();

    UI::Manager::Instance().Update(deltaTime, *window.GetWidthptr(), *window.GetHeightptr());
}

void Game::render()
{
    const auto averageTimeMs = [](const char *name) { return static_cast<double>(Profiler::GetAverageTime(name).count()) * 1e-6; };
    const int windowWidth = *window.GetWidthptr();
    const int windowHeight = *window.GetHeightptr();

    renderer3D.SetOutputResolution(windowWidth, windowHeight);
    renderer3D.BeginPass();
    Profiler::ProfileGPU("Clear", &Renderer3D::Clear, &renderer3D, window.GetClearColor(), true);
    renderer3D.SetCamera(this->camera);
    Profiler::ProfileGPU("RenderWorld", &World::Render, this->world.get(), std::ref(renderer3D), std::ref(this->camera));
    Profiler::ProfileGPU("Upscale", &Renderer3D::EndPass, &renderer3D);

    UI::Manager::Instance().SetPerformanceStats(averageTimeMs("Render"), averageTimeMs("RenderWorld"), averageTimeMs("Upscale"),
                                                averageTimeMs("UIUpscale"), averageTimeMs("SwapBuffers"));

    rendererUI.SetOutputResolution(windowWidth, windowHeight);
    rendererUI.BeginPass();
    UI::Manager::Instance().Render(rendererUI, windowWidth, windowHeight);
    Profiler::ProfileGPU("UIUpscale", &Renderer2D::EndPass, &rendererUI);
}
