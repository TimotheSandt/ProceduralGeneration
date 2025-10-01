#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp> 

#include "FBO.h"

#include "FPSCounter.h"
#include "Profiler.h"

enum WindowState { WINDOWED, BORDERLESS, FULLSCREEN, FULLSCREEN_UNFOCUSED};

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
    

    // Upscaling
    float renderScale;
    int renderWidth, renderHeight;
    bool enableUpscaling;
};



class Window
{
public:
    Window();
    Window(std::string title, int width, int height);
    Window(std::string title, int width, int height, WindowState windowState);
    Window(WindowParameters parameters);

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    ~Window();

    int Init();
    bool NewFrame();
    void SwapBuffers();
    void Close();

    static bool InitOpenGL();
    static void TerminateOpenGL();

    void Clear() const;


    // Window state
    void ChangeWindowState(WindowState state);
    void ToggleFullscreen();
    void ToggleBorderless();

    // Resolution Scaling methods
    void SetRenderScale(float scale);
    void EnableUpscaling(bool enable);
    float GetRenderScale() const { return parameters.renderScale; }
    void GetRenderResolution(int& width, int& height) const {
        width = parameters.renderWidth;
        height = parameters.renderHeight;
    }

    // Setters
    void SetClearColor(glm::vec4 color) { this->parameters.clearColor = color; }
    void SetMaxFPS(int fps) { this->parameters.maxFPS = fps; }
    void SetTrueEvery(double ms) { this->parameters.trueEveryms = ms; }

    // Getters
    int GetWidth() const { return this->parameters.width; }
    int GetHeight() const { return this->parameters.height; }
    int* GetWidthptr() { return &this->parameters.width; }
    int* GetHeightptr() { return &this->parameters.height; }
    bool ShouldClose() const { return glfwWindowShouldClose(this->window); }
    double GetAspectRatio() const { return (double)this->parameters.width / (double)this->parameters.height; }
    GLFWwindow* GetWindow() const { return this->window; }


    // Performance
    int GetFrame() const { return this->fpsCounter.getFrame(); }
    double GetFPS()  const{ return this->fpsCounter.getFPS(); }
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
    void Swap(Window& other) noexcept;

    // Window state
    void SaveWindowedParameters();
    void PostWindowStateChange() const;
    
    void ActivateFullscreen();
    void ActivateWindowed();
    void ActivateBorderless();


    // Resolution Scaling methods
    void InitFBOs();
    void BindRenderFBO() const;
    void UnbindRenderFBO() const;
    void UpdateFBOResotution();



    // Callbacks
    void SetupCallbacks();
    void CallbackInput(GLFWwindow* window, int action, int key);
    void CallbackFocus(GLFWwindow* window, int focused);
    void CallbackResize(GLFWwindow* window, int width, int height);
    void CallbackPosition(GLFWwindow* window, int x, int y);

    void ClearCallbacks();

    // Error handling
    static void SetupErrorHandling();
    bool IsWindowHealthy() const;

private:
    GLFWwindow* window = nullptr;

    FBO FBORendering;
    FBO FBOUpscaled;

    WindowParameters parameters;

    FPSCounter fpsCounter;

    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;

    static bool isOpenGLInitialized;
    static GLint GLFW_MAJOR_VERSION;
    static GLint GLFW_MINOR_VERSION;
};