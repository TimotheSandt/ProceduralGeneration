#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp> 


#include "FPSCounter.h"


struct Parameters
{
    int width;
    int height;
    int majorVersion;
    int minorVersion;
    bool IsDepthEnable;
    
    int maxFPS;
    bool vsync;
    bool fullscreen;

    double trueEveryms;

    glm::vec4 clearColor;
};



class Window
{
public:
    Window();
    ~Window();

    int Init();
    void Update();
    void ProcessInput();
    bool NewFrame();
    void SwapBuffers();
    
    void Close();

    int GetWidth() { return this->parameters.width; }
    int GetHeight() { return this->parameters.height; }
    int* GetWidthptr() { return &this->parameters.width; }
    int* GetHeightptr() { return &this->parameters.height; }

    void ChangeDepth(bool IsDepthEnable);
    bool IsDepthEnable() { return this->parameters.IsDepthEnable; }

    bool ShouldClose() { return glfwWindowShouldClose(this->window); }
    double GetAspectRatio () { return (double)this->parameters.width / (double)this->parameters.height; }

    GLFWwindow* GetWindow() { return this->window; }

    int GetFrame() { return this->fpsCounter.getFrame(); }
    void trueEvery(double ms);
    double GetFPS() { return this->fpsCounter.getFPS(); }
    double GetAverageFPS() { return this->fpsCounter.getAverageFPS(); }
    double GetMaxFPS() { return this->fpsCounter.getMaxFPS(); }
    double GetMinFPS() { return this->fpsCounter.getMinFPS(); }

    double GetElapseTimeSecond() { return this->fpsCounter.getElapseTimeInSeconds(); }
    double GetAverageElapseTimeSecond() { return this->fpsCounter.getAverageElapseTimeInSeconds(); }
    double GetMaxElapseTimeSecond() { return this->fpsCounter.getMaxElapseTimeInSeconds(); }
    double GetMinElapseTimeSecond() { return this->fpsCounter.getMinElapseTimeInSeconds(); }
    
    double GetElapseTimeMillisecond() { return this->fpsCounter.getElapseTimeInMilliseconds(); }
    double GetAverageElapseTimeMillisecond() { return this->fpsCounter.getAverageElapseTimeInMilliseconds(); }
    double GetMaxElapseTimeMillisecond() { return this->fpsCounter.getMaxElapseTimeInMilliseconds(); }
    double GetMinElapseTimeMillisecond() { return this->fpsCounter.getMinElapseTimeInMilliseconds(); }

private:
    GLFWwindow* window;

    Parameters parameters;

    FPSCounter fpsCounter;

    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;
};