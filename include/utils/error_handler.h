/**
 * @file error_handler.h
 * @brief Sophisticated error handling and debugging system
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <chrono>
#include <map>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <fstream>

namespace deo {
namespace utils {

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    DEBUG,      ///< Debug information
    INFO,       ///< Informational message
    WARNING,    ///< Warning message
    ERROR,      ///< Error message
    CRITICAL,   ///< Critical error
    FATAL       ///< Fatal error (will terminate)
};

/**
 * @brief Error categories for classification
 */
enum class ErrorCategory {
    BLOCKCHAIN,     ///< Blockchain-related errors
    CONSENSUS,      ///< Consensus mechanism errors
    NETWORKING,     ///< Network communication errors
    CRYPTOGRAPHY,   ///< Cryptographic errors
    STORAGE,        ///< Storage system errors
    VIRTUAL_MACHINE,///< VM execution errors
    CLI,           ///< Command line interface errors
    CONFIGURATION,  ///< Configuration errors
    VALIDATION,     ///< Data validation errors
    SYSTEM         ///< System-level errors
};

/**
 * @brief Error information structure
 */
struct ErrorInfo {
    std::string id;                     ///< Unique error ID
    ErrorSeverity severity;             ///< Error severity
    ErrorCategory category;             ///< Error category
    std::string message;                ///< Error message
    std::string file;                   ///< Source file
    int line;                          ///< Source line number
    std::string function;               ///< Function name
    std::chrono::system_clock::time_point timestamp; ///< Error timestamp
    std::map<std::string, std::string> context; ///< Additional context
    std::vector<std::string> stack_trace; ///< Stack trace
    std::string thread_id;              ///< Thread ID where error occurred
    
    ErrorInfo() : line(0) {}
};

/**
 * @brief Error handler callback function type
 */
using ErrorCallback = std::function<void(const ErrorInfo&)>;

/**
 * @brief Sophisticated error handling and debugging system
 * 
 * This class provides comprehensive error handling, logging, debugging,
 * and monitoring capabilities for the blockchain system.
 */
class ErrorHandler {
public:
    /**
     * @brief Initialize the error handling system
     * @param log_file Log file path (empty for console only)
     * @param enable_debugging Whether to enable debugging features
     */
    static void initialize(const std::string& log_file = "", bool enable_debugging = true);
    
    /**
     * @brief Shutdown the error handling system
     */
    static void shutdown();
    
    /**
     * @brief Report an error
     * @param severity Error severity
     * @param category Error category
     * @param message Error message
     * @param file Source file
     * @param line Source line number
     * @param function Function name
     * @param context Additional context information
     */
    static void reportError(ErrorSeverity severity,
                           ErrorCategory category,
                           const std::string& message,
                           const std::string& file = "",
                           int line = 0,
                           const std::string& function = "",
                           const std::map<std::string, std::string>& context = {});
    
    /**
     * @brief Register an error callback
     * @param callback Callback function
     */
    static void registerCallback(ErrorCallback callback);
    
    /**
     * @brief Unregister an error callback
     * @param callback Callback function to remove
     */
    static void unregisterCallback(ErrorCallback callback);
    
    /**
     * @brief Get error statistics
     * @return Map of error counts by category and severity
     */
    static std::map<std::string, size_t> getErrorStatistics();
    
    /**
     * @brief Get recent errors
     * @param count Number of recent errors to return
     * @return List of recent errors
     */
    static std::vector<ErrorInfo> getRecentErrors(size_t count = 100);
    
    /**
     * @brief Get all errors
     * @return Vector of error information
     */
    static std::vector<ErrorInfo> getErrors();
    
    /**
     * @brief Handle an error with message and severity
     * @param message Error message
     * @param severity Error severity
     */
    static void handleError(const std::string& message, ErrorSeverity severity);
    
    /**
     * @brief Clear error history
     */
    static void clearErrorHistory();
    
    /**
     * @brief Enable/disable error reporting
     * @param enabled Whether to enable error reporting
     */
    static void setErrorReportingEnabled(bool enabled);
    
    /**
     * @brief Set minimum severity level for reporting
     * @param severity Minimum severity level
     */
    static void setMinimumSeverity(ErrorSeverity severity);
    
    /**
     * @brief Enable/disable stack trace collection
     * @param enabled Whether to collect stack traces
     */
    static void setStackTraceEnabled(bool enabled);
    
    /**
     * @brief Enable/disable debugging mode
     * @param enabled Whether to enable debugging
     */
    static void setDebuggingEnabled(bool enabled);
    
    /**
     * @brief Get debugging information
     * @return Debugging information as JSON string
     */
    static std::string getDebugInfo();
    
    /**
     * @brief Start performance monitoring
     * @param operation_name Name of the operation to monitor
     * @return Operation ID for tracking
     */
    static std::string startPerformanceMonitoring(const std::string& operation_name);
    
    /**
     * @brief End performance monitoring
     * @param operation_id Operation ID returned by startPerformanceMonitoring
     */
    static void endPerformanceMonitoring(const std::string& operation_id);
    
    /**
     * @brief Get performance statistics
     * @return Performance statistics as JSON string
     */
    static std::string getPerformanceStatistics();
    
    /**
     * @brief Create a debug checkpoint
     * @param checkpoint_name Name of the checkpoint
     * @param data Additional debug data
     */
    static void createCheckpoint(const std::string& checkpoint_name, 
                                const std::map<std::string, std::string>& data = {});
    
    /**
     * @brief Get all debug checkpoints
     * @return List of checkpoints
     */
    static std::vector<std::pair<std::string, std::map<std::string, std::string>>> getCheckpoints();
    
    /**
     * @brief Clear debug checkpoints
     */
    static void clearCheckpoints();
    
    /**
     * @brief Trigger automatic error analysis
     */
    static void triggerErrorAnalysis();
    
    /**
     * @brief Get error analysis results
     * @return Error analysis as JSON string
     */
    static std::string getErrorAnalysis();

private:
    struct PerformanceData {
        std::string operation_name;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
        bool completed;
    };
    
    static std::unique_ptr<ErrorHandler> instance_;
    static std::mutex instance_mutex_;
    
    std::mutex mutex_;
    std::vector<ErrorInfo> error_history_;
    std::vector<ErrorCallback> callbacks_;
    std::map<std::string, size_t> error_statistics_;
    std::map<std::string, PerformanceData> performance_data_;
    std::vector<std::pair<std::string, std::map<std::string, std::string>>> checkpoints_;
    
    bool error_reporting_enabled_;
    bool stack_trace_enabled_;
    bool debugging_enabled_;
    ErrorSeverity minimum_severity_;
    std::string log_file_;
    std::ofstream log_stream_;
    
    std::thread analysis_thread_;
    std::atomic<bool> stop_analysis_;
    std::queue<std::string> analysis_queue_;
    std::condition_variable analysis_condition_;
    
public:
    ErrorHandler();
    ~ErrorHandler();
    
    /**
     * @brief Get the singleton instance
     * @return Error handler instance
     */
    static ErrorHandler& getInstance();
    
    /**
     * @brief Generate a unique error ID
     * @return Unique error ID
     */
    std::string generateErrorId();
    
    /**
     * @brief Collect stack trace
     * @return Stack trace as vector of strings
     */
    std::vector<std::string> collectStackTrace();
    
    /**
     * @brief Write error to log file
     * @param error Error information
     */
    void writeToLog(const ErrorInfo& error);
    
    /**
     * @brief Notify all registered callbacks
     * @param error Error information
     */
    void notifyCallbacks(const ErrorInfo& error);
    
    /**
     * @brief Update error statistics
     * @param error Error information
     */
    void updateStatistics(const ErrorInfo& error);
    
    /**
     * @brief Start analysis worker thread
     */
    void startAnalysisWorker();
    
    /**
     * @brief Analysis worker thread
     */
    void analysisWorker();
    
    /**
     * @brief Perform error analysis
     * @param error_id Error ID to analyze
     */
    void performErrorAnalysis(const std::string& error_id);
    
    /**
     * @brief Get thread ID as string
     * @return Thread ID string
     */
    std::string getThreadId();
    
    /**
     * @brief Get severity name as string
     * @param severity Error severity
     * @return Severity name
     */
    static std::string getSeverityName(ErrorSeverity severity);
    
    /**
     * @brief Get category name as string
     * @param category Error category
     * @return Category name
     */
    static std::string getCategoryName(ErrorCategory category);
};

// Convenience macros for error reporting
#define DEO_ERROR(category, message) \
    deo::utils::ErrorHandler::reportError( \
        deo::utils::ErrorSeverity::ERROR, \
        deo::utils::ErrorCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_WARNING(category, message) \
    deo::utils::ErrorHandler::reportError( \
        deo::utils::ErrorSeverity::WARNING, \
        deo::utils::ErrorCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_INFO(category, message) \
    deo::utils::ErrorHandler::reportError( \
        deo::utils::ErrorSeverity::INFO, \
        deo::utils::ErrorCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_DEBUG(category, message) \
    deo::utils::ErrorHandler::reportError( \
        deo::utils::ErrorSeverity::DEBUG, \
        deo::utils::ErrorCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_CRITICAL(category, message) \
    deo::utils::ErrorHandler::reportError( \
        deo::utils::ErrorSeverity::CRITICAL, \
        deo::utils::ErrorCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_FATAL(category, message) \
    deo::utils::ErrorHandler::reportError( \
        deo::utils::ErrorSeverity::FATAL, \
        deo::utils::ErrorCategory::category, \
        message, \
        __FILE__, \
        __LINE__, \
        __FUNCTION__)

#define DEO_CHECKPOINT(name) \
    deo::utils::ErrorHandler::createCheckpoint(name)

#define DEO_PERFORMANCE_START(name) \
    auto perf_id = deo::utils::ErrorHandler::startPerformanceMonitoring(name)

#define DEO_PERFORMANCE_END(id) \
    deo::utils::ErrorHandler::endPerformanceMonitoring(id)

} // namespace utils
} // namespace deo
