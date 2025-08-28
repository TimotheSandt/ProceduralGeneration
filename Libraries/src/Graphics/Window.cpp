#include "Window.h"

#include <iostream>
#include "PriorityHelper.h"

Window::Window() {
    this->window = nullptr;
    this->parameters.title = "Window";
    this->parameters.width = 800;
    this->parameters.height = 600;
    this->parameters.posX = 100;
    this->parameters.posY = 100;
    this->parameters.majorVersion = 3;
    this->parameters.minorVersion = 3;
    this->parameters.maxFPS = 60;
    this->parameters.vsync = false;
#ifdef DEBUG
    this->parameters.windowState = WindowState::WINDOWED;
#else
    this->parameters.windowState = WindowState::FULLSCREEN;
#endif // DEBUG
    this->parameters.clearColor = glm::vec4(0.07f, 0.13f, 0.17f, 1.0f);
    this->parameters.trueEveryms = 500;
    this->lastTime = this->fpsCounter.getLastTime();

    // Windowed
    this->parameters.windowedWidth = this->parameters.width;
    this->parameters.windowedHeight = this->parameters.height;
    this->parameters.windowedPosX = this->parameters.posX;
    this->parameters.windowedPosY = this->parameters.posY;

    // Initialize resolution scaling parameters
    this->parameters.renderScale = 1.0f;
    this->parameters.enableUpscaling = false;
    this->parameters.renderWidth = this->parameters.width;
    this->parameters.renderHeight = this->parameters.height;
}

Window::~Window() {
    this->Close();
}

int Window::Init() {
    // Set High Priority
    if (PriorityHelper::RequestHighPriority()) {
        PriorityHelper::ApplyPerformanceTweaks();
        PriorityHelper::DisplayCurrentPriority();
    }


    // Initialize GLFW
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, this->parameters.majorVersion);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, this->parameters.minorVersion);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

#ifdef DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif


    // Create a window of size 800x800 and called "OpenGL"
    this->window = glfwCreateWindow(this->parameters.width, this->parameters.height, this->parameters.title.c_str(), NULL, NULL);
    if (!this->window) {
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
        this->Close();
        return -1;
    }

    // Define the viewport dimensions
    glViewport(0, 0, this->parameters.width, this->parameters.height);
    glfwSwapInterval(this->parameters.vsync ? 1 : 0);
    
    glEnable(GL_DEPTH_TEST);
    

    glfwGetWindowPos(this->window, &this->parameters.posX, &this->parameters.posY);

    glfwSetWindowUserPointer(this->window, this);

    this->SetupCallbacks();
    this->SetupErrorHandling();
    

    this->ChangeWindowState(this->parameters.windowState);
    this->InitFBOs();

    return 0;
}

void Window::Close() {
    if (!this->window) return;

    glfwDestroyWindow(this->window);
    this->window = nullptr;
    glfwTerminate();

#ifdef _WIN32
    ShowWindow(FindWindowA("Shell_TrayWnd", NULL), SW_SHOW);
#endif
}

void Window::Clear() {
    glClearColor(
        this->parameters.clearColor.r, 
        this->parameters.clearColor.g, 
        this->parameters.clearColor.b, 
        this->parameters.clearColor.a
    );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
}


bool Window::NewFrame() {
    Profiler::Profile("PollEvents", &glfwPollEvents);

    if (!Profiler::Profile("HealthCheck", &Window::IsWindowHealthy, this)) {
        std::cerr << "Window is not healthy" << std::endl;
        return false;
    }

    this->fpsCounter.newFrame(this->parameters.maxFPS);

    if (this->parameters.enableUpscaling) {
        this->StartRenderFBO();
    }
    
    if (this->parameters.trueEveryms == 0) {
        this->fpsCounter.updateStat();
        return true;
    }

    auto now = this->fpsCounter.getTime();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->lastTime).count();
    if (elapsed >= this->parameters.trueEveryms) {
        this->fpsCounter.updateStat();
        this->lastTime = now;
        return true;
    }
    
    return false;
}

void Window::SwapBuffers() {
    if (!this->window) return;

    Profiler::Process();

    if (this->parameters.enableUpscaling) {
        Profiler::ProfileGPU("Upscale", &Window::EndRenderFBO, this);
    }

    glfwSwapBuffers(this->window);
}



///////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Window state functions ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

void Window::ChangeWindowState(WindowState state) {
    switch (state) {
        case WindowState::FULLSCREEN:
        case WindowState::FULLSCREEN_UNFOCUSED:
            this->ActivateFullscreen();
            break;
        case WindowState::BORDERLESS:
            this->ActivateBorderless();
            break;
        case WindowState::WINDOWED:
        default:
            this->ActivateWindowed();
            break;
    }
}


void Window::ToggleFullscreen() {
    if (this->parameters.windowState == WindowState::FULLSCREEN || 
        this->parameters.windowState == WindowState::FULLSCREEN_UNFOCUSED) {
        this->ActivateWindowed();
    } else {
        this->ActivateFullscreen();
    }
}

void Window::ToggleBorderless() {
    if (this->parameters.windowState == WindowState::BORDERLESS) {
        this->ActivateWindowed();
    } else {
        this->ActivateBorderless();
    }
}


void Window::ActivateFullscreen() {
    if (this->parameters.windowState == WindowState::FULLSCREEN) return;

    if (!IsWindowHealthy()) {
        std::cout << "Window not healthy, skipping fullscreen toggle" << std::endl;
        return;
    }

    if (this->parameters.windowState == WindowState::WINDOWED) {
        this->SaveWindowedParameters();
    }
    
    // Get primary monitor
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        std::cout << "Failed to get primary monitor" << std::endl;
        return;
    }
    
    // Get monitor's video mode
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        std::cout << "Failed to get video mode" << std::endl;
        return;
    }

    // Switch to fullscreen
    glfwSetWindowMonitor(this->window, monitor, 0, 0, 
        mode->width, mode->height, mode->refreshRate);
    
    // Update internal parameters
    this->parameters.width = mode->width;
    this->parameters.height = mode->height;
    this->parameters.windowState = WindowState::FULLSCREEN;
    
    
    PostWindowStateChange();
    std::cout << "Switched to fullscreen: " << mode->width << "x" << mode->height 
                << " @ " << mode->refreshRate << "Hz" << std::endl;
}

void Window::ActivateWindowed() {
    if (this->parameters.windowState == WindowState::WINDOWED) return;

    if (!IsWindowHealthy()) {
        std::cout << "Window not healthy, skipping windowed toggle" << std::endl;
        return;
    }

#ifdef _WIN32
    ShowWindow(FindWindowA("Shell_TrayWnd", NULL), SW_SHOW);
#endif

    

    glfwSetWindowMonitor(this->window, nullptr, this->parameters.windowedPosX, this->parameters.windowedPosY,
            this->parameters.windowedWidth, this->parameters.windowedHeight, GLFW_DONT_CARE);
    glfwSetWindowAttrib(this->window, GLFW_DECORATED, GLFW_TRUE);

    // Update internal parameters
    this->parameters.width = this->parameters.windowedWidth;
    this->parameters.height = this->parameters.windowedHeight;
    this->parameters.windowState = WindowState::WINDOWED;
    
    
    PostWindowStateChange();
    std::cout << "Switched to windowed: " << this->parameters.windowedWidth << "x" << this->parameters.windowedHeight << std::endl;
}


void Window::ActivateBorderless() {
    if (this->parameters.windowState == WindowState::BORDERLESS) return;
    if (!IsWindowHealthy()) {
        std::cout << "Window not healthy, skipping fullscreen toggle" << std::endl;
        return;
    }

    if (this->parameters.windowState == WindowState::WINDOWED) {
        SaveWindowedParameters();
    }

    // Get primary monitor
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        std::cout << "Failed to get primary monitor" << std::endl;
        return;
    }

    // Get monitor's video mode
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        std::cout << "Failed to get video mode" << std::endl;
        return;
    }

    int width = mode->width;
    int height = mode->height + 1;

    // On Windows, we need to hide the taskbar
#ifdef _WIN32
    ShowWindow(FindWindowA("Shell_TrayWnd", NULL), SW_HIDE);
#endif

    

    glfwSetWindowAttrib(this->window, GLFW_DECORATED, GLFW_FALSE);
    glfwSetWindowMonitor(this->window, nullptr, 0, 0, width, height, GLFW_DONT_CARE);
    
    // Update internal parameters
    this->parameters.width = width;
    this->parameters.height = height;
    this->parameters.windowState = WindowState::BORDERLESS;
    

    PostWindowStateChange();
    std::cout << "Switched to borderless: " << width << "x" << height << std::endl;
}

void Window::SaveWindowedParameters() {
    if (!this->window) return;

    glfwGetWindowSize(this->window, &this->parameters.windowedWidth, 
                     &this->parameters.windowedHeight);
    glfwGetWindowPos(this->window, &this->parameters.windowedPosX, 
                    &this->parameters.windowedPosY);
}


void Window::PostWindowStateChange() {
    if (!this->window) return;
    glfwMakeContextCurrent(this->window);

    glfwSwapInterval(this->parameters.vsync ? 1 : 0);

    glEnable(GL_DEPTH_TEST);

    glClearColor(
        this->parameters.clearColor.r, 
        this->parameters.clearColor.g, 
        this->parameters.clearColor.b, 
        this->parameters.clearColor.a
    );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(this->window);

    glfwFocusWindow(this->window);
}






///////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////        CALLBACK        ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////


void Window::SetupCallbacks() {
    glfwSetKeyCallback(this->window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        UNREFERENCED_PARAMETER(scancode);
        UNREFERENCED_PARAMETER(mods);

        try {
            Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (!windowObj) return;
            windowObj->CallbackInput(window, action, key);
            
        }
        catch (const std::exception& e) {
            std::cerr << "Error in key callback: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown error in key callback" << std::endl;
        }
    });
    
    // Callback pour gérer la perte de focus (peut aider avec la touche Windows)
    glfwSetWindowFocusCallback(this->window, [](GLFWwindow* window, int focused) {
        try {
            Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (!windowObj) return;
            windowObj->CallbackFocus(window, focused);
        }
        catch (...) {
            // Ignorer les erreurs dans ce callback
        }
    });

    // Callback d'erreur GLFW pour diagnostiquer les problèmes
    glfwSetErrorCallback([](int error_code, const char* description) {
        std::cerr << "GLFW Error " << error_code << ": " << description << std::endl;
    });
    
    glfwSetWindowSizeCallback(this->window, [](GLFWwindow* window, int width, int height) {
        try {
            Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (!windowObj) return;
            windowObj->CallbackResize(window, width, height);
        }
        catch (...) {
            // Ignorer les erreurs dans ce callback
        }
    });


    glfwSetWindowPosCallback(this->window, [](GLFWwindow* window, int x, int y) {
        try {
            Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (!windowObj) return;
            windowObj->CallbackPosition(window, x, y);
        }
        catch (...) {
            // Ignorer les erreurs dans ce callback
        }
    });
}


void Window::CallbackInput(GLFWwindow* window, int action, int key) {
    if (key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER || 
        key < 0 || key > GLFW_KEY_LAST) {
        return;
    }
    

    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
            case GLFW_KEY_F11:
                this->ToggleBorderless();
                break;

#ifdef DEBUG
            case GLFW_KEY_F12:
                this->ToggleFullscreen();
                break;
            
            // Resolution
            case GLFW_KEY_1:
                this->SetRenderScale(0.25f);  // 25%
                break;
            case GLFW_KEY_2:
                this->SetRenderScale(0.5f);   // 50%
                break;
            case GLFW_KEY_3:
                this->SetRenderScale(0.75f);  // 75%
                break;
            case GLFW_KEY_4:
                this->SetRenderScale(1.0f);   // 100% (native)
                break;
            case GLFW_KEY_5:
                this->EnableUpscaling(!parameters.enableUpscaling);
                break;

#endif
        }
    }
}


void Window::CallbackResize(GLFWwindow* window, int width, int height) {
    UNREFERENCED_PARAMETER(window);

    this->parameters.width = width;
    this->parameters.height = height;

    UpdateFBOResotution();

    glViewport(0, 0, this->parameters.width, this->parameters.height);
}

void Window::CallbackPosition(GLFWwindow* window, int x, int y) {
    UNREFERENCED_PARAMETER(window);

    this->parameters.posX = x;
    this->parameters.posY = y;
}

void Window::CallbackFocus(GLFWwindow* window, int focused) {
    UNREFERENCED_PARAMETER(window);

    if (!focused) {
        std::cout << "Window lost focus" << std::endl;
        if (this->parameters.windowState == WindowState::FULLSCREEN) { 
            std::this_thread::sleep_for(std::chrono::milliseconds(75));
            this->ActivateBorderless();
            this->parameters.windowState = WindowState::FULLSCREEN_UNFOCUSED;
        }
#ifdef _WIN32
        // Rendre la barre de tâches visible
        ShowWindow(FindWindowA("Shell_TrayWnd", NULL), SW_SHOW);
#endif
    } else {
        std::cout << "Window regained focus" << std::endl;
        if (this->parameters.windowState == WindowState::FULLSCREEN || this->parameters.windowState == WindowState::FULLSCREEN_UNFOCUSED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(75));
            this->ActivateFullscreen();
        }
#ifdef _WIN32
        else if (this->parameters.windowState == WindowState::BORDERLESS) {
            ShowWindow(FindWindowA("Shell_TrayWnd", NULL), SW_HIDE);
        }
#endif
    }
}






///////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////     ERROR HANDLING     ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

void Window::SetupErrorHandling() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | 
                 SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX);
#endif
    
    // Callback d'erreur GLFW global
    glfwSetErrorCallback([](int error_code, const char* description) {
        std::cerr << "GLFW Error " << error_code << ": " << description << std::endl;
        
        // Log plus détaillé pour certaines erreurs critiques
        switch (error_code) {
            case GLFW_INVALID_ENUM:
                std::cerr << "Invalid enum parameter" << std::endl;
                break;
            case GLFW_INVALID_VALUE:
                std::cerr << "Invalid value parameter" << std::endl;
                break;
            case GLFW_OUT_OF_MEMORY:
                std::cerr << "Out of memory" << std::endl;
                break;
            case GLFW_API_UNAVAILABLE:
                std::cerr << "API unavailable" << std::endl;
                break;
            case GLFW_VERSION_UNAVAILABLE:
                std::cerr << "Version unavailable" << std::endl;
                break;
            case GLFW_PLATFORM_ERROR:
                std::cerr << "Platform error" << std::endl;
                break;
            case GLFW_FORMAT_UNAVAILABLE:
                std::cerr << "Format unavailable" << std::endl;
                break;
        }
    });
}


// Fonction pour vérifier l'état de la fenêtre
bool Window::IsWindowHealthy() {
    if (!this->window) {
        return false;
    }
    
    // Vérifier si le contexte OpenGL est toujours valide
    GLFWwindow* currentContext = glfwGetCurrentContext();
    if (currentContext != this->window) {
        std::cout << "Warning: OpenGL context mismatch" << std::endl;
        glfwMakeContextCurrent(this->window);
    }
    
#ifdef DEBUG
    // Vérifier les erreurs OpenGL*
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "OpenGL Error detected: " << error << std::endl;
        return false;
    }
#endif
    
    return true;
}