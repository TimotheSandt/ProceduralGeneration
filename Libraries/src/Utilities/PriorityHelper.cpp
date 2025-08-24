#include "PriorityHelper.h"

bool PriorityHelper::RequestHighPriority() {
    bool success = true;
    
    try {
        // Get current process handle
        HANDLE hProcess = GetCurrentProcess();
        
        // Set process priority to high (similar to Game Mode)
        if (!SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS)) {
            std::wcerr << L"Warning: Failed to set high priority class. Error: " 
                        << GetLastError() << std::endl;
            // Try above normal instead
            if (!SetPriorityClass(hProcess, ABOVE_NORMAL_PRIORITY_CLASS)) {
                std::wcerr << L"Failed to set above normal priority class. Error: " 
                            << GetLastError() << std::endl;
                success = false;
            }
        }

        // Set main thread priority
        HANDLE hThread = GetCurrentThread();
        if (!SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST)) {
            std::wcerr << L"Warning: Failed to set thread priority. Error: " 
                        << GetLastError() << std::endl;
        }

        // Disable Windows Error Reporting for this process (reduces interruptions)
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | 
                    SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX);

        std::wcout << L"Game Mode-like optimizations applied!" << std::endl;
        return success;
    }
    catch (const std::exception& e) {
        std::wcerr << L"Error applying optimizations: " << e.what() << std::endl;
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
    
    std::wcout << L"Current process priority class: ";
    switch (priority) {
        case IDLE_PRIORITY_CLASS:
            std::wcout << L"IDLE" << std::endl;
            break;
        case BELOW_NORMAL_PRIORITY_CLASS:
            std::wcout << L"BELOW_NORMAL" << std::endl;
            break;
        case NORMAL_PRIORITY_CLASS:
            std::wcout << L"NORMAL" << std::endl;
            break;
        case ABOVE_NORMAL_PRIORITY_CLASS:
            std::wcout << L"ABOVE_NORMAL" << std::endl;
            break;
        case HIGH_PRIORITY_CLASS:
            std::wcout << L"HIGH (Game Mode-like)" << std::endl;
            break;
        case REALTIME_PRIORITY_CLASS:
            std::wcout << L"REALTIME" << std::endl;
            break;
        default:
            std::wcout << L"UNKNOWN (" << priority << L")" << std::endl;
    }
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
    
    std::wcout << L"Additional performance tweaks applied." << std::endl;
}