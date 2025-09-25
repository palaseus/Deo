/**
 * @file performance_monitor.cpp
 * @brief Performance monitoring and profiling system implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "utils/performance_monitor.h"
#include "utils/logger.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <sys/resource.h>
#include <unistd.h>

namespace deo {
namespace utils {

// PerformanceProfiler implementation
PerformanceProfiler::PerformanceProfiler(const std::string& operation_name)
    : operation_name_(operation_name), stopped_(false) {
    start_time_ = std::chrono::high_resolution_clock::now();
}

PerformanceProfiler::~PerformanceProfiler() {
    if (!stopped_) {
        stop();
    }
}

void PerformanceProfiler::stop() {
    if (!stopped_) {
        stopped_ = true;
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
        
        PerformanceMonitor::getInstance().stopProfiling(operation_name_);
    }
}

uint64_t PerformanceProfiler::getElapsedMs() const {
    if (stopped_) {
        return 0;
    }
    
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time_);
    return elapsed.count();
}

// PerformanceMonitor implementation
PerformanceMonitor& PerformanceMonitor::getInstance() {
    static PerformanceMonitor instance;
    return instance;
}

void PerformanceMonitor::startProfiling(const std::string& operation_name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto& metrics = metrics_[operation_name];
    metrics.operation_name = operation_name;
    metrics.last_call_time = std::chrono::system_clock::now();
}

void PerformanceMonitor::stopProfiling(const std::string& operation_name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(operation_name);
    if (it != metrics_.end()) {
        auto& metrics = it->second;
        auto current_time = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - metrics.last_call_time);
        
        uint64_t elapsed_ms = elapsed.count();
        
        metrics.call_count++;
        metrics.total_time_ms += elapsed_ms;
        metrics.last_time_ms = elapsed_ms;
        
        if (elapsed_ms < metrics.min_time_ms) {
            metrics.min_time_ms = elapsed_ms;
        }
        if (elapsed_ms > metrics.max_time_ms) {
            metrics.max_time_ms = elapsed_ms;
        }
        
        metrics.avg_time_ms = metrics.total_time_ms / metrics.call_count;
        
        // Check performance threshold
        auto threshold_it = performance_thresholds_.find(operation_name);
        if (threshold_it != performance_thresholds_.end() && elapsed_ms > threshold_it->second) {
            DEO_LOG_WARNING(PERFORMANCE, "Performance threshold exceeded for " + operation_name + 
                           ": " + std::to_string(elapsed_ms) + "ms > " + std::to_string(threshold_it->second) + "ms");
        }
    }
}

PerformanceMetrics PerformanceMonitor::getMetrics(const std::string& operation_name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(operation_name);
    if (it != metrics_.end()) {
        return it->second;
    }
    
    return PerformanceMetrics();
}

std::map<std::string, PerformanceMetrics> PerformanceMonitor::getAllMetrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void PerformanceMonitor::resetMetrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.clear();
}

void PerformanceMonitor::resetMetrics(const std::string& operation_name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.erase(operation_name);
}

std::string PerformanceMonitor::generateReport() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::stringstream ss;
    ss << "=== Performance Report ===\n";
    ss << "Generated at: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
    
    if (metrics_.empty()) {
        ss << "No performance data available.\n";
        return ss.str();
    }
    
    // Sort by total time (descending)
    std::vector<std::pair<std::string, PerformanceMetrics>> sorted_metrics(metrics_.begin(), metrics_.end());
    std::sort(sorted_metrics.begin(), sorted_metrics.end(),
              [](const auto& a, const auto& b) {
                  return a.second.total_time_ms > b.second.total_time_ms;
              });
    
    ss << std::left << std::setw(30) << "Operation" 
       << std::setw(10) << "Calls" 
       << std::setw(12) << "Total(ms)" 
       << std::setw(12) << "Avg(ms)" 
       << std::setw(12) << "Min(ms)" 
       << std::setw(12) << "Max(ms)" << "\n";
    ss << std::string(90, '-') << "\n";
    
    for (const auto& pair : sorted_metrics) {
        const auto& metrics = pair.second;
        ss << std::left << std::setw(30) << metrics.operation_name
           << std::setw(10) << metrics.call_count
           << std::setw(12) << metrics.total_time_ms
           << std::setw(12) << metrics.avg_time_ms
           << std::setw(12) << metrics.min_time_ms
           << std::setw(12) << metrics.max_time_ms << "\n";
    }
    
    ss << "\n=== System Metrics ===\n";
    ss << "Memory Usage: " << total_memory_usage_.load() << " bytes\n";
    ss << "CPU Usage: " << std::fixed << std::setprecision(2) << cpu_usage_.load() << "%\n";
    ss << "Network Sent: " << network_bytes_sent_.load() << " bytes\n";
    ss << "Network Received: " << network_bytes_received_.load() << " bytes\n";
    
    return ss.str();
}

void PerformanceMonitor::saveReport(const std::string& filename) const {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << generateReport();
        file.close();
        DEO_LOG_INFO(PERFORMANCE, "Performance report saved to: " + filename);
    } else {
        DEO_LOG_ERROR(PERFORMANCE, "Failed to save performance report to: " + filename);
    }
}

void PerformanceMonitor::startMonitoring() {
    if (!monitoring_active_.exchange(true)) {
        monitoring_thread_ = std::thread(&PerformanceMonitor::monitoringWorker, this);
        DEO_LOG_INFO(PERFORMANCE, "Performance monitoring started");
    }
}

void PerformanceMonitor::stopMonitoring() {
    if (monitoring_active_.exchange(false)) {
        monitoring_condition_.notify_all();
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
        DEO_LOG_INFO(PERFORMANCE, "Performance monitoring stopped");
    }
}

void PerformanceMonitor::recordMemoryUsage(size_t bytes) {
    total_memory_usage_.store(bytes);
}

void PerformanceMonitor::recordCpuUsage(double percentage) {
    cpu_usage_.store(percentage);
}

void PerformanceMonitor::recordNetworkBytes(size_t bytes_sent, size_t bytes_received) {
    network_bytes_sent_.fetch_add(bytes_sent);
    network_bytes_received_.fetch_add(bytes_received);
}

void PerformanceMonitor::setPerformanceThreshold(const std::string& operation_name, uint64_t threshold_ms) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    performance_thresholds_[operation_name] = threshold_ms;
}

bool PerformanceMonitor::isPerformanceThresholdExceeded(const std::string& operation_name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto threshold_it = performance_thresholds_.find(operation_name);
    if (threshold_it == performance_thresholds_.end()) {
        return false;
    }
    
    auto metrics_it = metrics_.find(operation_name);
    if (metrics_it == metrics_.end()) {
        return false;
    }
    
    return metrics_it->second.avg_time_ms > threshold_it->second;
}

void PerformanceMonitor::monitoringWorker() {
    while (monitoring_active_.load()) {
        updateSystemMetrics();
        
        std::unique_lock<std::mutex> lock(monitoring_mutex_);
        monitoring_condition_.wait_for(lock, std::chrono::seconds(1));
    }
}

void PerformanceMonitor::updateSystemMetrics() {
    // Update memory usage
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        recordMemoryUsage(usage.ru_maxrss * 1024); // Convert from KB to bytes
    }
    
    // Update CPU usage (simplified)
    static auto last_time = std::chrono::high_resolution_clock::now();
    static auto last_cpu_time = std::clock();
    
    auto current_time = std::chrono::high_resolution_clock::now();
    auto current_cpu_time = std::clock();
    
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time).count();
    auto cpu_diff = current_cpu_time - last_cpu_time;
    
    if (time_diff > 0) {
        double cpu_percentage = (cpu_diff * 100.0) / (time_diff * CLOCKS_PER_SEC / 1000.0);
        recordCpuUsage(std::min(100.0, std::max(0.0, cpu_percentage)));
    }
    
    last_time = current_time;
    last_cpu_time = current_cpu_time;
}

// ScopedProfiler implementation
ScopedProfiler::ScopedProfiler(const std::string& operation_name)
    : operation_name_(operation_name) {
    PerformanceMonitor::getInstance().startProfiling(operation_name);
}

ScopedProfiler::~ScopedProfiler() {
    PerformanceMonitor::getInstance().stopProfiling(operation_name_);
}

} // namespace utils
} // namespace deo
