#include "PriorityHelper.h"

bool PriorityHelper::RequestHighPriority() {
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
}

bool PriorityHelper::IsGameModeEnabledInRegistry() {
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
}


void PriorityHelper::DisplayCurrentPriority() {
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
}


std::wstring PriorityHelper::GetExecutablePath() {
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    
    if (length > 0 && length < MAX_PATH) {
        return std::wstring(path);
    }
    
    return L"Unknown";
}



void PriorityHelper::ApplyPerformanceTweaks() {
    SetProcessDPIAware();
    timeBeginPeriod(1);
    
    LOG_TRACE("Additional performance tweaks applied.");
}