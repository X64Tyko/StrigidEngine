#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>

// Log severity levels
enum class LogLevel
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5
};

// Singleton logger with thread-safe file writing
class Logger
{
public:
    static Logger& Get()
    {
        static Logger s_Instance;
        return s_Instance;
    }

    // Initialize logger with file output
    void Init(const std::string& logFilePath = "StrigidEngine.log", LogLevel minLevel = LogLevel::Debug);
    
    // Shut down and flush file
    void Shutdown();
    
    // Set minimum log level filter
    void SetMinLevel(LogLevel level) { m_MinLevel = level; }
    
    // Core logging function
    void Log(LogLevel level, const char* file, int line, const std::string& message);
    
private:
    Logger() = default;
    ~Logger() { Shutdown(); }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string GetTimestamp();
    std::string LevelToString(LogLevel level);
    std::string LevelToColor(LogLevel level);
    
private:
    std::ofstream m_LogFile;
    std::mutex m_Mutex;
    LogLevel m_MinLevel = LogLevel::Debug;
    bool m_Initialized = false;
};

// Convenience macros for logging
#define LOG_TRACE(msg) Logger::Get().Log(LogLevel::Trace, __FILE__, __LINE__, msg)
#define LOG_DEBUG(msg) Logger::Get().Log(LogLevel::Debug, __FILE__, __LINE__, msg)
#define LOG_INFO(msg) Logger::Get().Log(LogLevel::Info, __FILE__, __LINE__, msg)
#define LOG_WARN(msg) Logger::Get().Log(LogLevel::Warning, __FILE__, __LINE__, msg)
#define LOG_ERROR(msg) Logger::Get().Log(LogLevel::Error, __FILE__, __LINE__, msg)
#define LOG_FATAL(msg) Logger::Get().Log(LogLevel::Fatal, __FILE__, __LINE__, msg)

// Formatted logging macros with variadic arguments
#define LOG_TRACE_F(fmt, ...) do { \
    char buffer[512]; \
    snprintf(buffer, sizeof(buffer), fmt, __VA_ARGS__); \
    LOG_TRACE(buffer); \
} while(0)

#define LOG_DEBUG_F(fmt, ...) do { \
    char buffer[512]; \
    snprintf(buffer, sizeof(buffer), fmt, __VA_ARGS__); \
    LOG_DEBUG(buffer); \
} while(0)

#define LOG_INFO_F(fmt, ...) do { \
    char buffer[512]; \
    snprintf(buffer, sizeof(buffer), fmt, __VA_ARGS__); \
    LOG_INFO(buffer); \
} while(0)

#define LOG_WARN_F(fmt, ...) do { \
    char buffer[512]; \
    snprintf(buffer, sizeof(buffer), fmt, __VA_ARGS__); \
    LOG_WARN(buffer); \
} while(0)

#define LOG_ERROR_F(fmt, ...) do { \
    char buffer[512]; \
    snprintf(buffer, sizeof(buffer), fmt, __VA_ARGS__); \
    LOG_ERROR(buffer); \
} while(0)

#define LOG_FATAL_F(fmt, ...) do { \
    char buffer[512]; \
    snprintf(buffer, sizeof(buffer), fmt, __VA_ARGS__); \
    LOG_FATAL(buffer); \
} while(0)
