/**
 * @file logger.h
 * @brief Advanced logging system for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <fstream>
#include <chrono>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <map>

namespace deo {
namespace utils {

/**
 * @brief Log levels
 */
enum class LogLevel {
    TRACE = 0,    ///< Trace level (most verbose)
    DEBUG,        ///< Debug level
    INFO,         ///< Information level
    WARNING,      ///< Warning level
    ERROR,        ///< Error level
    CRITICAL,     ///< Critical level
    FATAL         ///< Fatal level (least verbose)
};

/**
 * @brief Log categories for organization
 */
enum class LogCategory {
    GENERAL,      ///< General messages
    BLOCKCHAIN,   ///< Blockchain operations
    CONSENSUS,    ///< Consensus mechanism
    NETWORKING,   ///< Network operations
    CRYPTOGRAPHY, ///< Cryptographic operations
    STORAGE,      ///< Storage operations
    VIRTUAL_MACHINE, ///< VM operations
    CLI,          ///< Command line interface
    CONFIGURATION,///< Configuration
    PERFORMANCE,  ///< Performance monitoring
    SECURITY      ///< Security-related messages
};

/**
 * @brief Log entry structure
 */
struct LogEntry {
    LogLevel level;                                    ///< Log level
    LogCategory category;                              ///< Log category
    std::string message;                               ///< Log message
    std::string file;                                  ///< Source file
    int line;                                         ///< Source line number
    std::string function;                              ///< Function name
    std::chrono::system_clock::time_point timestamp;  ///< Timestamp
    std::string thread_id;                             ///< Thread ID
    std::map<std::string, std::string> context;        ///< Additional context
    
    LogEntry() : line(0) {}
};

/**
 * @brief Log formatter interface
 */
class LogFormatter {
public:
    virtual ~LogFormatter() = default;
    virtual std::string format(const LogEntry& entry) = 0;
};

/**
 * @brief Default log formatter
 */
class DefaultFormatter : public LogFormatter {
public:
    std::string format(const LogEntry& entry) override;
};

/**
 * @brief JSON log formatter
 */
class JsonFormatter : public LogFormatter {
public:
    std::string format(const LogEntry& entry) override;
};

/**
 * @brief Advanced logging system
 * 
 * This class provides comprehensive logging capabilities with support for
 * multiple output destinations, formatting, filtering, and performance optimization.
 */
class Logger {
public:
    /**
     * @brief Initialize the logging system
     * @param level Minimum log level
     * @param log_file Log file path (empty for console only)
     * @param enable_async Whether to enable asynchronous logging
     */
    static void initialize(LogLevel level = LogLevel::INFO, 
                          const std::string& log_file = "", 
                          bool enable_async = true);
    
    /**
     * @brief Shutdown the logging system
     */
    static void shutdown();
    
    /**
     * @brief Log a message
     * @param level Log level
     * @param category Log category
     * @param message Log message
     * @param file Source file
     * @param line Source line number
     * @param function Function name
     * @param context Additional context
     */
    static void log(LogLevel level,
                   LogCategory category,
                   const std::string& message,
                   const std::string& file = "",
                   int line = 0,
                   const std::string& function = "",
                   const std::map<std::string, std::string>& context = {});
    
    /**
     * @brief Set the minimum log level
     * @param level Minimum log level
     */
    static void setLevel(LogLevel level);
    
    /**
     * @brief Set the log formatter
     * @param formatter Log formatter
     */
    static void setFormatter(std::unique_ptr<LogFormatter> formatter);
    
    /**
     * @brief Enable/disable logging for a specific category
     * @param category Log category
     * @param enabled Whether to enable logging
     */
    static void setCategoryEnabled(LogCategory category, bool enabled);
    
    /**
     * @brief Enable/disable console output
     * @param enabled Whether to enable console output
     */
    static void setConsoleEnabled(bool enabled);
    
    /**
     * @brief Enable/disable file output
     * @param enabled Whether to enable file output
     */
    static void setFileEnabled(bool enabled);
    
    /**
     * @brief Set the log file path
     * @param file_path Log file path
     */
    static void setLogFile(const std::string& file_path);
    
    /**
     * @brief Enable/disable asynchronous logging
     * @param enabled Whether to enable async logging
     */
    static void setAsyncEnabled(bool enabled);
    
    /**
     * @brief Flush all pending log entries
     */
    static void flush();
    
    /**
     * @brief Get logging statistics
     * @return Map of log counts by level and category
     */
    static std::map<std::string, size_t> getStatistics();
    
    /**
     * @brief Clear logging statistics
     */
    static void clearStatistics();
    
    /**
     * @brief Get recent log entries
     * @param count Number of recent entries to return
     * @return List of recent log entries
     */
    static std::vector<LogEntry> getRecentEntries(size_t count = 100);
    
    /**
     * @brief Get all log entries
     * @return Vector of log entries
     */
    static std::vector<LogEntry> getLogs();
    
    /**
     * @brief Log a message (instance method for testing)
     * @param message Log message
     * @param level Log level
     */
    void log(const std::string& message, LogLevel level);
    
    /**
     * @brief Enable/disable log rotation
     * @param enabled Whether to enable log rotation
     * @param max_size Maximum log file size in bytes
     * @param max_files Maximum number of log files to keep
     */
    static void setLogRotation(bool enabled, size_t max_size = 10 * 1024 * 1024, size_t max_files = 5);
    
    /**
     * @brief Create a log context for structured logging
     * @return Log context map
     */
    static std::map<std::string, std::string> createContext();

private:
    struct LogConfig {
        LogLevel min_level;
        std::string log_file;
        bool console_enabled;
        bool file_enabled;
        bool async_enabled;
        bool rotation_enabled;
        size_t max_file_size;
        size_t max_files;
        std::map<LogCategory, bool> category_enabled;
    };
    
    static std::unique_ptr<Logger> instance_;
    static std::mutex instance_mutex_;
    
    std::mutex mutex_;
    LogConfig config_;
    std::unique_ptr<LogFormatter> formatter_;
    std::ofstream log_stream_;
    
    std::thread async_thread_;
    std::atomic<bool> stop_async_;
    std::queue<LogEntry> log_queue_;
    std::condition_variable log_condition_;
    
    std::map<std::string, size_t> statistics_;
    std::vector<LogEntry> recent_entries_;
    
public:
    Logger();
    ~Logger();
    
    /**
     * @brief Get the singleton instance
     * @return Logger instance
     */
    static Logger& getInstance();
    
    /**
     * @brief Write log entry to output destinations
     * @param entry Log entry to write
     */
    void writeLog(const LogEntry& entry);
    
    /**
     * @brief Write to console
     * @param entry Log entry
     */
    void writeToConsole(const LogEntry& entry);
    
    /**
     * @brief Write to file
     * @param entry Log entry
     */
    void writeToFile(const LogEntry& entry);
    
    /**
     * @brief Start the asynchronous logging worker
     */
    void startAsyncWorker();
    
    /**
     * @brief Asynchronous logging worker
     */
    void asyncWorker();
    
    /**
     * @brief Rotate log file if necessary
     */
    void rotateLogFile();
    
    /**
     * @brief Get level name as string
     * @param level Log level
     * @return Level name
     */
    std::string getLevelName(LogLevel level) const;
    
    /**
     * @brief Get category name as string
     * @param category Log category
     * @return Category name
     */
    std::string getCategoryName(LogCategory category) const;
    
    /**
     * @brief Get thread ID as string
     * @return Thread ID string
     */
    std::string getThreadId();
    
    /**
     * @brief Update statistics
     * @param entry Log entry
     */
    void updateStatistics(const LogEntry& entry);
    
    /**
     * @brief Add to recent entries
     * @param entry Log entry
     */
    void addToRecentEntries(const LogEntry& entry);
};

// Convenience macros for logging
#define DEO_LOG_TRACE(category, message) \
    deo::utils::Logger::log( \
        deo::utils::LogLevel::TRACE, \
        deo::utils::LogCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_LOG_DEBUG(category, message) \
    ::deo::utils::Logger::log( \
        ::deo::utils::LogLevel::DEBUG, \
        ::deo::utils::LogCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_LOG_INFO(category, message) \
    ::deo::utils::Logger::log( \
        ::deo::utils::LogLevel::INFO, \
        ::deo::utils::LogCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_LOG_WARNING(category, message) \
    ::deo::utils::Logger::log( \
        ::deo::utils::LogLevel::WARNING, \
        ::deo::utils::LogCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_LOG_ERROR(category, message) \
    ::deo::utils::Logger::log( \
        ::deo::utils::LogLevel::ERROR, \
        ::deo::utils::LogCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_LOG_CRITICAL(category, message) \
    deo::utils::Logger::log( \
        deo::utils::LogLevel::CRITICAL, \
        deo::utils::LogCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_LOG_FATAL(category, message) \
    deo::utils::Logger::log( \
        deo::utils::LogLevel::FATAL, \
        deo::utils::LogCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

} // namespace utils
} // namespace deo
