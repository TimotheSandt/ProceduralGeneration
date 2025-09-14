#include "Logger.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif


std::vector<Logger::LogMessage> Logger::logs;
std::mutex Logger::logMutex;
std::unique_ptr<std::ofstream> Logger::logFile;
size_t Logger::lastFlushedIndex = 0;
#ifdef DEBUG
LogLevel Logger::lLevelPrinted = LogLevel::L_DEBUGGING;
#else
LogLevel Logger::lLevelPrinted = LogLevel::L_INFO;
#endif



void Logger::Initialize(const std::string& sFilename) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    std::string filename = sFilename;
    if (filename.empty()) {
        // Create logs directory if it doesn't exist
        std::filesystem::create_directories("logs");
        
        // Generate timestamped filename
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << "logs/log_" << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S") << ".log";
        filename = ss.str();
    } else {
        // Ensure directory exists for provided filename
        std::filesystem::path filepath(filename);
        if (filepath.has_parent_path()) {
            std::filesystem::create_directories(filepath.parent_path());
        }
    }
    
    // Close existing file
    if (logFile && logFile->is_open()) {
        *logFile << "\n=== Logger Session Ended: " 
                 << FormatTimestamp(std::chrono::system_clock::now()) << " ===\n";
        logFile->close();
    }
    
    // Open new file
    logFile = std::make_unique<std::ofstream>(filename, std::ios::out | std::ios::app);
    
    if (!logFile->is_open()) {
        std::cerr << "ERROR: Could not create/open log file: " << filename << std::endl;
        
        // Try fallback location
        std::string fallback = "fallback_log.txt";
        logFile = std::make_unique<std::ofstream>(fallback, std::ios::out | std::ios::app);
        
        if (logFile->is_open()) {
            std::cerr << "Using fallback log file: " << fallback << std::endl;
            filename = fallback;
        } else {
            std::cerr << "FATAL: Cannot create any log file!" << std::endl;
            logFile.reset();
            return;
        }
    }
    
    // Write session header
    auto now = std::chrono::system_clock::now();
    *logFile << "\n=== Logger Session Started: " 
             << FormatTimestamp(now) << " ===" << std::endl;
    *logFile << "Process ID: " << getpid() << std::endl;
    *logFile << "Working Directory: " << std::filesystem::current_path() << std::endl;
    *logFile << std::endl;
    logFile->flush();
    
    std::cout << "Logger initialized successfully. File: " << filename << std::endl;
}


void Logger::Clear() { 
    std::lock_guard<std::mutex> lock(logMutex);
    logs.clear();
    lastFlushedIndex = 0;
}

void Logger::Print() {
    std::lock_guard<std::mutex> lock(logMutex);
    for (const LogMessage& log : logs) {
        PrintLog(log);
    }
}

void Logger::SetMinimumLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex);
    lLevelPrinted = level;
}

LogLevel Logger::GetMinimumLevel() {
    std::lock_guard<std::mutex> lock(logMutex);
    return lLevelPrinted;
}

void Logger::FlushToFile() {
    size_t sizeLogs = logs.size();
    LOG_DEBUGGING("=== FlushToFile Debug Info ===");
    LOG_DEBUGGING("Total logs: ", sizeLogs);
    LOG_DEBUGGING("Last flushed index: ", lastFlushedIndex);
    LOG_DEBUGGING("Logs to write: ", (sizeLogs - lastFlushedIndex));

    std::lock_guard<std::mutex> lock(logMutex);
    
    if (!logFile) {
        std::cout << "ERROR: logFile is null!" << std::endl;
        return;
    }
    
    if (!logFile->is_open()) {
        std::cout << "ERROR: logFile is not open!" << std::endl;
        return;
    }
    
    // Check if there are new logs to write
    if (lastFlushedIndex >= logs.size()) {
        std::cout << "No new logs to write" << std::endl;
        return;
    }
    
    // Write logs that haven't been written yet
    size_t logsWritten = 0;
    for (size_t i = lastFlushedIndex; i < logs.size(); ++i) {
        const LogMessage& log = logs[i];
        
        if (log.level == L_DEBUGGING) {
            continue;
        }
        
        *logFile << "[" << FormatTimestamp(log.time) << "] ";
        *logFile << "[" << LevelToString(log.level) << "] ";
        
#ifdef DEBUG
        *logFile << log.file << ":" << log.line << ": ";
#endif
        
        *logFile << log.message;
        
        if (log.errorCode != 0) {
            *logFile << " (Error: " << log.errorCode << ")";
        }
        
        *logFile << std::endl;
        logsWritten++;
    }
    lastFlushedIndex = logs.size();
    
    logFile->flush();
#ifdef DEBUG
    if (logFile->fail()) {
        std::cout << "\033[31m";
        std::cout << "ERROR: Failed to write to log file!";
        std::cout << "\033[0m" << std::endl;
    } else {
        std::cout << "\033[32m";
        std::cout << "Successfully flushed logs to file";
        std::cout << "\033[0m" << std::endl;
    }
#endif
}

size_t getTerminalWidth() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns;
    
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        return columns > 0 ? columns : 80;
    }
    return 80; // Défaut si échec
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80; // Défaut si échec
#endif
}

size_t Logger::CalculateLogLength(const LogMessage& log) {
    size_t length = 4;
    length += FormatTimestamp(log.time).size() + 2;  // +2 for "[]"
    if (log.repetition > 1) {
        length += std::to_string(log.repetition).size() + 2;  // +2 for "[]"
    }
    length += LevelToString(log.level).size() + 2;  // +2 for "[]"
#ifdef DEBUG
    length += log.file.size() + 1;  // +1 for ":"
    length += std::to_string(log.line).size();  // +1 for ":"
#endif
    length += log.message.size();
    return length;
}


size_t Logger::CalculateNumberOfLines(const LogMessage& log) {
    size_t terminalWidth = getTerminalWidth();
    if (terminalWidth == 0) return 1;
    
    // Calculer la longueur du préfixe (tout sauf le message)
    size_t prefixLength = 4;
    prefixLength += FormatTimestamp(log.time).size() + 2;
    if (log.repetition > 1) {
        prefixLength += std::to_string(log.repetition).size() + 2;
    }
    prefixLength += LevelToString(log.level).size() + 2;
#ifdef DEBUG
    prefixLength += log.file.size() + 1;
    prefixLength += std::to_string(log.line).size();
#endif
    
    // Diviser le message par les \n
    std::vector<std::string> messageLines;
    std::stringstream ss(log.message);
    std::string line;
    
    while (std::getline(ss, line)) {
        messageLines.push_back(line);
    }
    
    // Si pas de \n dans le message, ajouter le message complet
    if (messageLines.empty()) {
        messageLines.push_back(log.message);
    }
    
    size_t totalLines = 0;
    
    for (size_t i = 0; i < messageLines.size(); i++) {
        size_t lineLength;
        
        if (i == 0) {
            // Première ligne : préfixe + contenu
            lineLength = prefixLength + messageLines[i].size();
        } else {
            // Lignes suivantes : seulement le contenu (avec indentation possible)
            lineLength = messageLines[i].size();
        }
        
        // Calculer combien de lignes cette partie prend
        if (lineLength == 0) {
            totalLines++; // Ligne vide
        } else {
            totalLines += (lineLength + terminalWidth - 1) / terminalWidth;
        }
    }
    
    return totalLines > 0 ? totalLines : 1;
}



void Logger::AddLog(LogMessage&& log) {
    std::lock_guard<std::mutex> lock(logMutex);

    auto timeLimit = std::chrono::system_clock::now() - std::chrono::seconds(3);
    
    bool isDuplicate = false;
    size_t duplicateIndex = 0;
    size_t dstUp = 0;
    
    // Chercher en partant de la fin, sécurisé contre les vecteurs vides
    if (!logs.empty()) {
        for (size_t i = logs.size(); i > 0; i--) {
            size_t idx = i - 1;  // Index réel
            
            if (logs[idx].time < timeLimit) {
                break;  // Trop ancien, arrêter la recherche
            }
            
            dstUp += CalculateNumberOfLines(logs[idx]);
            
            if (logs[idx] == log) {
                logs[idx].repetition++;  // Modifier l'original dans le vecteur
                duplicateIndex = idx;
                isDuplicate = true;
                break;
            }
        }
    }

    if (isDuplicate) {        
        // Remonter à la ligne du duplicate
        std::cout << "\033[" << dstUp << "F";
        
        const auto& duplicateLog = logs[duplicateIndex];
        bool needFullRewrite = std::to_string(duplicateLog.repetition).size() != 
                              std::to_string(duplicateLog.repetition - 1).size() ||
                              duplicateLog.repetition == 2;
        
        if (needFullRewrite) {
            PrintLog(duplicateLog);
        } else {
            // Mise à jour partielle du compteur de répétition
            std::string timestamp = FormatTimestamp(duplicateLog.time);
            int prefixLen = timestamp.size() + 4; // timestamp + "] ["
            std::cout << "\033[" << prefixLen << "C";
            std::cout << duplicateLog.repetition;
        }
        
        // Redescendre à la position originale
        std::cout << "\033[" << dstUp - (needFullRewrite ? 1 : 0) << "E";
    } else {
        logs.push_back(std::move(log));
        PrintLog(logs.back());
        
        if (logs.back().level == L_FATAL) {
            std::exit(1);
        }
    }
}


std::string Logger::FormatTimestamp(const std::chrono::time_point<std::chrono::system_clock>& timestamp) {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
} 


void Logger::PrintLog(const LogMessage& log) {
    if (log.level < lLevelPrinted) return;

    std::cout << "[" << FormatTimestamp(log.time) << "] ";
    if (log.repetition > 1) {
        std::cout << "[" << log.repetition << "x] ";
    }
    ChangeColor(log.level);
    std::cout << "[" << LevelToString(log.level) << "]";
    
    #ifdef DEBUG
    ResetColor();
    std::cout << " " << log.file << ":" << log.line << " ";
    ChangeColor(log.level);
    if (log.level >= L_WARNING)
        std::cerr << log.message;
    else
        std::cout << log.message;
    #else
    std::cout << " " << log.message;
    #endif

    if (log.errorCode != 0) {
        std::cerr << " (Error: " << log.errorCode << ")";
    }

    ResetColor();
    std::cout << std::endl;
}

std::string Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::L_DEBUGGING: return "DEBUGGING";
        case LogLevel::L_DEBUG: return "DEBUG";
        case LogLevel::L_TRACE: return "TRACE";
        case LogLevel::L_INFO: return "INFO";
        case LogLevel::L_WARNING: return "WARNING";
        case LogLevel::L_ERROR: return "ERROR";
        case LogLevel::L_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}


std::string Logger::GetDefaultColor() { 
    return "39;49"; // Default color: text and background are white
}

std::string Logger::GetColorLevel(LogLevel level) {
    switch (level) {
#ifdef DEBUG
        case LogLevel::L_DEBUGGING: return "95;49";
#endif
        case LogLevel::L_DEBUG:     return "90;49";
        case LogLevel::L_TRACE:     return GetDefaultColor();
        case LogLevel::L_INFO:      return "36;49";
        case LogLevel::L_WARNING:   return "33;49";
        case LogLevel::L_ERROR:     return "31;49";
        case LogLevel::L_FATAL:     return "97;41";
        default:                    return GetDefaultColor();
    }
}

void Logger::ChangeColor(LogLevel level) {
    std::cout << "\033[" << GetColorLevel(level) << "m";
}

void Logger::ChangeColor(const std::string& color) {
    std::cout << "\033[" << color << "m";
}

void Logger::ResetColor() {
    std::cout << "\033[" << GetDefaultColor() << "m";
}

