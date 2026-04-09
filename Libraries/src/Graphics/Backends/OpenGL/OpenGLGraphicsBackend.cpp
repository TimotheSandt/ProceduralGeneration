#include "Graphics/Backends/OpenGL/OpenGLGraphicsBackend.h"

#include "Window.h"

GraphicsAPI OpenGLGraphicsBackend::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

bool OpenGLGraphicsBackend::Initialize() { return Window::InitOpenGL(); }

void OpenGLGraphicsBackend::Shutdown() noexcept { Window::TerminateOpenGL(); }

bool OpenGLGraphicsBackend::IsAvailable() const noexcept { return true; }

std::string OpenGLGraphicsBackend::DescribeAvailability() const { return "OpenGL backend is available."; }
