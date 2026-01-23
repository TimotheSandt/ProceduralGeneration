#include "Window.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "Logger.h"
#include "utilities.h"


bool Window::isOpenGLInitialized = false;
GLint Window::GLFW_MAJOR_VERSION = 4;
GLint Window::GLFW_MINOR_VERSION = 3;

Window::Window() {
    this->window = nullptr;
    this->parameters.title = "Window";
    this->parameters.width = 800;
    this->parameters.height = 600;
    this->parameters.posX = 100;
    this->parameters.posY = 100;
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

Window::Window(Window&& other) noexcept {
    this->Swap(other);
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        this->Close();
        this->Swap(other);
    }
    return *this;
}

void Window::Swap(Window& other) noexcept {
    std::swap(this->window, other.window);
    std::swap(this->FBORendering, other.FBORendering);
    std::swap(this->FBOUpscaled, other.FBOUpscaled);
    std::swap(this->parameters, other.parameters);
    std::swap(this->fpsCounter, other.fpsCounter);
    std::swap(this->lastTime, other.lastTime);
    std::swap(this->isOpenGLInitialized, other.isOpenGLInitialized);
}


int Window::Init() {
    // Create a window of size 800x800 and called "OpenGL"
    this->window = glfwCreateWindow(this->parameters.width, this->parameters.height, this->parameters.title.c_str(), NULL, NULL);
    if (!this->window) {
        LOG_ERROR(1, "Failed to create GLFW window");
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(this->window);

    // Initialize GLAD to load all OpenGL function pointers
    gladLoadGL();
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_FATAL(-1, "Failed to initialize GLAD");
        this->Close();
        return -1;
    }
    GL_CHECK_ERROR_M("gladLoadGL");

    // Define the viewport dimensions
    glViewport(0, 0, this->parameters.width, this->parameters.height);
    GL_CHECK_ERROR_M("glViewport");

    glfwSwapInterval(this->parameters.vsync ? 1 : 0);
    GL_CHECK_ERROR_M("glfwSwapInterval");

    glEnable(GL_DEPTH_TEST);
    GL_CHECK_ERROR_M("glEnable");


    glfwGetWindowPos(this->window, &this->parameters.posX, &this->parameters.posY);
    GL_CHECK_ERROR_M("glfwGetWindowPos");

    glfwSetWindowUserPointer(this->window, this);
    GL_CHECK_ERROR_M("glfwSetWindowUserPointer");

    this->SetupCallbacks();

    this->ChangeWindowState(this->parameters.windowState);
    this->InitFBOs();

    this->inputManager = &InputManager::GetInstance(this->window);

    return 0;
}

void Window::Close() {
    if (!this->window) return;

    this->ClearCallbacks();

    this->FBORendering.Destroy();
    this->FBOUpscaled.Destroy();

    InputManager::RemoveInstance(this->window);
    this->inputManager = nullptr;

    glfwDestroyWindow(this->window);
    this->window = nullptr;

#ifdef _WIN32
    ShowWindow(FindWindowA("Shell_TrayWnd", NULL), SW_SHOW);
#endif
}

bool Window::InitOpenGL() {
    if (isOpenGLInitialized) return isOpenGLInitialized;


    // Initialize GLFW
    if (!glfwInit()) {
        LOG_FATAL(-1, "Failed to initialize GLFW");
        return isOpenGLInitialized = false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GLFW_MAJOR_VERSION);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GLFW_MINOR_VERSION);
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

    SetupErrorHandling();

    return isOpenGLInitialized = true;
}


void Window::TerminateOpenGL() {
    if (!isOpenGLInitialized) return;
    glfwSetErrorCallback(nullptr);
    glfwTerminate();
    isOpenGLInitialized = false;
}


void Window::Clear() const {
    glClearColor(
        this->parameters.clearColor.r,
        this->parameters.clearColor.g,
        this->parameters.clearColor.b,
        this->parameters.clearColor.a
    );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GL_CHECK_ERROR_M("glClear");
}


bool Window::NewFrame() {
    Profiler::Profile("PollEvents", &glfwPollEvents);
    Profiler::Process();
    this->inputManager->Update();
    this->HandleInput();

    if (!Profiler::Profile("HealthCheck", &Window::IsWindowHealthy, this)) {
        LOG_WARNING("Window is not healthy");
        return false;
    }

    this->fpsCounter.newFrame(this->parameters.maxFPS);

    if (this->parameters.enableUpscaling) {
        this->BindRenderFBO();
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

    if (this->parameters.enableUpscaling) {
        Profiler::ProfileGPU("Upscale", &Window::UnbindRenderFBO, this);
    }

    glfwSwapBuffers(this->window);
    GL_CHECK_ERROR_M("glfwSwapBuffers");
}


void Window::HandleInput() {
    if (this->inputManager->IsKeyJustPressed(KeyButton::ESCAPE)) {
        glfwSetWindowShouldClose(window, true);
    }
    if (this->inputManager->IsKeyJustPressed(KeyButton::F11)) {
        this->ToggleBorderless();
    }

#ifdef DEBUG
    if (this->inputManager->IsKeyJustPressed(KeyButton::F12)) {
        this->ToggleFullscreen();
    }
    if (this->inputManager->IsKeyJustPressed(KeyButton::NUM_1)) {
        this->SetRenderScale(0.25f);  // 25%
    }
    if (this->inputManager->IsKeyJustPressed(KeyButton::NUM_2)) {
        this->SetRenderScale(0.5f);   // 50%
    }
    if (this->inputManager->IsKeyJustPressed(KeyButton::NUM_3)) {
        this->SetRenderScale(0.75f);  // 75%
    }
    if (this->inputManager->IsKeyJustPressed(KeyButton::NUM_4)) {
        this->SetRenderScale(1.0f);   // 100% (native)
    }
    if (this->inputManager->IsKeyJustPressed(KeyButton::NUM_5)) {
        this->EnableUpscaling(!parameters.enableUpscaling);
    }
#endif
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
        LOG_WARNING("Window not healthy, skipping fullscreen toggle");
        return;
    }

    if (this->parameters.windowState == WindowState::WINDOWED) {
        this->SaveWindowedParameters();
    }

    // Get primary monitor
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        LOG_ERROR(1, "Failed to get primary monitor");
        return;
    }

    // Get monitor's video mode
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        LOG_ERROR(1, "Failed to get video mode");
        return;
    }

    // Switch to fullscreen
    glfwSetWindowMonitor(this->window, monitor, 0, 0,
        mode->width, mode->height, mode->refreshRate);
    GL_CHECK_ERROR_M("glfwSetWindowMonitor");

    // Update internal parameters
    this->parameters.width = mode->width;
    this->parameters.height = mode->height;
    this->parameters.windowState = WindowState::FULLSCREEN;


    PostWindowStateChange();
    LOG_DEBUG("Switched to fullscreen: ", mode->width, "x", mode->height, " @ ", mode->refreshRate, "Hz");
}

void Window::ActivateWindowed() {
    if (this->parameters.windowState == WindowState::WINDOWED) return;

    if (!IsWindowHealthy()) {
        LOG_WARNING("Window not healthy, skipping windowed toggle");
        return;
    }

#ifdef _WIN32
    ShowWindow(FindWindowA("Shell_TrayWnd", NULL), SW_SHOW);
#endif



    glfwSetWindowMonitor(this->window, nullptr, this->parameters.windowedPosX, this->parameters.windowedPosY,
            this->parameters.windowedWidth, this->parameters.windowedHeight, GLFW_DONT_CARE);
    GL_CHECK_ERROR_M("glfwSetWindowMonitor");
    glfwSetWindowAttrib(this->window, GLFW_DECORATED, GLFW_TRUE);
    GL_CHECK_ERROR_M("glfwSetWindowAttrib");

    // Update internal parameters
    this->parameters.width = this->parameters.windowedWidth;
    this->parameters.height = this->parameters.windowedHeight;
    this->parameters.windowState = WindowState::WINDOWED;


    PostWindowStateChange();
    LOG_DEBUG("Switched to windowed: ", this->parameters.windowedWidth, "x", this->parameters.windowedHeight);
}


void Window::ActivateBorderless() {
    if (this->parameters.windowState == WindowState::BORDERLESS) return;
    if (!IsWindowHealthy()) {
        LOG_WARNING("Window not healthy, skipping borderless toggle");
        return;
    }

    if (this->parameters.windowState == WindowState::WINDOWED) {
        SaveWindowedParameters();
    }

    // Get primary monitor
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        LOG_ERROR(1, "Failed to get primary monitor");
        return;
    }

    // Get monitor's video mode
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        LOG_ERROR(1, "Failed to get video mode");
        return;
    }

    int width = mode->width;
    int height = mode->height + 1;

    // On Windows, we need to hide the taskbar
#ifdef _WIN32
    ShowWindow(FindWindowA("Shell_TrayWnd", NULL), SW_HIDE);
#endif



    glfwSetWindowAttrib(this->window, GLFW_DECORATED, GLFW_FALSE);
    GL_CHECK_ERROR_M("glfwSetWindowAttrib");
    glfwSetWindowMonitor(this->window, nullptr, 0, 0, width, height, GLFW_DONT_CARE);
    GL_CHECK_ERROR_M("glfwSetWindowMonitor");

    // Update internal parameters
    this->parameters.width = width;
    this->parameters.height = height;
    this->parameters.windowState = WindowState::BORDERLESS;


    PostWindowStateChange();
    LOG_DEBUG("Switched to borderless: ", width, "x", height);
}

void Window::SaveWindowedParameters() {
    if (!this->window) return;

    glfwGetWindowSize(this->window, &this->parameters.windowedWidth,
                     &this->parameters.windowedHeight);
    GL_CHECK_ERROR_M("glfwGetWindowSize");
    glfwGetWindowPos(this->window, &this->parameters.windowedPosX,
                    &this->parameters.windowedPosY);
    GL_CHECK_ERROR_M("glfwGetWindowPos");
}


void Window::PostWindowStateChange() const {
    if (!this->window) return;
    glfwMakeContextCurrent(this->window);
    GL_CHECK_ERROR_M("glfwMakeContextCurrent");

    glfwSwapInterval(this->parameters.vsync ? 1 : 0);
    GL_CHECK_ERROR_M("glfwSwapInterval");

    glEnable(GL_DEPTH_TEST);

    glClearColor(
        this->parameters.clearColor.r,
        this->parameters.clearColor.g,
        this->parameters.clearColor.b,
        this->parameters.clearColor.a
    );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GL_CHECK_ERROR_M("glClear");

    glfwSwapBuffers(this->window);
    GL_CHECK_ERROR_M("glfwSwapBuffers");

    glfwFocusWindow(this->window);
    GL_CHECK_ERROR_M("glfwFocusWindow");
}






///////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////        CALLBACK        ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////


void Window::SetupCallbacks() {
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
        LOG_DEBUGGING("Window lost focus");
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
        LOG_DEBUGGING("Window regained focus");
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


void Window::ClearCallbacks() {
    glfwSetWindowFocusCallback(this->window, nullptr);
    glfwSetWindowSizeCallback(this->window, nullptr);
    glfwSetWindowPosCallback(this->window, nullptr);
    glfwSetKeyCallback(this->window, nullptr);
    glfwSetFramebufferSizeCallback(this->window, nullptr);
    glfwSetWindowIconifyCallback(this->window, nullptr);
    glfwSetWindowMaximizeCallback(this->window, nullptr);
    glfwSetWindowCloseCallback(this->window, nullptr);
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
        LOG_ERROR(error_code, "GLFW Error: ", description);

        // Log plus détaillé pour certaines erreurs critiques
        switch (error_code) {
            case GLFW_INVALID_ENUM:
                LOG_ERROR(error_code, "Invalid enum parameter");
                break;
            case GLFW_INVALID_VALUE:
                LOG_ERROR(error_code, "Invalid value parameter");
                break;
            case GLFW_OUT_OF_MEMORY:
                LOG_ERROR(error_code, "Out of memory");
                break;
            case GLFW_API_UNAVAILABLE:
                LOG_ERROR(error_code, "API unavailable");
                break;
            case GLFW_VERSION_UNAVAILABLE:
                LOG_ERROR(error_code, "Version unavailable");
                break;
            case GLFW_PLATFORM_ERROR:
                LOG_ERROR(error_code, "Platform error");
                break;
            case GLFW_FORMAT_UNAVAILABLE:
                LOG_ERROR(error_code, "Format unavailable");
                break;
        }
    });
}


// Fonction pour vérifier l'état de la fenêtre
bool Window::IsWindowHealthy() const {
    if (!this->window) {
        return false;
    }

    // Vérifier si le contexte OpenGL est toujours valide
    GLFWwindow* currentContext = glfwGetCurrentContext();
    if (currentContext != this->window) {
        LOG_WARNING("OpenGL context mismatch");
        glfwMakeContextCurrent(this->window);
    }

#ifdef DEBUG
    // Vérifier les erreurs OpenGL*
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR(error, "OpenGL Error detected");
        return false;
    }
#endif

    return true;
}
