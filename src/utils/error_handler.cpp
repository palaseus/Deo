/**
 * @file error_handler.cpp
 * @brief Sophisticated error handling and debugging system implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "utils/error_handler.h"
#include "utils/logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <random>
#include <execinfo.h>
#include <unistd.h>

namespace deo {
namespace utils {

std::unique_ptr<ErrorHandler> ErrorHandler::instance_ = nullptr;
std::mutex ErrorHandler::instance_mutex_;

void ErrorHandler::initialize(const std::string& log_file, bool enable_debugging) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::make_unique<ErrorHandler>();
        instance_->log_file_ = log_file;
        instance_->debugging_enabled_ = enable_debugging;
        
        if (!log_file.empty()) {
            instance_->log_stream_.open(log_file, std::ios::app);
        }
        
        instance_->startAnalysisWorker();
        
        DEO_LOG_INFO(GENERAL, "Error handling system initialized");
    }
}

void ErrorHandler::shutdown() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->stop_analysis_ = true;
        instance_->analysis_condition_.notify_all();
        
        if (instance_->analysis_thread_.joinable()) {
            instance_->analysis_thread_.join();
        }
        
        if (instance_->log_stream_.is_open()) {
            instance_->log_stream_.close();
        }
        
        instance_.reset();
        
        DEO_LOG_INFO(GENERAL, "Error handling system shutdown");
    }
}

void ErrorHandler::reportError(ErrorSeverity severity,
                              ErrorCategory category,
                              const std::string& message,
                              const std::string& file,
                              int line,
                              const std::string& function,
                              const std::map<std::string, std::string>& context) {
    auto& instance = getInstance();
    
    ErrorInfo error;
    error.id = instance.generateErrorId();
    error.severity = severity;
    error.category = category;
    error.message = message;
    error.file = file;
    error.line = line;
    error.function = function;
    error.timestamp = std::chrono::system_clock::now();
    error.context = context;
    error.thread_id = instance.getThreadId();
    
    if (instance.stack_trace_enabled_) {
        error.stack_trace = instance.collectStackTrace();
    }
    
    instance.writeToLog(error);
    instance.notifyCallbacks(error);
    instance.updateStatistics(error);
    
    // Add to recent errors
    {
        std::lock_guard<std::mutex> lock(instance.mutex_);
        instance.error_history_.push_back(error);
        
        // Keep only the last 1000 errors
        if (instance.error_history_.size() > 1000) {
            instance.error_history_.erase(instance.error_history_.begin());
        }
    }
    
    // Trigger analysis for critical errors
    if (severity >= ErrorSeverity::CRITICAL) {
        instance.triggerErrorAnalysis();
    }
}

void ErrorHandler::registerCallback(ErrorCallback callback) {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    instance.callbacks_.push_back(callback);
}

void ErrorHandler::unregisterCallback(ErrorCallback callback) {
    // Note: std::function objects cannot be compared for equality
    // This is a known limitation. Consider using a different callback management approach
    // For now, this function is a no-op to allow compilation
    (void)callback; // Suppress unused parameter warning
}

std::map<std::string, size_t> ErrorHandler::getErrorStatistics() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    return instance.error_statistics_;
}

std::vector<ErrorInfo> ErrorHandler::getRecentErrors(size_t count) {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    
    size_t start = (instance.error_history_.size() > count) ? 
                   instance.error_history_.size() - count : 0;
    
    return std::vector<ErrorInfo>(instance.error_history_.begin() + start, 
                                  instance.error_history_.end());
}

void ErrorHandler::clearErrorHistory() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    instance.error_history_.clear();
    instance.error_statistics_.clear();
}

void ErrorHandler::setErrorReportingEnabled(bool enabled) {
    auto& instance = getInstance();
    instance.error_reporting_enabled_ = enabled;
}

void ErrorHandler::setMinimumSeverity(ErrorSeverity severity) {
    auto& instance = getInstance();
    instance.minimum_severity_ = severity;
}

void ErrorHandler::setStackTraceEnabled(bool enabled) {
    auto& instance = getInstance();
    instance.stack_trace_enabled_ = enabled;
}

void ErrorHandler::setDebuggingEnabled(bool enabled) {
    auto& instance = getInstance();
    instance.debugging_enabled_ = enabled;
}

std::string ErrorHandler::getDebugInfo() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"error_count\": " << instance.error_history_.size() << ",\n";
    ss << "  \"checkpoints\": " << instance.checkpoints_.size() << ",\n";
    ss << "  \"performance_operations\": " << instance.performance_data_.size() << ",\n";
    ss << "  \"debugging_enabled\": " << (instance.debugging_enabled_ ? "true" : "false") << ",\n";
    ss << "  \"stack_trace_enabled\": " << (instance.stack_trace_enabled_ ? "true" : "false") << "\n";
    ss << "}";
    
    return ss.str();
}

std::string ErrorHandler::startPerformanceMonitoring(const std::string& operation_name) {
    auto& instance = getInstance();
    std::string operation_id = instance.generateErrorId();
    
    PerformanceData data;
    data.operation_name = operation_name;
    data.start_time = std::chrono::system_clock::now();
    data.completed = false;
    
    instance.performance_data_[operation_id] = data;
    
    return operation_id;
}

void ErrorHandler::endPerformanceMonitoring(const std::string& operation_id) {
    auto& instance = getInstance();
    auto it = instance.performance_data_.find(operation_id);
    if (it != instance.performance_data_.end()) {
        it->second.end_time = std::chrono::system_clock::now();
        it->second.completed = true;
    }
}

std::string ErrorHandler::getPerformanceStatistics() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"operations\": [\n";
    
    bool first = true;
    for (const auto& pair : instance.performance_data_) {
        if (!first) ss << ",\n";
        first = false;
        
        const auto& data = pair.second;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            data.end_time - data.start_time).count();
        
        ss << "    {\n";
        ss << "      \"id\": \"" << pair.first << "\",\n";
        ss << "      \"operation\": \"" << data.operation_name << "\",\n";
        ss << "      \"duration_ms\": " << duration << ",\n";
        ss << "      \"completed\": " << (data.completed ? "true" : "false") << "\n";
        ss << "    }";
    }
    
    ss << "\n  ]\n";
    ss << "}";
    
    return ss.str();
}

void ErrorHandler::createCheckpoint(const std::string& checkpoint_name, 
                                   const std::map<std::string, std::string>& data) {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    instance.checkpoints_.emplace_back(checkpoint_name, data);
}

std::vector<std::pair<std::string, std::map<std::string, std::string>>> ErrorHandler::getCheckpoints() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    return instance.checkpoints_;
}

void ErrorHandler::clearCheckpoints() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    instance.checkpoints_.clear();
}

void ErrorHandler::triggerErrorAnalysis() {
    auto& instance = getInstance();
    std::string error_id = instance.generateErrorId();
    
    {
        std::lock_guard<std::mutex> lock(instance.mutex_);
        instance.analysis_queue_.push(error_id);
    }
    
    instance.analysis_condition_.notify_one();
}

std::string ErrorHandler::getErrorAnalysis() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"total_errors\": " << instance.error_history_.size() << ",\n";
    ss << "  \"error_categories\": {\n";
    
    bool first = true;
    for (const auto& pair : instance.error_statistics_) {
        if (!first) ss << ",\n";
        first = false;
        ss << "    \"" << pair.first << "\": " << pair.second;
    }
    
    ss << "\n  }\n";
    ss << "}";
    
    return ss.str();
}

ErrorHandler::ErrorHandler() 
    : error_reporting_enabled_(true)
    , stack_trace_enabled_(true)
    , debugging_enabled_(true)
    , minimum_severity_(ErrorSeverity::DEBUG)
    , stop_analysis_(false) {
}

ErrorHandler::~ErrorHandler() {
    if (analysis_thread_.joinable()) {
        stop_analysis_ = true;
        analysis_condition_.notify_all();
        analysis_thread_.join();
    }
}

ErrorHandler& ErrorHandler::getInstance() {
    if (!instance_) {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_) {
            instance_ = std::make_unique<ErrorHandler>();
        }
    }
    return *instance_;
}

std::string ErrorHandler::generateErrorId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; ++i) {
        ss << dis(gen);
    }
    
    return ss.str();
}

std::vector<std::string> ErrorHandler::collectStackTrace() {
    std::vector<std::string> stack_trace;
    
    void* array[10];
    size_t size = backtrace(array, 10);
    char** strings = backtrace_symbols(array, size);
    
    for (size_t i = 0; i < size; ++i) {
        stack_trace.push_back(strings[i]);
    }
    
    free(strings);
    return stack_trace;
}

void ErrorHandler::writeToLog(const ErrorInfo& error) {
    if (!error_reporting_enabled_ || error.severity < minimum_severity_) {
        return;
    }
    
    std::stringstream ss;
    auto time_t = std::chrono::system_clock::to_time_t(error.timestamp);
    ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";
    ss << "[" << ErrorHandler::getSeverityName(error.severity) << "] ";
    ss << "[" << ErrorHandler::getCategoryName(error.category) << "] ";
    ss << error.message;
    
    if (!error.file.empty()) {
        ss << " (" << error.file << ":" << error.line;
        if (!error.function.empty()) {
            ss << " in " << error.function;
        }
        ss << ")";
    }
    
    std::string log_message = ss.str();
    
    // Write to console
    if (error.severity >= ErrorSeverity::ERROR) {
        std::cerr << log_message << std::endl;
    } else {
        std::cout << log_message << std::endl;
    }
    
    // Write to file
    if (log_stream_.is_open()) {
        log_stream_ << log_message << std::endl;
        log_stream_.flush();
    }
}

void ErrorHandler::notifyCallbacks(const ErrorInfo& error) {
    for (const auto& callback : callbacks_) {
        try {
            callback(error);
        } catch (const std::exception& e) {
            // Don't let callback errors propagate
            std::cerr << "Error in error callback: " << e.what() << std::endl;
        }
    }
}

void ErrorHandler::updateStatistics(const ErrorInfo& error) {
    std::string key = ErrorHandler::getSeverityName(error.severity) + "_" + ErrorHandler::getCategoryName(error.category);
    error_statistics_[key]++;
}

void ErrorHandler::startAnalysisWorker() {
    analysis_thread_ = std::thread([this]() { analysisWorker(); });
}

void ErrorHandler::analysisWorker() {
    while (!stop_analysis_) {
        std::unique_lock<std::mutex> lock(mutex_);
        analysis_condition_.wait(lock, [this] { return !analysis_queue_.empty() || stop_analysis_; });
        
        if (stop_analysis_) break;
        
        if (!analysis_queue_.empty()) {
            std::string error_id = analysis_queue_.front();
            analysis_queue_.pop();
            lock.unlock();
            
            performErrorAnalysis(error_id);
            lock.lock();
        }
    }
}

void ErrorHandler::performErrorAnalysis(const std::string& error_id) {
    // Perform error analysis logic here
    // This is a placeholder implementation
    DEO_LOG_DEBUG(GENERAL, "Performing error analysis for: " + error_id);
}

std::string ErrorHandler::getThreadId() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

std::string ErrorHandler::getSeverityName(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::DEBUG: return "DEBUG";
        case ErrorSeverity::INFO: return "INFO";
        case ErrorSeverity::WARNING: return "WARNING";
        case ErrorSeverity::ERROR: return "ERROR";
        case ErrorSeverity::CRITICAL: return "CRITICAL";
        case ErrorSeverity::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string ErrorHandler::getCategoryName(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::BLOCKCHAIN: return "BLOCKCHAIN";
        case ErrorCategory::CONSENSUS: return "CONSENSUS";
        case ErrorCategory::NETWORKING: return "NETWORKING";
        case ErrorCategory::CRYPTOGRAPHY: return "CRYPTOGRAPHY";
        case ErrorCategory::STORAGE: return "STORAGE";
        case ErrorCategory::VIRTUAL_MACHINE: return "VM";
        case ErrorCategory::CLI: return "CLI";
        case ErrorCategory::CONFIGURATION: return "CONFIG";
        case ErrorCategory::VALIDATION: return "VALIDATION";
        case ErrorCategory::SYSTEM: return "SYSTEM";
        default: return "UNKNOWN";
    }
}

std::vector<ErrorInfo> ErrorHandler::getErrors() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    return instance.error_history_;
}

void ErrorHandler::handleError(const std::string& message, ErrorSeverity severity) {
    ErrorInfo error_info;
    error_info.message = message;
    error_info.severity = severity;
    error_info.category = ErrorCategory::SYSTEM;
    error_info.timestamp = std::chrono::system_clock::now();
    std::stringstream ss;
    ss << std::this_thread::get_id();
    error_info.thread_id = ss.str();
    
    reportError(error_info.severity, error_info.category, message);
}

} // namespace utils
} // namespace deo
