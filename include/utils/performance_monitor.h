/**
 * @file performance_monitor.h
 * @brief Performance monitoring and profiling system
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <chrono>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>
#include <condition_variable>

namespace deo {
namespace utils {

/**
 * @brief Performance metrics for a specific operation
 */
struct PerformanceMetrics {
    std::string operation_name;
    uint64_t call_count;
    uint64_t total_time_ms;
    uint64_t min_time_ms;
    uint64_t max_time_ms;
    uint64_t avg_time_ms;
    uint64_t last_time_ms;
    std::chrono::system_clock::time_point last_call_time;
    
    PerformanceMetrics() : call_count(0), total_time_ms(0), min_time_ms(UINT64_MAX), 
                          max_time_ms(0), avg_time_ms(0), last_time_ms(0) {}
};

/**
 * @brief Performance profiler for timing operations
 */
class PerformanceProfiler {
public:
    PerformanceProfiler(const std::string& operation_name);
    ~PerformanceProfiler();
    
    void stop();
    uint64_t getElapsedMs() const;
    
private:
    std::string operation_name_;
    std::chrono::high_resolution_clock::time_point start_time_;
    bool stopped_;
};

/**
 * @brief Performance monitor for tracking system metrics
 */
class PerformanceMonitor {
public:
    static PerformanceMonitor& getInstance();
    
    void startProfiling(const std::string& operation_name);
    void stopProfiling(const std::string& operation_name);
    
    PerformanceMetrics getMetrics(const std::string& operation_name) const;
    std::map<std::string, PerformanceMetrics> getAllMetrics() const;
    
    void resetMetrics();
    void resetMetrics(const std::string& operation_name);
    
    std::string generateReport() const;
    void saveReport(const std::string& filename) const;
    
    void startMonitoring();
    void stopMonitoring();
    
    // System resource monitoring
    void recordMemoryUsage(size_t bytes);
    void recordCpuUsage(double percentage);
    void recordNetworkBytes(size_t bytes_sent, size_t bytes_received);
    
    // Performance thresholds
    void setPerformanceThreshold(const std::string& operation_name, uint64_t threshold_ms);
    bool isPerformanceThresholdExceeded(const std::string& operation_name) const;
    
private:
    PerformanceMonitor() = default;
    ~PerformanceMonitor() = default;
    
    // Disable copy and move
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
    PerformanceMonitor(PerformanceMonitor&&) = delete;
    PerformanceMonitor& operator=(PerformanceMonitor&&) = delete;
    
    mutable std::mutex metrics_mutex_;
    std::map<std::string, PerformanceMetrics> metrics_;
    std::map<std::string, uint64_t> performance_thresholds_;
    
    // System monitoring
    std::atomic<bool> monitoring_active_;
    std::thread monitoring_thread_;
    std::condition_variable monitoring_condition_;
    std::mutex monitoring_mutex_;
    
    // System metrics
    std::atomic<uint64_t> total_memory_usage_;
    std::atomic<double> cpu_usage_;
    std::atomic<uint64_t> network_bytes_sent_;
    std::atomic<uint64_t> network_bytes_received_;
    
    void monitoringWorker();
    void updateSystemMetrics();
};

/**
 * @brief RAII performance profiler
 */
class ScopedProfiler {
public:
    ScopedProfiler(const std::string& operation_name);
    ~ScopedProfiler();
    
private:
    std::string operation_name_;
};

// Convenience macros
#define DEO_PROFILE(operation_name) ScopedProfiler _profiler(operation_name)
#define DEO_PROFILE_FUNCTION() ScopedProfiler _profiler(__FUNCTION__)

} // namespace utils
} // namespace deo
