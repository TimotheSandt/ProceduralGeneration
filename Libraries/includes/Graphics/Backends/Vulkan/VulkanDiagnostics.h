#pragma once

#include <string_view>

namespace VulkanDiagnostics
{

void LogErrors(std::string_view context) noexcept;

} // namespace VulkanDiagnostics
