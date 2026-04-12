#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdint>
#include <string>

#include <glm/vec4.hpp>

#include "FPSCounter.h"

class InputManager;

enum WindowState : std::uint8_t
{
    WINDOWED,
    BORDERLESS,
    FULLSCREEN,
    FULLSCREEN_UNFOCUSED
};

struct WindowParameters
{
    // Window
    std::string title;
    WindowState windowState;
    int width, height;
    int posX, posY;

    // Timing
    int maxFPS;
    bool vsync;

    // Color
    glm::vec4 clearColor;
    double trueEveryms;

    // Windowed
    int windowedWidth, windowedHeight;
    int windowedPosX, windowedPosY;

    bool taskbarVisible; // TODO
};

class Window
{
  public:
    Window();
    Window(std::string title, int width, int height);
    Window(std::string title, int width, int height, WindowState windowState);
    Window(WindowParameters parameters);

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    Window(Window &&) noexcept;
    Window &operator=(Window &&) noexcept;

    ~Window();

    int Init();
    bool NewFrame();
    void SwapBuffers();
    void Close();

    // Window state
    void ChangeWindowState(WindowState state);
    void ToggleFullscreen();
    void ToggleBorderless();

    // Setters
    void SetClearColor(glm::vec4 color) { this->parameters.clearColor = color; }
    void SetMaxFPS(int fps) { this->parameters.maxFPS = fps; }
    void SetTrueEvery(double ms) { this->parameters.trueEveryms = ms; }

    // Getters
    int GetWidth() const { return this->parameters.width; }
    int GetHeight() const { return this->parameters.height; }
    glm::vec4 GetClearColor() const { return this->parameters.clearColor; }
    int *GetWidthptr() { return &this->parameters.width; }
    int *GetHeightptr() { return &this->parameters.height; }
    bool ShouldClose() const { return glfwWindowShouldClose(this->window); }
    double GetAspectRatio() const { return (double)this->parameters.width / (double)this->parameters.height; }
    GLFWwindow *GetWindow() const { return this->window; }

    // Performance
    int GetFrame() const { return this->fpsCounter.getFrame(); }
    double GetFPS() const { return this->fpsCounter.getFPS(); }
    double GetAverageFPS() const { return this->fpsCounter.getAverageFPS(); }
    double GetMaxFPS() const { return this->fpsCounter.getMaxFPS(); }
    double GetMinFPS() const { return this->fpsCounter.getMinFPS(); }

    double GetElapseTimeSecond() const { return this->fpsCounter.getElapseTimeInSeconds(); }
    double GetAverageElapseTimeSecond() const { return this->fpsCounter.getAverageElapseTimeInSeconds(); }
    double GetMaxElapseTimeSecond() const { return this->fpsCounter.getMaxElapseTimeInSeconds(); }
    double GetMinElapseTimeSecond() const { return this->fpsCounter.getMinElapseTimeInSeconds(); }

    double GetElapseTimeMillisecond() const { return this->fpsCounter.getElapseTimeInMilliseconds(); }
    double GetAverageElapseTimeMillisecond() const { return this->fpsCounter.getAverageElapseTimeInMilliseconds(); }
    double GetMaxElapseTimeMillisecond() const { return this->fpsCounter.getMaxElapseTimeInMilliseconds(); }
    double GetMinElapseTimeMillisecond() const { return this->fpsCounter.getMinElapseTimeInMilliseconds(); }

  private:
    void Swap(Window &other) noexcept;

    // Window state
    void SaveWindowedParameters();
    void PostWindowStateChange() const;

    void ActivateFullscreen();
    void ActivateWindowed();
    void ActivateBorderless();

    // Callbacks
    void SetupCallbacks();
    void CallbackFocus(GLFWwindow *window, int focused);
    void CallbackResize(GLFWwindow *window, int width, int height);
    void CallbackPosition(GLFWwindow *window, int x, int y);

    void ClearCallbacks();
    bool IsWindowHealthy() const;

  private:
    GLFWwindow *window = nullptr;

    WindowParameters parameters;

    InputManager *inputManager;

    FPSCounter fpsCounter;

    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;
};
