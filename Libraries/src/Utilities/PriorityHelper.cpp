#include "PriorityHelper.h"

#ifdef _WIN32
#include <windows.h>
#include <winreg.h>
#else
#include <unistd.h>
#include <limits.h>
#include <cstring>
#include <sched.h>
#include <sys/resource.h>
#include <errno.h>
#endif

bool PriorityHelper::RequestHighPriority() {
#ifdef _WIN32
    bool success = true;
    
    try {
        // Get current process handle
        HANDLE hProcess = GetCurrentProcess();
        
        // Set process priority to high (similar to Game Mode)
        if (!SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS)) {
            LOG_EWARNING(GetLastError(), "Failed to set high priority class");
            // Try above normal instead
            if (!SetPriorityClass(hProcess, ABOVE_NORMAL_PRIORITY_CLASS)) {
                LOG_ERROR(GetLastError(), "Failed to set above normal priority class");
                success = false;
            }
        }

        // Set main thread priority
        HANDLE hThread = GetCurrentThread();
        if (!SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST)) {
            LOG_EWARNING(GetLastError(), "Failed to set thread priority");
        }

        // Disable Windows Error Reporting for this process (reduces interruptions)
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                    SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX);
        
        LOG_TRACE("Game Mode-like optimizations applied!");
        return success;
    }
    catch (const std::exception& e) {
        LOG_ERROR(GetLastError(), "Error applying optimizations: ", e.what());
        return false;
    }
#else
    // On Linux/macOS, attempt to set higher process priority using setpriority (requires permissions)
    if (setpriority(PRIO_PROCESS, 0, -10) != 0) {
        LOG_WARNING("Failed to increase process priority: %s (may require root privileges)", strerror(errno));
        return false;
    }
    // Attempt to set scheduling policy to FIFO for real-time (requires root and CAP_SYS_NICE)
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        LOG_WARNING("Failed to set real-time scheduling: %s", strerror(errno));
    }
    LOG_TRACE("Platform-specific priority optimizations applied!");
    return true;
#endif
}

bool PriorityHelper::IsGameModeEnabledInRegistry() {
#ifdef _WIN32
    HKEY hKey;
    DWORD value = 0;
    DWORD valueSize = sizeof(DWORD);
    DWORD valueType;
    
    // Check the Game Mode registry setting
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\GameBar",
        0,
        KEY_READ,
        &hKey
    );
    
    if (result == ERROR_SUCCESS) {
        result = RegQueryValueExW(
            hKey,
            L"AllowAutoGameMode",
            nullptr,
            &valueType,
            (LPBYTE)&value,
            &valueSize
        );
        
        RegCloseKey(hKey);
        
        if (result == ERROR_SUCCESS && valueType == REG_DWORD) {
            return (value == 1);
        }
    }
    
    // If registry key doesn't exist, Game Mode might still be available
    // Check if we're on Windows 10/11 by trying another Game Bar key
    result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"System\\GameConfigStore",
        0,
        KEY_READ,
        &hKey
    );
    
    if (result == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true; // GameConfigStore exists, so Game Mode is likely available
    }
    
    return false;
#else
    LOG_TRACE("Game Mode check not applicable on non-Windows platforms");
    return false;
#endif
}


void PriorityHelper::DisplayCurrentPriority() {
#ifdef _WIN32
    HANDLE hProcess = GetCurrentProcess();
    DWORD priority = GetPriorityClass(hProcess);
    
    std::string msg = "Current process priority class: ";
    switch (priority) {
        case IDLE_PRIORITY_CLASS:
            msg += "IDLE";
            break;
        case BELOW_NORMAL_PRIORITY_CLASS:
            msg += "BELOW_NORMAL";
            break;
        case NORMAL_PRIORITY_CLASS:
            msg += "NORMAL";
            break;
        case ABOVE_NORMAL_PRIORITY_CLASS:
            msg += "ABOVE_NORMAL";
            break;
        case HIGH_PRIORITY_CLASS:
            msg += "HIGH (Game Mode-like)";
            break;
        case REALTIME_PRIORITY_CLASS:
            msg += "REALTIME";
            break;
        default:
            msg += "UNKNOWN (" + std::to_string(priority) + ")";
            break;
    }
    LOG_TRACE(msg);
#else
    int prio = getpriority(PRIO_PROCESS, 0);
    std::string msg = "Current process nice value: " + std::to_string(prio);
    LOG_TRACE(msg);
#endif
}


std::string PriorityHelper::GetExecutablePath() {
#ifdef _WIN32
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    
    if (length > 0 && length < MAX_PATH) {
        return std::string(path);
    }
    return "Unknown";
#else
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, PATH_MAX - 1);
    if (len != -1) {
        path[len] = '\0';
        return std::string(path);
    }
    return "Unknown";
#endif
}



void PriorityHelper::ApplyPerformanceTweaks() {
#ifdef _WIN32
    SetProcessDPIAware();
    timeBeginPeriod(1);
    LOG_TRACE("Additional Windows performance tweaks applied.");
#else
    // On Linux, set higher priority for timer if possible, or other tweaks
    // For example, disable core dumps or adjust I/O priority if needed
    LOG_TRACE("Additional platform performance tweaks applied (Linux/macOS).");
#endif
}