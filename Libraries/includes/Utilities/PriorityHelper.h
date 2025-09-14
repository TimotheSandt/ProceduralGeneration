#pragma once

#include <windows.h>
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
    static std::wstring GetExecutablePath();

    // Apply additional performance tweaks
    static void ApplyPerformanceTweaks();
};