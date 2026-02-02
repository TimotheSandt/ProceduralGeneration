#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#ifdef DEBUG
#include <stacktrace>
#endif

enum LogLevel {
#ifdef DEBUG
    L_DEBUGGING = -1, // Mostly for testing variables when added, shouldn't be keep long even for debugging
#endif
    L_DEBUG,
    L_TRACE,
    L_INFO,
    L_WARNING,
    L_ERROR,
    L_FATAL
};

class Logger {
private:
    struct LogMessage
    {
        LogLevel level;
        size_t repetition = 1;
        std::chrono::time_point<std::chrono::system_clock> time;
#ifdef DEBUG
        std::string file;
        int line;
#endif
        int errorCode = 0; // 0 = no error
        std::string message;
#ifdef DEBUG
        std::string stackTrace;
#endif


        bool operator==(const LogMessage& other) const {
            return (
                level == other.level &&
                message == other.message &&
                errorCode == other.errorCode
#ifdef DEBUG
                && file == other.file && line == other.line &&
                stackTrace == other.stackTrace
#endif
            );
        }

        bool operator!=(const LogMessage& other) const {
            return !(*this == other);
        }
    };

public:
    static void Initialize(const std::string& filename = "");
    static void Clear();
    static void Print();
    static void SetMinimumLevel(LogLevel level);
    static LogLevel GetMinimumLevel();
    static void FlushToFile();

    template <typename... Args>
    static void Log(
        LogLevel level,
#ifdef DEBUG
        const std::string& file,
        int line,
#endif
        Args&&... args);

    template <typename... Args>
    static void LogError(
        LogLevel level,
#ifdef DEBUG
        const std::string& file,
        int line,
#endif
        int errorCode,
        Args&&... args);

private:
    static void AddLog(LogMessage&& msg);
    static size_t CalculateLogLength(const LogMessage& log);
    static size_t CalculateNumberOfLines(const LogMessage& log);
    static std::string FormatTimestamp(const std::chrono::time_point<std::chrono::system_clock>& time);
    static void PrintLog(const LogMessage& log);
    static std::string LevelToString(LogLevel level);
    static std::string GetDefaultColor();
    static std::string GetColorLevel(LogLevel level);
    static void ChangeColor(LogLevel level);
    static void ChangeColor(const std::string& color);
    static void ResetColor();
#ifdef DEBUG
    static std::string CaptureStackTrace();
#endif

    static std::vector<LogMessage> logs;
    static LogLevel lLevelPrinted;
    static std::mutex logMutex;
    static std::unique_ptr<std::ofstream> logFile;
    static bool isLoggingToFile;
    static size_t lastFlushedIndex;
};




template <typename... Args>
void Logger::Log(
    LogLevel level,
#ifdef DEBUG
    const std::string& file,
    int line,
#endif
    Args&&... args)
{
    Logger::LogError(level,
#ifdef DEBUG
        file, line,
#endif
        0, std::forward<Args>(args)...
    );
}

template <typename... Args>
void Logger::LogError(
    LogLevel level,
#ifdef DEBUG
    const std::string& file,
    int line,
#endif
    int errorCode,
    Args&&... args)
{
    std::ostringstream oss;
    ((oss << args), ...);

    LogMessage msg;
    msg.level = level;
    msg.time = std::chrono::system_clock::now();
#ifdef DEBUG
    msg.file = file;
    msg.line = line;
#endif
    msg.message = oss.str();
    msg.errorCode = errorCode;
#ifdef DEBUG
    msg.stackTrace = CaptureStackTrace();
#endif

    Logger::AddLog(std::move(msg));
}



#define LOG_LEVEL Logger::GetMinimumLevel()
#ifdef DEBUG
#define SET_LOG_LEVEL_DEBUG Logger::SetMinimumLevel(L_DEBUGGING)
#else
#define SET_LOG_LEVEL_DEBUG Logger::SetMinimumLevel(L_DEBUG)
#endif
#define SET_LOG_LEVEL_TRACE Logger::SetMinimumLevel(L_TRACE)
#define SET_LOG_LEVEL_INFO Logger::SetMinimumLevel(L_INFO)
#define SET_LOG_LEVEL_WARNING Logger::SetMinimumLevel(L_WARNING)
#define SET_LOG_LEVEL_ERROR Logger::SetMinimumLevel(L_ERROR)
#define SET_LOG_LEVEL_FATAL Logger::SetMinimumLevel(L_FATAL)
#define SET_LOG_FILE(filename) Logger::Initialize(filename)
#define SET_LOG_FILE_DEFAULT Logger::Initialize()
#define FLUSH_LOG_TO_FILE Logger::FlushToFile()

#ifdef DEBUG
    #define LOG(level, ...) Logger::Log(level, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_DEBUGGING(...) Logger::Log(L_DEBUGGING, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_DEBUG(...) Logger::Log(L_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_TRACE(...) Logger::Log(L_TRACE, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_INFO(...) Logger::Log(L_INFO, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_WARNING(...) Logger::Log(L_WARNING, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_EWARNING(...) Logger::LogError(L_WARNING, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_ERROR(...) Logger::LogError(L_ERROR, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_FATAL(...) Logger::LogError(L_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define LOG(level, ...) Logger::Log(level, __VA_ARGS__)
    #define LOG_DEBUGGING(...)
    #define LOG_DEBUG(...) Logger::Log(L_DEBUG, __VA_ARGS__)
    #define LOG_TRACE(...) Logger::Log(L_TRACE, __VA_ARGS__)
    #define LOG_INFO(...) Logger::Log(L_INFO, __VA_ARGS__)
    #define LOG_WARNING(...) Logger::Log(L_WARNING, __VA_ARGS__)
    #define LOG_EWARNING(...) Logger::LogError(L_WARNING, __VA_ARGS__)
    #define LOG_ERROR(...) Logger::LogError(L_ERROR, __VA_ARGS__)
    #define LOG_FATAL(...) Logger::LogError(L_FATAL, __VA_ARGS__)
#endif

#ifdef DEBUG
#define GL_CHECK_ERROR_M(...) do { \
    GLenum err; \
    while ((err = glGetError()) != GL_NO_ERROR) { \
        LOG_ERROR(err, "OpenGL error : ", __VA_ARGS__); \
    } \
} while (0)

#define GL_CHECK_ERROR() do { \
    GLenum err; \
    while ((err = glGetError()) != GL_NO_ERROR) { \
        LOG_ERROR(err, "OpenGL error"); \
    } \
} while (0)
#else
#define GL_CHECK_ERROR()
#define GL_CHECK_ERROR_M(...)
#endif
