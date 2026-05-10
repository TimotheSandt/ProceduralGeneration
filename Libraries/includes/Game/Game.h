#pragma once

#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <vector>

#include "World.h"
#include "Camera.h"
#include "Window.h"
#include "Renderer2D.h"
#include "Renderer3D.h"
#include "Profiler.h"
#include "UIManager.h"
#include "Rendering/TextRenderer.h"
#include "InputManager.h"

struct GameLaunchOptions
{
    WorldMode worldMode = WorldMode::Terrain;
#ifdef DEBUG
    double benchmarkSeconds = 0.0;
    double benchmarkLogIntervalSeconds = 1.0;
#endif
    bool renderOverlay = true;
    bool renderUI = true;
    bool profileGpu = true;
};

GameLaunchOptions ParseGameLaunchOptions(int argc, const char *const *argv);
void PrintGameLaunchUsage(std::ostream &out);

class Game
{
  public:
    explicit Game(GameLaunchOptions options = {});
    ~Game();

    void init();
    void stop();

    void run();

  private:
    void processInput();
    void update();
    void render();
#ifdef DEBUG
    void recordBenchmarkFrame(std::chrono::steady_clock::duration frameDuration);
    void logBenchmarkSample(double elapsedSeconds, bool finalSample = false);
#endif

  private:
    GameLaunchOptions options;
    Window window;
    Renderer3D renderer3D;
    Renderer2D rendererUI;

    Camera camera;
    std::unique_ptr<World> world = nullptr;
    std::unique_ptr<UI::TextRenderer> textRenderer;
    bool stopped = false;
#ifdef DEBUG
    std::chrono::steady_clock::time_point benchmarkStartTime;
    std::chrono::steady_clock::time_point benchmarkLastLogTime;
    std::vector<double> benchmarkFrameTimesMs;
    std::uint64_t benchmarkTotalFrames = 0;
#endif
};
