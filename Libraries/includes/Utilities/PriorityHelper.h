#pragma once

#ifdef _WIN32
#include <windows.h>
#endif
#include <string>

#include "Logger.h"

class PriorityHelper {
public:
    // Apply Game Mode-like optimizations to current process
    static bool RequestHighPriority();

    // Check if Game Mode is enabled via registry
    static bool IsGameModeEnabledInRegistry();

    // Check current process priority
    static void DisplayCurrentPriority();

    // Get current executable path
    static std::string GetExecutablePath();

    // Apply additional performance tweaks
    static void ApplyPerformanceTweaks();
};