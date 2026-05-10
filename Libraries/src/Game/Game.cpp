#include "Game.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Core/WindowContext.h"

namespace
{

#ifdef DEBUG
bool ParsePositiveDouble(std::string_view text, double &outValue)
{
    try
    {
        std::size_t parsedCharacters = 0;
        outValue = std::stod(std::string(text), &parsedCharacters);
        return parsedCharacters == text.size() && outValue > 0.0;
    }
    catch (const std::exception &)
    {
        return false;
    }
}
#endif

WorldMode ParseWorldMode(std::string_view value)
{
    if (value == "current" || value == "terrain")
    {
        return WorldMode::Terrain;
    }
    if (value == "empty")
    {
        return WorldMode::Empty;
    }
    throw std::invalid_argument("Unknown world mode: " + std::string(value));
}

bool IsGameArgumentWithValue(std::string_view argument)
{
#ifdef DEBUG
    return argument == "--world" || argument == "--benchmark" || argument == "--benchmark-seconds" ||
           argument == "--benchmark-log-interval";
#else
    return argument == "--world";
#endif
}

} // namespace

GameLaunchOptions ParseGameLaunchOptions(int argc, const char *const *argv)
{
    GameLaunchOptions options;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);

#ifdef DEBUG
        if (argument == "--no-overlay")
        {
            options.renderOverlay = false;
            continue;
        }
        if (argument == "--no-ui")
        {
            options.renderOverlay = false;
            options.renderUI = false;
            continue;
        }
        if (argument == "--no-profiler")
        {
            options.profileGpu = false;
            continue;
        }
#endif
        if (argument == "--world")
        {
            if (index + 1 >= argc)
            {
                throw std::invalid_argument("Missing value after --world");
            }
            options.worldMode = ParseWorldMode(argv[++index]);
            continue;
        }
        if (argument.rfind("--world=", 0) == 0)
        {
            options.worldMode = ParseWorldMode(argument.substr(8));
            continue;
        }
#ifdef DEBUG
        if (argument == "--benchmark" || argument == "--benchmark-seconds")
        {
            if (index + 1 >= argc || !ParsePositiveDouble(argv[index + 1], options.benchmarkSeconds))
            {
                throw std::invalid_argument("Invalid benchmark duration");
            }
            ++index;
            continue;
        }
        if (argument.rfind("--benchmark=", 0) == 0)
        {
            if (!ParsePositiveDouble(argument.substr(12), options.benchmarkSeconds))
            {
                throw std::invalid_argument("Invalid benchmark duration");
            }
            continue;
        }
        if (argument.rfind("--benchmark-seconds=", 0) == 0)
        {
            if (!ParsePositiveDouble(argument.substr(20), options.benchmarkSeconds))
            {
                throw std::invalid_argument("Invalid benchmark duration");
            }
            continue;
        }
        if (argument == "--benchmark-log-interval")
        {
            if (index + 1 >= argc || !ParsePositiveDouble(argv[index + 1], options.benchmarkLogIntervalSeconds))
            {
                throw std::invalid_argument("Invalid benchmark log interval");
            }
            ++index;
            continue;
        }
        if (argument.rfind("--benchmark-log-interval=", 0) == 0)
        {
            if (!ParsePositiveDouble(argument.substr(25), options.benchmarkLogIntervalSeconds))
            {
                throw std::invalid_argument("Invalid benchmark log interval");
            }
            continue;
        }
#endif

        if (IsGameArgumentWithValue(argument))
        {
            ++index;
        }
    }

    return options;
}

void PrintGameLaunchUsage(std::ostream &out)
{
#ifdef DEBUG
    out << "Game options: [--world current|empty] [--benchmark-seconds <seconds>] [--benchmark-log-interval <seconds>] [--no-overlay] "
           "[--no-ui] [--no-profiler]\n";
    out << "Example benchmark: ProceduralGeneration --api vulkan --world current --benchmark-seconds 8\n";
#else
    out << "Game options: [--world current|empty]\n";
#endif
}

Game::Game(GameLaunchOptions optionsIn) : options(optionsIn)
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
    this->world->Init(options.worldMode);

    this->textRenderer = std::make_unique<UI::TextRenderer>();
    textRenderer->init(*window.GetWidthptr(), *window.GetHeightptr());
    textRenderer->loadFont(GET_RESOURCE_PATH("fonts/Roboto-Regular.ttf"), "default", 48);

    // Initialize UI system
    UI::Manager::Instance().Init(*window.GetWidthptr(), *window.GetHeightptr());
    UI::Manager::Instance().SetActive(options.renderUI);
    LOG_INFO("Game initialized: world=", WorldModeToString(options.worldMode),
             ", overlay=", options.renderOverlay ? "on" : "off", ", ui=", options.renderUI ? "on" : "off",
             ", gpuProfiler=", options.profileGpu ? "on" : "off");
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
#ifdef DEBUG
    this->benchmarkStartTime = std::chrono::steady_clock::now();
    this->benchmarkLastLogTime = this->benchmarkStartTime;
    this->benchmarkFrameTimesMs.clear();
    if (options.benchmarkSeconds > 0.0)
    {
        // Reserve for one log interval at ~4000 FPS headroom to prevent realloc during the run.
        benchmarkFrameTimesMs.reserve(static_cast<std::size_t>(options.benchmarkLogIntervalSeconds * 4000.0));
    }
    this->benchmarkTotalFrames = 0;
#endif

    while (!window.ShouldClose())
    {
#ifdef DEBUG
        const auto frameStart = std::chrono::steady_clock::now();
#endif
        if (!this->window.NewFrame())
        {
            continue;
        }

        this->update();
        if (options.profileGpu)
        {
            Profiler::ProfileGPU("Render", &Game::render, this);
            Profiler::ProfileGPU("SwapBuffers", &Window::SwapBuffers, window);
        }
        else
        {
            this->render();
            window.SwapBuffers();
        }

#ifdef DEBUG
        const auto frameEnd = std::chrono::steady_clock::now();
        this->recordBenchmarkFrame(frameEnd - frameStart);

        if (options.benchmarkSeconds > 0.0)
        {
            const double elapsedSeconds = std::chrono::duration<double>(frameEnd - benchmarkStartTime).count();
            if (elapsedSeconds >= options.benchmarkSeconds)
            {
                this->logBenchmarkSample(elapsedSeconds, true);
                glfwSetWindowShouldClose(this->window.GetWindow(), true);
            }
        }
#endif
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

    this->world->Update();

    if (options.renderUI)
    {
        UI::Manager::Instance().Update(deltaTime, *window.GetWidthptr(), *window.GetHeightptr());
    }
}

void Game::render()
{
    const auto averageTimeMs = [](const char *name) { return static_cast<double>(Profiler::GetAverageTime(name).count()) * 1e-6; };
    const int windowWidth = *window.GetWidthptr();
    const int windowHeight = *window.GetHeightptr();

    renderer3D.SetOutputResolution(windowWidth, windowHeight);
    renderer3D.BeginPass();
    if (options.profileGpu)
    {
        Profiler::ProfileGPU("Clear", &Renderer3D::Clear, &renderer3D, window.GetClearColor(), true);
    }
    else
    {
        renderer3D.Clear(window.GetClearColor(), true);
    }
    renderer3D.SetCamera(this->camera);
    if (options.profileGpu)
    {
        Profiler::ProfileGPU("RenderWorld", &World::Render, this->world.get(), std::ref(renderer3D), std::ref(this->camera));
        Profiler::ProfileGPU("Upscale", &Renderer3D::EndPass, &renderer3D);
    }
    else
    {
        this->world->Render(renderer3D, this->camera);
        renderer3D.EndPass();
    }

    if (options.renderUI || options.renderOverlay)
    {
        rendererUI.SetOutputResolution(windowWidth, windowHeight);
        rendererUI.BeginPass();
        if (options.renderOverlay)
        {
            const glm::vec3 overlayColor(1.0f, 0.8f, 1.0f);
            rendererUI.RenderText(*textRenderer, "fps: " + std::to_string(int(window.GetAverageFPS())), 10, 10, 0.5f, overlayColor,
                                  UI::TextAnchor::TopLeft);
            const std::string statsText =
                std::format("Render: {:.3f}ms\nRender World: {:.3f}ms\nUpscale: {:.3f}ms\nUI Upscale: {:.3f}ms\nSwap Buffers: {:.3f}ms",
                            averageTimeMs("Render"), averageTimeMs("RenderWorld"), averageTimeMs("Upscale"), averageTimeMs("UIUpscale"),
                            averageTimeMs("SwapBuffers"));
            rendererUI.RenderText(*textRenderer, statsText, 10, 50, 0.3f, overlayColor, UI::TextAnchor::TopLeft);
        }
        if (options.renderUI)
        {
            UI::Manager::Instance().Render(rendererUI, windowWidth, windowHeight);
        }
        if (options.profileGpu)
        {
            Profiler::ProfileGPU("UIUpscale", &Renderer2D::EndPass, &rendererUI);
        }
        else
        {
            rendererUI.EndPass();
        }
    }
}

#ifdef DEBUG
void Game::recordBenchmarkFrame(std::chrono::steady_clock::duration frameDuration)
{
    if (options.benchmarkSeconds <= 0.0)
    {
        return;
    }

    ++benchmarkTotalFrames;
    benchmarkFrameTimesMs.push_back(std::chrono::duration<double, std::milli>(frameDuration).count());

    const auto now = std::chrono::steady_clock::now();
    if (options.benchmarkSeconds > 0.0 && std::chrono::duration<double>(now - benchmarkStartTime).count() >= options.benchmarkSeconds)
    {
        return;
    }

    const double elapsedSinceLog = std::chrono::duration<double>(now - benchmarkLastLogTime).count();
    if (elapsedSinceLog >= options.benchmarkLogIntervalSeconds)
    {
        const double elapsedTotal = std::chrono::duration<double>(now - benchmarkStartTime).count();
        this->logBenchmarkSample(elapsedTotal);
        benchmarkLastLogTime = now;
    }
}

void Game::logBenchmarkSample(double elapsedSeconds, bool finalSample)
{
    if (benchmarkFrameTimesMs.empty())
    {
        return;
    }

    std::vector<double> sortedFrameTimes = benchmarkFrameTimesMs;
    std::sort(sortedFrameTimes.begin(), sortedFrameTimes.end(), std::greater<>());
    const std::size_t worstFrameCount = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(sortedFrameTimes.size() * 0.01)));
    double worstFrameTimeMs = 0.0;
    for (std::size_t index = 0; index < worstFrameCount; ++index)
    {
        worstFrameTimeMs += sortedFrameTimes[index];
    }
    worstFrameTimeMs /= static_cast<double>(worstFrameCount);

    double totalFrameTimeMs = 0.0;
    for (const double frameTimeMs : benchmarkFrameTimesMs)
    {
        totalFrameTimeMs += frameTimeMs;
    }

    const double sampleSeconds = totalFrameTimeMs / 1000.0;
    const double averageFps = sampleSeconds > 0.0 ? static_cast<double>(benchmarkFrameTimesMs.size()) / sampleSeconds : 0.0;
    const double onePercentLowFps = worstFrameTimeMs > 0.0 ? 1000.0 / worstFrameTimeMs : 0.0;

    const unsigned int renderedChunks = world != nullptr ? world->GetRenderedChunkCount() : 0;
    const unsigned int renderedTriangles = world != nullptr ? world->GetRenderedTriangleCount() : 0;
    LOG_INFO(finalSample ? "[BENCH FINAL] " : "[BENCH] ", "world=", WorldModeToString(options.worldMode),
             " elapsed=", std::format("{:.2f}s", elapsedSeconds), " frames=", benchmarkTotalFrames,
             " sampleFrames=", benchmarkFrameTimesMs.size(), " avgFPS=", std::format("{:.1f}", averageFps),
             " 1%low=", std::format("{:.1f}", onePercentLowFps), " worstMs=", std::format("{:.3f}", sortedFrameTimes.front()),
             " renderedChunks=", renderedChunks, " renderedTriangles=", renderedTriangles);
    FLUSH_LOG_TO_FILE;

    benchmarkFrameTimesMs.clear();
}
#endif
