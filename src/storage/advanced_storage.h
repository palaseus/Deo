/**
 * @file advanced_storage.h
 * @brief Advanced storage optimizations and features for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#ifndef DEO_ADVANCED_STORAGE_H
#define DEO_ADVANCED_STORAGE_H

#include "block_storage.h"
#include "state_storage.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>

namespace deo::storage {

/**
 * @brief Advanced block storage with compression and indexing
 */
class AdvancedBlockStorage : public BlockStorage {
public:
    AdvancedBlockStorage(const std::string& data_directory);
    ~AdvancedBlockStorage();
    
    // Enhanced block operations
    bool storeBlock(std::shared_ptr<core::Block> block) override;
    std::shared_ptr<core::Block> getBlock(const std::string& hash) override;
    std::shared_ptr<core::Block> getBlock(uint32_t height) override;
    
    // Compression operations
    bool enableCompression(bool enable);
    bool isCompressionEnabled() const;
    double getCompressionRatio() const;
    
    // Indexing operations
    bool buildIndex();
    bool rebuildIndex();
    std::vector<std::string> searchBlocks(const std::string& query) const;
    std::vector<std::string> getBlocksByTimestamp(uint64_t start_time, uint64_t end_time) const;
    std::vector<std::string> getBlocksByDifficulty(uint32_t min_difficulty, uint32_t max_difficulty) const;
    
    // Batch operations
    bool storeBlocks(const std::vector<std::shared_ptr<core::Block>>& blocks);
    std::vector<std::shared_ptr<core::Block>> getBlocks(uint32_t start_height, uint32_t end_height) override;
    
    // Statistics
    std::unordered_map<std::string, int> getStatistics() const override;
    std::unordered_map<std::string, double> getPerformanceMetrics() const;
    
private:
    bool compression_enabled_;
    double compression_ratio_;
    std::unordered_map<std::string, std::string> block_index_;
    std::unordered_map<uint32_t, std::string> height_index_;
    std::unordered_map<uint64_t, std::vector<std::string>> timestamp_index_;
    std::unordered_map<uint32_t, std::vector<std::string>> difficulty_index_;
    mutable std::mutex index_mutex_;
    
    std::string compressBlock(const std::string& block_data) const;
    std::string decompressBlock(const std::string& compressed_data) const;
    void updateIndexes(std::shared_ptr<core::Block> block);
    void removeFromIndexes(const std::string& block_hash);
};

/**
 * @brief Advanced state storage with caching and optimization
 */
class AdvancedStateStorage : public StateStorage {
public:
    AdvancedStateStorage(const std::string& data_directory);
    ~AdvancedStateStorage();
    
    // Enhanced state operations
    bool setAccountState(const std::string& address, const AccountState& state) override;
    std::shared_ptr<AccountState> getAccountState(const std::string& address) override;
    
    // Caching operations
    bool enableCaching(bool enable);
    bool isCachingEnabled() const;
    void clearCache();
    std::unordered_map<std::string, size_t> getCacheStatistics() const;
    
    // Batch operations
    bool setAccountStates(const std::unordered_map<std::string, AccountState>& states);
    std::unordered_map<std::string, AccountState> getAccountStates(const std::vector<std::string>& addresses) const;
    
    // State optimization
    void optimizeStateStorage();
    void defragmentStateStorage();
    size_t getStateStorageSize() const;
    
    // Statistics
    std::unordered_map<std::string, int> getStatistics() const override;
    std::unordered_map<std::string, double> getPerformanceMetrics() const;
    
private:
    bool caching_enabled_;
    std::unordered_map<std::string, AccountState> state_cache_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;
    mutable std::mutex cache_mutex_;
    
    void updateCache(const std::string& address, const AccountState& state);
    void removeFromCache(const std::string& address);
    bool isInCache(const std::string& address) const;
    void cleanupExpiredCache();
};

/**
 * @brief Distributed storage for blockchain data
 */
class DistributedStorage {
public:
    DistributedStorage();
    ~DistributedStorage();
    
    // Node management
    bool addStorageNode(const std::string& node_id, const std::string& address, uint16_t port);
    bool removeStorageNode(const std::string& node_id);
    std::vector<std::string> getStorageNodes() const;
    bool isStorageNodeAvailable(const std::string& node_id) const;
    
    // Data distribution
    bool distributeBlock(std::shared_ptr<core::Block> block);
    bool distributeState(const std::string& address, const AccountState& state);
    std::shared_ptr<core::Block> retrieveBlock(const std::string& hash);
    std::shared_ptr<AccountState> retrieveState(const std::string& address);
    
    // Replication
    bool replicateData(const std::string& data_id, const std::string& data);
    bool removeReplication(const std::string& data_id);
    std::vector<std::string> getReplicatedNodes(const std::string& data_id) const;
    
    // Load balancing
    void rebalanceStorage();
    std::unordered_map<std::string, double> getNodeLoads() const;
    std::string getOptimalNode(const std::string& data_id) const;
    
private:
    struct StorageNode {
        std::string node_id;
        std::string address;
        uint16_t port;
        std::atomic<bool> is_online{true};
        std::atomic<double> load{0.0};
        std::atomic<uint64_t> stored_items{0};
        std::chrono::system_clock::time_point last_activity;
    };
    
    std::unordered_map<std::string, StorageNode> storage_nodes_;
    std::unordered_map<std::string, std::vector<std::string>> data_replication_;
    std::mutex nodes_mutex_;
    std::atomic<bool> initialized_;
    
    std::string selectStorageNode(const std::string& data_id) const;
    void updateNodeLoad(const std::string& node_id, double load);
    bool isNodeHealthy(const std::string& node_id) const;
};

/**
 * @brief Storage manager for managing multiple storage systems
 */
class StorageManager {
public:
    StorageManager();
    ~StorageManager();
    
    // Storage management
    bool addStorageSystem(const std::string& name, std::shared_ptr<BlockStorage> storage);
    bool addStateStorageSystem(const std::string& name, std::shared_ptr<StateStorage> storage);
    bool removeStorageSystem(const std::string& name);
    
    // Storage operations
    bool storeBlock(std::shared_ptr<core::Block> block);
    std::shared_ptr<core::Block> getBlock(const std::string& hash);
    bool storeState(const std::string& address, const AccountState& state);
    std::shared_ptr<AccountState> getState(const std::string& address);
    
    // Storage optimization
    void optimizeAllStorage();
    void defragmentAllStorage();
    std::unordered_map<std::string, size_t> getStorageSizes() const;
    
    // Statistics
    std::unordered_map<std::string, int> getStatistics() const;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> getAllStatistics() const;
    
private:
    std::unordered_map<std::string, std::shared_ptr<BlockStorage>> block_storages_;
    std::unordered_map<std::string, std::shared_ptr<StateStorage>> state_storages_;
    std::mutex storage_mutex_;
    std::atomic<bool> initialized_;
    
    bool isStorageSystemAvailable(const std::string& name) const;
    std::shared_ptr<BlockStorage> getOptimalBlockStorage() const;
    std::shared_ptr<StateStorage> getOptimalStateStorage() const;
};

/**
 * @brief Storage optimizer for performance optimization
 */
class StorageOptimizer {
public:
    StorageOptimizer();
    ~StorageOptimizer();
    
    // Optimization operations
    void optimizeStoragePerformance();
    void analyzeStorageMetrics();
    std::vector<std::string> getOptimizationRecommendations() const;
    
    // Storage tuning
    void tuneStorageParameters(const std::string& storage_name, 
                             const std::unordered_map<std::string, double>& parameters);
    std::unordered_map<std::string, double> getOptimalParameters(const std::string& storage_name) const;
    
    // Performance monitoring
    void startPerformanceMonitoring();
    void stopPerformanceMonitoring();
    std::unordered_map<std::string, double> getPerformanceMetrics() const;
    
    // Storage analysis
    void analyzeStorageUsage();
    void identifyStorageBottlenecks();
    std::vector<std::string> getStorageRecommendations() const;
    
private:
    std::unordered_map<std::string, std::unordered_map<std::string, double>> storage_parameters_;
    std::unordered_map<std::string, double> performance_metrics_;
    std::atomic<bool> monitoring_active_;
    std::vector<std::string> optimization_recommendations_;
    
    void analyzeStoragePerformance();
    void generateOptimizationRecommendations();
    void updatePerformanceMetrics();
    void identifyBottlenecks();
};

/**
 * @brief Storage backup and recovery system
 */
class StorageBackupRecovery {
public:
    StorageBackupRecovery();
    ~StorageBackupRecovery();
    
    // Backup operations
    bool createBackup(const std::string& backup_name, const std::string& storage_path);
    bool restoreBackup(const std::string& backup_name, const std::string& storage_path);
    bool deleteBackup(const std::string& backup_name);
    std::vector<std::string> getAvailableBackups() const;
    
    // Incremental backup
    bool createIncrementalBackup(const std::string& backup_name, const std::string& base_backup);
    bool restoreIncrementalBackup(const std::string& backup_name, const std::string& base_backup);
    
    // Backup verification
    bool verifyBackup(const std::string& backup_name) const;
    std::unordered_map<std::string, bool> verifyAllBackups() const;
    
    // Backup scheduling
    void scheduleBackup(const std::string& backup_name, const std::string& schedule);
    void cancelScheduledBackup(const std::string& backup_name);
    std::vector<std::string> getScheduledBackups() const;
    
private:
    std::unordered_map<std::string, std::string> backup_schedules_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> backup_timestamps_;
    std::mutex backup_mutex_;
    std::atomic<bool> initialized_;
    
    bool isValidBackupName(const std::string& backup_name) const;
    std::string generateBackupPath(const std::string& backup_name) const;
    void executeScheduledBackup(const std::string& backup_name);
};

} // namespace deo::storage

#endif // DEO_ADVANCED_STORAGE_H
