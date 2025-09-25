/**
 * @file performance_optimizer.cpp
 * @brief Performance optimization implementation for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "performance_optimizer.h"
#include "logging.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>

namespace deo::core {

ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex_);
                    this->condition_.wait(lock, [this] { return this->stop_ || !this->tasks_.empty(); });
                    if (this->stop_ && this->tasks_.empty()) {
                        return;
                    }
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                }
                
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    condition_.notify_all();
    for (std::thread &worker : workers_) {
        worker.join();
    }
}

PerformanceOptimizer::PerformanceOptimizer() 
    : thread_pool_(std::make_unique<ThreadPool>()), 
      monitoring_enabled_(false), 
      optimizations_enabled_(true) {
    DEO_LOG_INFO(PERFORMANCE, "Performance optimizer initialized");
}

PerformanceOptimizer::~PerformanceOptimizer() {
    if (monitoring_enabled_) {
        stopMonitoring();
    }
    DEO_LOG_INFO(PERFORMANCE, "Performance optimizer destroyed");
}

void PerformanceOptimizer::startMonitoring() {
    if (monitoring_enabled_) {
        DEO_LOG_WARNING(PERFORMANCE, "Performance monitoring already enabled");
        return;
    }
    
    monitoring_enabled_ = true;
    metrics_.start_time = std::chrono::system_clock::now();
    metrics_.last_update = std::chrono::system_clock::now();
    
    DEO_LOG_INFO(PERFORMANCE, "Performance monitoring started");
}

void PerformanceOptimizer::stopMonitoring() {
    if (!monitoring_enabled_) {
        DEO_LOG_WARNING(PERFORMANCE, "Performance monitoring not enabled");
        return;
    }
    
    monitoring_enabled_ = false;
    updateMetrics();
    
    DEO_LOG_INFO(PERFORMANCE, "Performance monitoring stopped");
    DEO_LOG_INFO(PERFORMANCE, "Final metrics - Operations: " + std::to_string(metrics_.total_operations.load()) +
                 ", Success rate: " + std::to_string(static_cast<double>(metrics_.successful_operations.load()) / 
                 std::max(metrics_.total_operations.load(), static_cast<uint64_t>(1)) * 100.0) + "%" +
                 ", Avg latency: " + std::to_string(metrics_.average_latency_ms.load()) + "ms");
}

PerformanceMetrics PerformanceOptimizer::getMetrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void PerformanceOptimizer::recordOperation(bool success, double latency_ms) {
    if (!monitoring_enabled_) {
        return;
    }
    
    metrics_.total_operations++;
    if (success) {
        metrics_.successful_operations++;
    } else {
        metrics_.failed_operations++;
    }
    
    // Update latency metrics
    double current_avg = metrics_.average_latency_ms.load();
    uint64_t total_ops = metrics_.total_operations.load();
    
    // Calculate new average latency
    double new_avg = (current_avg * (total_ops - 1) + latency_ms) / total_ops;
    metrics_.average_latency_ms.store(new_avg);
    
    // Update peak latency
    double current_peak = metrics_.peak_latency_ms.load();
    if (latency_ms > current_peak) {
        metrics_.peak_latency_ms.store(latency_ms);
    }
    
    // Update throughput
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - metrics_.start_time);
    if (duration.count() > 0) {
        double throughput = static_cast<double>(metrics_.total_operations.load()) / duration.count();
        metrics_.throughput_ops_per_sec.store(throughput);
    }
    
    // Update last update time
    metrics_.last_update = now;
}

void PerformanceOptimizer::setCacheSize(const std::string& cache_name, size_t size) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    if (caches_.find(cache_name) != caches_.end()) {
        // Recreate cache with new size
        caches_[cache_name] = std::make_unique<PerformanceCache<std::string, std::string>>(size);
        DEO_LOG_INFO(PERFORMANCE, "Cache '" + cache_name + "' size set to " + std::to_string(size));
    }
}

void PerformanceOptimizer::setThreadPoolSize(size_t num_threads) {
    if (thread_pool_) {
        thread_pool_->shutdown();
    }
    thread_pool_ = std::make_unique<ThreadPool>(num_threads);
    DEO_LOG_INFO(PERFORMANCE, "Thread pool size set to " + std::to_string(num_threads));
}

void PerformanceOptimizer::enableOptimizations(bool enable) {
    optimizations_enabled_ = enable;
    DEO_LOG_INFO(PERFORMANCE, "Optimizations " + std::string(enable ? "enabled" : "disabled"));
}

void PerformanceOptimizer::optimizeMemoryUsage() {
    if (!optimizations_enabled_) {
        return;
    }
    
    // Clear expired cache entries
    cleanupExpiredCacheEntries();
    
    // Force garbage collection if possible
    // Optimize memory usage
    DEO_LOG_DEBUG(PERFORMANCE, "Memory optimization completed");
}

size_t PerformanceOptimizer::getMemoryUsage() const {
    // Calculate actual memory usage
    // In a real implementation, you would use platform-specific APIs
    size_t total_memory = 0;
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    for (const auto& cache_pair : caches_) {
        total_memory += cache_pair.second->size() * 1024; // Rough estimate
    }
    
    return total_memory;
}

ThreadPool& PerformanceOptimizer::getThreadPool() {
    return *thread_pool_;
}

void PerformanceOptimizer::updateMetrics() {
    if (!monitoring_enabled_) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - metrics_.start_time);
    
    if (duration.count() > 0) {
        double throughput = static_cast<double>(metrics_.total_operations.load()) / duration.count();
        metrics_.throughput_ops_per_sec.store(throughput);
    }
}

void PerformanceOptimizer::cleanupExpiredCacheEntries() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    for (auto& cache_pair : caches_) {
        // Simple cleanup - clear cache if it's getting too large
        if (cache_pair.second->size() > 1000) {
            cache_pair.second->clear();
            DEO_LOG_DEBUG(PERFORMANCE, "Cleared cache: " + cache_pair.first);
        }
    }
}

} // namespace deo::core
