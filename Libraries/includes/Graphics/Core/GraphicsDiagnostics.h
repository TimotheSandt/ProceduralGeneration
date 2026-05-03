#pragma once

#include <string>

void LogActiveGraphicsBackendErrors(std::string context) noexcept;

#ifdef DEBUG
#define GRAPHICS_CHECK_ERRORS_M(context) LogActiveGraphicsBackendErrors(context)
#define GRAPHICS_CHECK_ERRORS() LogActiveGraphicsBackendErrors("")
#else
#define GRAPHICS_CHECK_ERRORS_M(context) ((void)0)
#define GRAPHICS_CHECK_ERRORS() ((void)0)
#endif
