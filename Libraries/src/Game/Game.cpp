#include "Game.h"

#include <format>
#include <stdexcept>
#include <string>

#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Core/WindowContext.h"

Game::Game()
{
    if (!IsGraphicsAPIActive(GraphicsAPI::OpenGL) && !IsGraphicsAPIActive(GraphicsAPI::Vulkan))
    {
        throw std::runtime_error("Game currently requires the OpenGL or Vulkan runtime backend");
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
    UI::Manager::Instance().Init(*window.GetWidthptr(), *window.GetHeightptr());
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
        GraphicsWindowContext::EnsureContextReady(this->window.GetWindow());
    }
    if (this->world)
    {
        this->world->Destroy();
        this->world.reset();
    }
    this->textRenderer.reset();
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

    rendererUI.SetOutputResolution(windowWidth, windowHeight);
    rendererUI.BeginPass();
    const glm::vec3 overlayColor(1.0f, 0.8f, 1.0f);
    rendererUI.RenderText(*textRenderer, "fps: " + std::to_string(int(window.GetAverageFPS())), 10, 10, 0.5f, overlayColor,
                          UI::TextAnchor::TopLeft);
    const std::string statsText =
        std::format("Render: {:.3f}ms\nRender World: {:.3f}ms\nUpscale: {:.3f}ms\nUI Upscale: {:.3f}ms\nSwap Buffers: {:.3f}ms",
                    averageTimeMs("Render"), averageTimeMs("RenderWorld"), averageTimeMs("Upscale"), averageTimeMs("UIUpscale"),
                    averageTimeMs("SwapBuffers"));
    rendererUI.RenderText(*textRenderer, statsText, 10, 50, 0.3f, overlayColor, UI::TextAnchor::TopLeft);
    UI::Manager::Instance().Render(rendererUI, windowWidth, windowHeight);
    Profiler::ProfileGPU("UIUpscale", &Renderer2D::EndPass, &rendererUI);
}
