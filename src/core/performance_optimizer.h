/**
 * @file performance_optimizer.h
 * @brief Performance optimization utilities for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#ifndef DEO_PERFORMANCE_OPTIMIZER_H
#define DEO_PERFORMANCE_OPTIMIZER_H

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <functional>

namespace deo::core {

/**
 * @brief Performance metrics for monitoring system performance
 */
struct PerformanceMetrics {
    std::atomic<uint64_t> total_operations{0};
    std::atomic<uint64_t> successful_operations{0};
    std::atomic<uint64_t> failed_operations{0};
    std::atomic<double> average_latency_ms{0.0};
    std::atomic<double> peak_latency_ms{0.0};
    std::atomic<double> throughput_ops_per_sec{0.0};
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_update;
    
    PerformanceMetrics() : start_time(std::chrono::system_clock::now()),
                          last_update(std::chrono::system_clock::now()) {}
};

/**
 * @brief Cache for frequently accessed data
 */
template<typename Key, typename Value>
class PerformanceCache {
public:
    PerformanceCache(size_t max_size = 1000) : max_size_(max_size) {}
    
    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            value = it->second;
            return true;
        }
        return false;
    }
    
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cache_.size() >= max_size_) {
            // Simple LRU eviction - remove oldest entry
            cache_.erase(cache_.begin());
        }
        cache_[key] = value;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }
    
private:
    std::unordered_map<Key, Value> cache_;
    mutable std::mutex mutex_;
    size_t max_size_;
};

/**
 * @brief Thread pool for parallel processing
 */
class ThreadPool {
public:
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    void shutdown();
    size_t size() const { return workers_.size(); }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};

/**
 * @brief Performance optimizer for blockchain operations
 */
class PerformanceOptimizer {
public:
    PerformanceOptimizer();
    ~PerformanceOptimizer();
    
    // Performance monitoring
    void startMonitoring();
    void stopMonitoring();
    PerformanceMetrics getMetrics() const;
    void recordOperation(bool success, double latency_ms);
    
    // Caching
    template<typename Key, typename Value>
    PerformanceCache<Key, Value>& getCache(const std::string& name);
    
    // Thread pool
    ThreadPool& getThreadPool();
    
    // Optimization settings
    void setCacheSize(const std::string& cache_name, size_t size);
    void setThreadPoolSize(size_t num_threads);
    void enableOptimizations(bool enable);
    
    // Memory management
    void optimizeMemoryUsage();
    size_t getMemoryUsage() const;
    
    // Batch processing
    template<typename T>
    void processBatch(const std::vector<T>& items, 
                     std::function<void(const T&)> processor,
                     size_t batch_size = 100);
    
private:
    mutable std::mutex metrics_mutex_;
    PerformanceMetrics metrics_;
    std::unordered_map<std::string, std::unique_ptr<PerformanceCache<std::string, std::string>>> caches_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::atomic<bool> monitoring_enabled_;
    std::atomic<bool> optimizations_enabled_;
    
    void updateMetrics();
    void cleanupExpiredCacheEntries();
};

// Template implementations
template<typename Key, typename Value>
PerformanceCache<Key, Value>& PerformanceOptimizer::getCache(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    if (caches_.find(name) == caches_.end()) {
        caches_[name] = std::make_unique<PerformanceCache<std::string, std::string>>();
    }
    return *static_cast<PerformanceCache<Key, Value>*>(caches_[name].get());
}

template<typename T>
void PerformanceOptimizer::processBatch(const std::vector<T>& items, 
                                       std::function<void(const T&)> processor,
                                       size_t batch_size) {
    if (!optimizations_enabled_) {
        // Process sequentially if optimizations disabled
        for (const auto& item : items) {
            processor(item);
        }
        return;
    }
    
    // Process in batches
    for (size_t i = 0; i < items.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, items.size());
        std::vector<T> batch(items.begin() + i, items.begin() + end);
        
        // Process batch in parallel
        std::vector<std::future<void>> futures;
        for (const auto& item : batch) {
            futures.push_back(thread_pool_->enqueue(processor, item));
        }
        
        // Wait for all tasks in batch to complete
        for (auto& future : futures) {
            future.wait();
        }
    }
}

template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks_.emplace([task](){ (*task)(); });
    }
    
    condition_.notify_one();
    return res;
}

} // namespace deo::core

#endif // DEO_PERFORMANCE_OPTIMIZER_H
