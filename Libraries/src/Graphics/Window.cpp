#include "Window.h"

#include "Graphics/Core/GraphicsDiagnostics.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Core/WindowContext.h"
#include "InputManager.h"
#include "Profiler.h"
#include "Logger.h"
#include "utilities.h"

#ifdef _WIN32
#include <windows.h>
#endif

Window::Window()
{
    this->window = nullptr;
    this->inputManager = nullptr;
    this->parameters.title = "Window";
    this->parameters.width = 800;
    this->parameters.height = 600;
    this->parameters.posX = 100;
    this->parameters.posY = 100;
    this->parameters.maxFPS = 0;
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
}

Window::~Window() { this->Close(); }

Window::Window(Window &&other) noexcept { this->Swap(other); }

Window &Window::operator=(Window &&other) noexcept
{
    if (this != &other)
    {
        this->Close();
        this->Swap(other);
    }
    return *this;
}

void Window::Swap(Window &other) noexcept
{
    std::swap(this->window, other.window);
    std::swap(this->parameters, other.parameters);
    std::swap(this->inputManager, other.inputManager);
    std::swap(this->fpsCounter, other.fpsCounter);
    std::swap(this->lastTime, other.lastTime);
}

int Window::Init()
{
    if (!IsGraphicsAPIActive(GraphicsAPI::OpenGL) && !IsGraphicsAPIActive(GraphicsAPI::Vulkan))
    {
        LOG_ERROR(1, "Window currently supports only the OpenGL or Vulkan runtime backends");
        return -1;
    }

    // Create a window for the active graphics runtime.
    this->window = glfwCreateWindow(this->parameters.width, this->parameters.height, this->parameters.title.c_str(), nullptr, nullptr);
    if (!this->window)
    {
        LOG_ERROR(1, "Failed to create GLFW window");
        return -1;
    }

    if (!GraphicsWindowContext::Initialize(this->window, this->parameters.vsync, this->parameters.width, this->parameters.height))
    {
        this->Close();
        return -1;
    }

    glfwGetWindowPos(this->window, &this->parameters.posX, &this->parameters.posY);
    GRAPHICS_CHECK_ERRORS_M("glfwGetWindowPos");

    glfwSetWindowUserPointer(this->window, this);
    GRAPHICS_CHECK_ERRORS_M("glfwSetWindowUserPointer");

    this->SetupCallbacks();

    this->ChangeWindowState(this->parameters.windowState);
    this->inputManager = &InputManager::GetInstance(this->window);

    return 0;
}

void Window::Close()
{
    if (!this->window)
    {
        return;
    }

    this->ClearCallbacks();

    InputManager::RemoveInstance(this->window);
    this->inputManager = nullptr;

    GraphicsWindowContext::Shutdown(this->window);
    glfwDestroyWindow(this->window);
    this->window = nullptr;

#ifdef _WIN32
    ShowWindow(FindWindowA("Shell_TrayWnd", nullptr), SW_SHOW);
#endif
}

bool Window::NewFrame()
{
    Profiler::Profile("PollEvents", &glfwPollEvents);
    Profiler::Process();
    this->inputManager->Update();

    if (!Profiler::Profile("HealthCheck", &Window::IsWindowHealthy, this))
    {
        LOG_WARNING("Window is not healthy");
        return false;
    }

    // For Vulkan (and other APIs that require per-frame state setup), acquire the
    // swapchain image and open the render pass at the start of each frame.
    if (this->window != nullptr)
    {
        GraphicsWindowContext::ApplyDefaultFramebufferState(this->window, this->parameters.vsync, this->parameters.clearColor);
    }

    this->fpsCounter.newFrame(this->parameters.maxFPS);

    if (this->parameters.trueEveryms == 0)
    {
        this->fpsCounter.updateStat();
        return true;
    }

    auto now = this->fpsCounter.getTime();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->lastTime).count();
    if (elapsed >= static_cast<long long>(this->parameters.trueEveryms))
    {
        this->fpsCounter.updateStat();
        this->lastTime = now;
    }

    return true;
}

void Window::SwapBuffers()
{
    if (!this->window)
    {
        return;
    }

    GraphicsWindowContext::Present(this->window);
}

///////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Window state functions ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

void Window::ChangeWindowState(WindowState state)
{
    switch (state)
    {
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

void Window::ToggleFullscreen()
{
    if (this->parameters.windowState == WindowState::FULLSCREEN || this->parameters.windowState == WindowState::FULLSCREEN_UNFOCUSED)
    {
        this->ActivateWindowed();
    }
    else
    {
        this->ActivateFullscreen();
    }
}

void Window::ToggleBorderless()
{
    if (this->parameters.windowState == WindowState::BORDERLESS)
    {
        this->ActivateWindowed();
    }
    else
    {
        this->ActivateBorderless();
    }
}

void Window::ActivateFullscreen()
{
    if (this->parameters.windowState == WindowState::FULLSCREEN)
    {
        return;
    }

    if (!IsWindowHealthy())
    {
        LOG_WARNING("Window not healthy, skipping fullscreen toggle");
        return;
    }

    if (this->parameters.windowState == WindowState::WINDOWED)
    {
        this->SaveWindowedParameters();
    }

    // Get primary monitor
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        LOG_ERROR(1, "Failed to get primary monitor");
        return;
    }

    // Get monitor's video mode
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (!mode)
    {
        LOG_ERROR(1, "Failed to get video mode");
        return;
    }

    // Switch to fullscreen
    glfwSetWindowMonitor(this->window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    GRAPHICS_CHECK_ERRORS_M("glfwSetWindowMonitor");

    // Update internal parameters
    this->parameters.width = mode->width;
    this->parameters.height = mode->height;
    this->parameters.windowState = WindowState::FULLSCREEN;

    PostWindowStateChange();
    LOG_DEBUG("Switched to fullscreen: ", mode->width, "x", mode->height, " @ ", mode->refreshRate, "Hz");
}

void Window::ActivateWindowed()
{
    if (this->parameters.windowState == WindowState::WINDOWED)
    {
        return;
    }

    if (!IsWindowHealthy())
    {
        LOG_WARNING("Window not healthy, skipping windowed toggle");
        return;
    }

#ifdef _WIN32
    ShowWindow(FindWindowA("Shell_TrayWnd", nullptr), SW_SHOW);
#endif

    glfwSetWindowMonitor(this->window, nullptr, this->parameters.windowedPosX, this->parameters.windowedPosY,
                         this->parameters.windowedWidth, this->parameters.windowedHeight, GLFW_DONT_CARE);
    GRAPHICS_CHECK_ERRORS_M("glfwSetWindowMonitor");
    glfwSetWindowAttrib(this->window, GLFW_DECORATED, GLFW_TRUE);
    GRAPHICS_CHECK_ERRORS_M("glfwSetWindowAttrib");

    // Update internal parameters
    this->parameters.width = this->parameters.windowedWidth;
    this->parameters.height = this->parameters.windowedHeight;
    this->parameters.windowState = WindowState::WINDOWED;

    PostWindowStateChange();
    LOG_DEBUG("Switched to windowed: ", this->parameters.windowedWidth, "x", this->parameters.windowedHeight);
}

void Window::ActivateBorderless()
{
    if (this->parameters.windowState == WindowState::BORDERLESS)
    {
        return;
    }
    if (!IsWindowHealthy())
    {
        LOG_WARNING("Window not healthy, skipping borderless toggle");
        return;
    }

    if (this->parameters.windowState == WindowState::WINDOWED)
    {
        SaveWindowedParameters();
    }

    // Get primary monitor
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        LOG_ERROR(1, "Failed to get primary monitor");
        return;
    }

    // Get monitor's video mode
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (!mode)
    {
        LOG_ERROR(1, "Failed to get video mode");
        return;
    }

    int width = mode->width;
    int height = mode->height + 1;

    // On Windows, we need to hide the taskbar
#ifdef _WIN32
    ShowWindow(FindWindowA("Shell_TrayWnd", nullptr), SW_HIDE);
#endif

    glfwSetWindowAttrib(this->window, GLFW_DECORATED, GLFW_FALSE);
    GRAPHICS_CHECK_ERRORS_M("glfwSetWindowAttrib");
    glfwSetWindowMonitor(this->window, nullptr, 0, 0, width, height, GLFW_DONT_CARE);
    GRAPHICS_CHECK_ERRORS_M("glfwSetWindowMonitor");

    // Update internal parameters
    this->parameters.width = width;
    this->parameters.height = height;
    this->parameters.windowState = WindowState::BORDERLESS;

    PostWindowStateChange();
    LOG_DEBUG("Switched to borderless: ", width, "x", height);
}

void Window::SaveWindowedParameters()
{
    if (!this->window)
    {
        return;
    }

    glfwGetWindowSize(this->window, &this->parameters.windowedWidth, &this->parameters.windowedHeight);
    GRAPHICS_CHECK_ERRORS_M("glfwGetWindowSize");
    glfwGetWindowPos(this->window, &this->parameters.windowedPosX, &this->parameters.windowedPosY);
    GRAPHICS_CHECK_ERRORS_M("glfwGetWindowPos");
}

void Window::PostWindowStateChange() const
{
    if (!this->window)
    {
        return;
    }

    GraphicsWindowContext::ApplyDefaultFramebufferState(this->window, this->parameters.vsync, this->parameters.clearColor);
}

///////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////        CALLBACK        ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

void Window::SetupCallbacks()
{
    // Callback pour gérer la perte de focus (peut aider avec la touche Windows)
    glfwSetWindowFocusCallback(this->window,
                               [](GLFWwindow *window, int focused)
                               {
                                   try
                                   {
                                       Window *windowObj = static_cast<Window *>(glfwGetWindowUserPointer(window));
                                       if (!windowObj)
                                       {
                                           return;
                                       }
                                       windowObj->CallbackFocus(window, focused);
                                   }
                                   catch (...)
                                   {
                                       // Ignorer les erreurs dans ce callback
                                   }
                               });

    glfwSetWindowSizeCallback(this->window,
                              [](GLFWwindow *window, int width, int height)
                              {
                                  try
                                  {
                                      Window *windowObj = static_cast<Window *>(glfwGetWindowUserPointer(window));
                                      if (!windowObj)
                                      {
                                          return;
                                      }
                                      windowObj->CallbackResize(window, width, height);
                                  }
                                  catch (...)
                                  {
                                      // Ignorer les erreurs dans ce callback
                                  }
                              });

    glfwSetWindowPosCallback(this->window,
                             [](GLFWwindow *window, int x, int y)
                             {
                                 try
                                 {
                                     Window *windowObj = static_cast<Window *>(glfwGetWindowUserPointer(window));
                                     if (!windowObj)
                                     {
                                         return;
                                     }
                                     windowObj->CallbackPosition(window, x, y);
                                 }
                                 catch (...)
                                 {
                                     // Ignorer les erreurs dans ce callback
                                 }
                             });
}

void Window::CallbackResize(GLFWwindow *window, int width, int height)
{
    UNUSED(window);

    this->parameters.width = width;
    this->parameters.height = height;
    GraphicsRenderState::SetViewport(0, 0, this->parameters.width, this->parameters.height);
}

void Window::CallbackPosition(GLFWwindow *window, int x, int y)
{
    UNUSED(window);

    this->parameters.posX = x;
    this->parameters.posY = y;
}

void Window::CallbackFocus(GLFWwindow *window, int focused)
{
    UNUSED(window);
    UNUSED(focused);

    LOG_DEBUGGING("Window focus changed");

#ifdef _WIN32
    if (this->parameters.windowState == WindowState::BORDERLESS)
    {
        ShowWindow(FindWindowA("Shell_TrayWnd", nullptr), SW_HIDE);
    }
    else
    {
        ShowWindow(FindWindowA("Shell_TrayWnd", nullptr), SW_SHOW);
    }
#endif
}

void Window::ClearCallbacks()
{
    glfwSetWindowFocusCallback(this->window, nullptr);
    glfwSetWindowSizeCallback(this->window, nullptr);
    glfwSetWindowPosCallback(this->window, nullptr);
    glfwSetKeyCallback(this->window, nullptr);
    glfwSetFramebufferSizeCallback(this->window, nullptr);
    glfwSetWindowIconifyCallback(this->window, nullptr);
    glfwSetWindowMaximizeCallback(this->window, nullptr);
    glfwSetWindowCloseCallback(this->window, nullptr);
}

// Fonction pour vérifier l'état de la fenêtre
bool Window::IsWindowHealthy() const
{
    if (!this->window)
    {
        return false;
    }

    // Vérifier si le contexte OpenGL est toujours valide
    if (IsGraphicsAPIActive(GraphicsAPI::OpenGL))
    {
        GLFWwindow *currentContext = glfwGetCurrentContext();
        if (currentContext != this->window)
        {
            LOG_WARNING("OpenGL context mismatch");
            GraphicsWindowContext::EnsureContextReady(this->window);
        }
    }

    return true;
}
