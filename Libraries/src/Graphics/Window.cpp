#include "Window.h"

#include <iostream>

Window::Window() {
    this->window = nullptr;
    this->parameters.width = 800;
    this->parameters.height = 600;
    this->parameters.majorVersion = 3;
    this->parameters.minorVersion = 3;
    this->parameters.IsDepthEnable = true;
    this->parameters.maxFPS = 60;
    this->parameters.vsync = false;
    this->parameters.fullscreen = false;
    this->parameters.clearColor = glm::vec4(0.07f, 0.13f, 0.17f, 1.0f);
    this->parameters.trueEveryms = 500;
    this->lastTime = this->fpsCounter.getLastTime();
}

Window::~Window() {
    this->Close();
}

int Window::Init() {
    // Initialize GLFW
    glfwInit();

    // Tell GLFW what version of OpenGL we want to use
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, this->parameters.majorVersion);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, this->parameters.minorVersion);
    // Tell GLFW we are using the core profile (Only the modern functions will be available)
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    // Center the window
    glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);


    // Create a window of size 800x800 and called "OpenGL"
    this->window = glfwCreateWindow(this->parameters.width, this->parameters.height, "OpenGL", NULL, NULL);
    if (this->window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    // Make the window's context current
    glfwMakeContextCurrent(this->window);

    // Initialize GLAD to load all OpenGL function pointers
    gladLoadGL();
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Define the viewport dimensions
    glViewport(0, 0, this->parameters.width, this->parameters.height);
    glfwWindowHint(GLFW_REFRESH_RATE, this->parameters.maxFPS);
    glfwSwapInterval(this->parameters.vsync);

    
    glEnable(GL_DEPTH_TEST);

    return 0;
}


void Window::Close() {
    glfwDestroyWindow(this->window);
    glfwTerminate();
}

void Window::Update() {
    int w, h;
    glfwGetWindowSize(this->window, &w, &h);

    if (w != this->parameters.width || h != this->parameters.height) {
        this->parameters.width = w;
        this->parameters.height = h;
        glViewport(0, 0, this->parameters.width, this->parameters.height);
    }
}

bool Window::NewFrame() {
    this->fpsCounter.newFrame(this->parameters.maxFPS);
    glfwPollEvents();
    this->ProcessInput();
    this->Update();

    
    glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (this->parameters.trueEveryms == 0) {
        this->fpsCounter.updateStat();
        return true;
    }
    auto now = this->fpsCounter.getTime();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->lastTime).count() >= this->parameters.trueEveryms) {
        this->fpsCounter.updateStat();
        this->lastTime = now;
        return true;
    }
    
    return false;
}

void Window::ProcessInput() {
    if (glfwGetKey(this->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(this->window, true);
    }
}

void Window::SwapBuffers() {
    glfwSwapBuffers(this->window);
}


void Window::ChangeDepth(bool IsDepthEnable) {
    this->parameters.IsDepthEnable = IsDepthEnable;
    if (IsDepthEnable) {
        glEnable(GL_DEPTH_TEST);
    }
    else {
        glDisable(GL_DEPTH_TEST);
    }
}



void Window::trueEvery(double ms) {
    this->parameters.trueEveryms = ms;
}