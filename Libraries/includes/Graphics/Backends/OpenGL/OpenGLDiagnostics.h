#pragma once

#include <string_view>

namespace OpenGLDiagnostics
{

void LogErrors(std::string_view context) noexcept;

} // namespace OpenGLDiagnostics
