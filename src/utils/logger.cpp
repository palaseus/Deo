/**
 * @file logger.cpp
 * @brief Advanced logging system implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "utils/logger.h"
#include "utils/error_handler.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <filesystem>

namespace deo {
namespace utils {

std::unique_ptr<Logger> Logger::instance_ = nullptr;
std::mutex Logger::instance_mutex_;

void Logger::initialize(LogLevel level, const std::string& log_file, bool enable_async) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::make_unique<Logger>();
        instance_->config_.min_level = level;
        instance_->config_.log_file = log_file;
        instance_->config_.async_enabled = enable_async;
        
        if (!log_file.empty()) {
            instance_->log_stream_.open(log_file, std::ios::app);
            instance_->config_.file_enabled = true;
        }
        
        if (enable_async) {
            instance_->startAsyncWorker();
        }
        
        DEO_LOG_INFO(GENERAL, "Logging system initialized");
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->stop_async_ = true;
        instance_->log_condition_.notify_all();
        
        if (instance_->async_thread_.joinable()) {
            instance_->async_thread_.join();
        }
        
        if (instance_->log_stream_.is_open()) {
            instance_->log_stream_.close();
        }
        
        instance_.reset();
    }
}

void Logger::log(LogLevel level,
                LogCategory category,
                const std::string& message,
                const std::string& file,
                int line,
                const std::string& function,
                const std::map<std::string, std::string>& context) {
    auto& instance = getInstance();
    
    if (level < instance.config_.min_level) {
        return;
    }
    
    LogEntry entry;
    entry.level = level;
    entry.category = category;
    entry.message = message;
    entry.file = file;
    entry.line = line;
    entry.function = function;
    entry.timestamp = std::chrono::system_clock::now();
    entry.thread_id = instance.getThreadId();
    entry.context = context;
    
    if (instance.config_.async_enabled) {
        std::lock_guard<std::mutex> lock(instance.mutex_);
        instance.log_queue_.push(entry);
        instance.log_condition_.notify_one();
    } else {
        instance.writeLog(entry);
    }
    
    instance.updateStatistics(entry);
    instance.addToRecentEntries(entry);
}

void Logger::setLevel(LogLevel level) {
    auto& instance = getInstance();
    instance.config_.min_level = level;
}

void Logger::setFormatter(std::unique_ptr<LogFormatter> formatter) {
    auto& instance = getInstance();
    instance.formatter_ = std::move(formatter);
}

void Logger::setCategoryEnabled(LogCategory category, bool enabled) {
    auto& instance = getInstance();
    instance.config_.category_enabled[category] = enabled;
}

void Logger::setConsoleEnabled(bool enabled) {
    auto& instance = getInstance();
    instance.config_.console_enabled = enabled;
}

void Logger::setFileEnabled(bool enabled) {
    auto& instance = getInstance();
    instance.config_.file_enabled = enabled;
}

void Logger::setLogFile(const std::string& file_path) {
    auto& instance = getInstance();
    instance.config_.log_file = file_path;
    
    if (instance.log_stream_.is_open()) {
        instance.log_stream_.close();
    }
    
    if (!file_path.empty()) {
        instance.log_stream_.open(file_path, std::ios::app);
    }
}

void Logger::setAsyncEnabled(bool enabled) {
    auto& instance = getInstance();
    if (enabled && !instance.config_.async_enabled) {
        instance.startAsyncWorker();
    } else if (!enabled && instance.config_.async_enabled) {
        instance.stop_async_ = true;
        instance.log_condition_.notify_all();
        if (instance.async_thread_.joinable()) {
            instance.async_thread_.join();
        }
    }
    
    instance.config_.async_enabled = enabled;
}

void Logger::flush() {
    auto& instance = getInstance();
    if (instance.log_stream_.is_open()) {
        instance.log_stream_.flush();
    }
}

std::map<std::string, size_t> Logger::getStatistics() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    return instance.statistics_;
}

void Logger::clearStatistics() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    instance.statistics_.clear();
}

std::vector<LogEntry> Logger::getRecentEntries(size_t count) {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    
    size_t start = (instance.recent_entries_.size() > count) ? 
                   instance.recent_entries_.size() - count : 0;
    
    return std::vector<LogEntry>(instance.recent_entries_.begin() + start, 
                                 instance.recent_entries_.end());
}

void Logger::setLogRotation(bool enabled, size_t max_size, size_t max_files) {
    auto& instance = getInstance();
    instance.config_.rotation_enabled = enabled;
    instance.config_.max_file_size = max_size;
    instance.config_.max_files = max_files;
}

std::map<std::string, std::string> Logger::createContext() {
    return std::map<std::string, std::string>();
}

Logger::Logger() : stop_async_(false) {
    formatter_ = std::make_unique<DefaultFormatter>();
    
    // Initialize category enabled map
    for (int i = 0; i < 11; ++i) { // Number of categories
        config_.category_enabled[static_cast<LogCategory>(i)] = true;
    }
}

Logger::~Logger() {
    if (async_thread_.joinable()) {
        stop_async_ = true;
        log_condition_.notify_all();
        async_thread_.join();
    }
}

Logger& Logger::getInstance() {
    if (!instance_) {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_) {
            instance_ = std::make_unique<Logger>();
        }
    }
    return *instance_;
}

void Logger::writeLog(const LogEntry& entry) {
    if (!config_.category_enabled[entry.category]) {
        return;
    }
    
    std::string formatted_message = formatter_->format(entry);
    
    if (config_.console_enabled) {
        writeToConsole(entry);
    }
    
    if (config_.file_enabled) {
        writeToFile(entry);
    }
    
    if (config_.rotation_enabled) {
        rotateLogFile();
    }
}

void Logger::writeToConsole(const LogEntry& entry) {
    std::string formatted_message = formatter_->format(entry);
    
    if (entry.level >= LogLevel::ERROR) {
        std::cerr << formatted_message << std::endl;
    } else {
        std::cout << formatted_message << std::endl;
    }
}

void Logger::writeToFile(const LogEntry& entry) {
    if (log_stream_.is_open()) {
        std::string formatted_message = formatter_->format(entry);
        log_stream_ << formatted_message << std::endl;
        log_stream_.flush();
    }
}

void Logger::asyncWorker() {
    while (!stop_async_) {
        std::unique_lock<std::mutex> lock(mutex_);
        log_condition_.wait(lock, [this] { return !log_queue_.empty() || stop_async_; });
        
        if (stop_async_) break;
        
        while (!log_queue_.empty()) {
            LogEntry entry = log_queue_.front();
            log_queue_.pop();
            lock.unlock();
            
            writeLog(entry);
            lock.lock();
        }
    }
}

void Logger::rotateLogFile() {
    if (!log_stream_.is_open()) return;
    
    // Check file size
    std::ifstream file(config_.log_file, std::ios::ate | std::ios::binary);
    if (file.is_open()) {
        size_t file_size = file.tellg();
        file.close();
        
        if (file_size >= config_.max_file_size) {
            log_stream_.close();
            
            // Rotate existing files
            for (int i = config_.max_files - 1; i > 0; --i) {
                std::string old_file = config_.log_file + "." + std::to_string(i);
                std::string new_file = config_.log_file + "." + std::to_string(i + 1);
                
                if (std::filesystem::exists(old_file)) {
                    std::filesystem::rename(old_file, new_file);
                }
            }
            
            // Move current file to .1
            std::string rotated_file = config_.log_file + ".1";
            std::filesystem::rename(config_.log_file, rotated_file);
            
            // Open new log file
            log_stream_.open(config_.log_file, std::ios::app);
        }
    }
}

std::string Logger::getLevelName(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCategoryName(LogCategory category) const {
    switch (category) {
        case LogCategory::GENERAL: return "GENERAL";
        case LogCategory::BLOCKCHAIN: return "BLOCKCHAIN";
        case LogCategory::CONSENSUS: return "CONSENSUS";
        case LogCategory::NETWORKING: return "NETWORKING";
        case LogCategory::CRYPTOGRAPHY: return "CRYPTO";
        case LogCategory::STORAGE: return "STORAGE";
        case LogCategory::VIRTUAL_MACHINE: return "VM";
        case LogCategory::CLI: return "CLI";
        case LogCategory::CONFIGURATION: return "CONFIG";
        case LogCategory::PERFORMANCE: return "PERF";
        case LogCategory::SECURITY: return "SECURITY";
        default: return "UNKNOWN";
    }
}

std::string Logger::getThreadId() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

void Logger::updateStatistics(const LogEntry& entry) {
    std::string key = getLevelName(entry.level) + "_" + getCategoryName(entry.category);
    statistics_[key]++;
}

void Logger::addToRecentEntries(const LogEntry& entry) {
    recent_entries_.push_back(entry);
    
    // Keep only the last 1000 entries
    if (recent_entries_.size() > 1000) {
        recent_entries_.erase(recent_entries_.begin());
    }
}

void Logger::startAsyncWorker() {
    async_thread_ = std::thread([this]() { asyncWorker(); });
}

std::string DefaultFormatter::format(const LogEntry& entry) {
    std::stringstream ss;
    
    // Timestamp
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";
    
    // Level
    ss << "[" << Logger::getInstance().getLevelName(entry.level) << "] ";
    
    // Category
    ss << "[" << Logger::getInstance().getCategoryName(entry.category) << "] ";
    
    // Thread ID
    ss << "[" << entry.thread_id << "] ";
    
    // Message
    ss << entry.message;
    
    // File and line
    if (!entry.file.empty()) {
        ss << " (" << entry.file << ":" << entry.line;
        if (!entry.function.empty()) {
            ss << " in " << entry.function;
        }
        ss << ")";
    }
    
    // Context
    if (!entry.context.empty()) {
        ss << " [";
        bool first = true;
        for (const auto& pair : entry.context) {
            if (!first) ss << ", ";
            ss << pair.first << "=" << pair.second;
            first = false;
        }
        ss << "]";
    }
    
    return ss.str();
}

std::string JsonFormatter::format(const LogEntry& entry) {
    std::stringstream ss;
    
    ss << "{\n";
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    ss << "  \"timestamp\": \"" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\",\n";
    ss << "  \"level\": \"" << Logger::getInstance().getLevelName(entry.level) << "\",\n";
    ss << "  \"category\": \"" << Logger::getInstance().getCategoryName(entry.category) << "\",\n";
    ss << "  \"thread_id\": \"" << entry.thread_id << "\",\n";
    ss << "  \"message\": \"" << entry.message << "\",\n";
    
    if (!entry.file.empty()) {
        ss << "  \"file\": \"" << entry.file << "\",\n";
        ss << "  \"line\": " << entry.line << ",\n";
        if (!entry.function.empty()) {
            ss << "  \"function\": \"" << entry.function << "\",\n";
        }
    }
    
    if (!entry.context.empty()) {
        ss << "  \"context\": {\n";
        bool first = true;
        for (const auto& pair : entry.context) {
            if (!first) ss << ",\n";
            ss << "    \"" << pair.first << "\": \"" << pair.second << "\"";
            first = false;
        }
        ss << "\n  }\n";
    }
    
    ss << "}";
    
    return ss.str();
}

std::vector<LogEntry> Logger::getLogs() {
    auto& instance = getInstance();
    std::lock_guard<std::mutex> lock(instance.mutex_);
    return instance.recent_entries_;
}

void Logger::log(const std::string& message, LogLevel level) {
    log(level, LogCategory::GENERAL, message);
}

} // namespace utils
} // namespace deo
