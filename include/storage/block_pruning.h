/**
 * @file block_pruning.h
 * @brief Block pruning and archival system for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "core/block.h"
#include "storage/leveldb_storage.h"

namespace deo {
namespace storage {

/**
 * @brief Pruning mode enumeration
 */
enum class PruningMode {
    FULL_ARCHIVE,    ///< Keep all blocks and state (full node)
    PRUNED,          ///< Keep only recent blocks and current state
    HYBRID,          ///< Keep recent blocks + periodic snapshots
    CUSTOM           ///< Custom pruning strategy
};

/**
 * @brief Pruning configuration
 */
struct PruningConfig {
    PruningMode mode = PruningMode::FULL_ARCHIVE;
    
    // Pruning parameters
    uint64_t keep_blocks = 0;           ///< Number of recent blocks to keep (0 = all)
    uint64_t keep_state_blocks = 0;     ///< Number of blocks to keep state for (0 = all)
    uint64_t snapshot_interval = 0;     ///< Create snapshots every N blocks (0 = disabled)
    
    // Size limits
    uint64_t max_storage_size_mb = 0;   ///< Maximum storage size in MB (0 = unlimited)
    uint64_t max_block_count = 0;       ///< Maximum number of blocks to keep (0 = unlimited)
    
    // Time-based pruning
    std::chrono::hours max_age_hours{0}; ///< Maximum age of blocks to keep (0 = unlimited)
    
    // Custom pruning function
    std::function<bool(const std::shared_ptr<core::Block>&, uint64_t)> custom_prune_func;
    
    // Archive settings
    bool enable_archival = false;       ///< Enable archival to external storage
    std::string archive_path;           ///< Path for archived blocks
    uint64_t archive_after_blocks = 0;  ///< Archive blocks after N confirmations
};

/**
 * @brief Block pruning and archival manager
 * 
 * This class manages block pruning and archival operations to optimize
 * storage usage while maintaining blockchain integrity and accessibility.
 */
class BlockPruningManager {
public:
    /**
     * @brief Constructor
     * @param block_storage Reference to block storage
     * @param state_storage Reference to state storage
     */
    BlockPruningManager(std::shared_ptr<LevelDBBlockStorage> block_storage,
                       std::shared_ptr<LevelDBStateStorage> state_storage);
    
    /**
     * @brief Destructor
     */
    ~BlockPruningManager();
    
    // Disable copy and move semantics
    BlockPruningManager(const BlockPruningManager&) = delete;
    BlockPruningManager& operator=(const BlockPruningManager&) = delete;
    BlockPruningManager(BlockPruningManager&&) = delete;
    BlockPruningManager& operator=(BlockPruningManager&&) = delete;

    /**
     * @brief Initialize the pruning manager
     * @param config Pruning configuration
     * @return True if initialization was successful
     */
    bool initialize(const PruningConfig& config);
    
    /**
     * @brief Shutdown the pruning manager
     */
    void shutdown();

    /**
     * @brief Update pruning configuration
     * @param config New pruning configuration
     * @return True if configuration was updated successfully
     */
    bool updateConfig(const PruningConfig& config);
    
    /**
     * @brief Get current pruning configuration
     * @return Current pruning configuration
     */
    const PruningConfig& getConfig() const;

    /**
     * @brief Check if a block should be pruned
     * @param block Block to check
     * @param current_height Current blockchain height
     * @return True if block should be pruned
     */
    bool shouldPruneBlock(const std::shared_ptr<core::Block>& block, uint64_t current_height) const;
    
    /**
     * @brief Check if state for a block should be pruned
     * @param block_height Block height to check
     * @param current_height Current blockchain height
     * @return True if state should be pruned
     */
    bool shouldPruneState(uint64_t block_height, uint64_t current_height) const;

    /**
     * @brief Perform pruning operation
     * @param current_height Current blockchain height
     * @return Number of blocks pruned
     */
    uint64_t performPruning(uint64_t current_height);
    
    /**
     * @brief Perform state pruning operation
     * @param current_height Current blockchain height
     * @return Number of state entries pruned
     */
    uint64_t performStatePruning(uint64_t current_height);

    /**
     * @brief Create a snapshot of current state
     * @param block_height Block height for the snapshot
     * @return True if snapshot was created successfully
     */
    bool createSnapshot(uint64_t block_height);
    
    /**
     * @brief Restore state from a snapshot
     * @param block_height Block height to restore to
     * @return True if restoration was successful
     */
    bool restoreFromSnapshot(uint64_t block_height);
    
    /**
     * @brief List available snapshots
     * @return Vector of snapshot block heights
     */
    std::vector<uint64_t> listSnapshots() const;

    /**
     * @brief Archive old blocks to external storage
     * @param from_height Starting block height
     * @param to_height Ending block height
     * @return Number of blocks archived
     */
    uint64_t archiveBlocks(uint64_t from_height, uint64_t to_height);
    
    /**
     * @brief Restore archived blocks from external storage
     * @param from_height Starting block height
     * @param to_height Ending block height
     * @return Number of blocks restored
     */
    uint64_t restoreArchivedBlocks(uint64_t from_height, uint64_t to_height);
    
    /**
     * @brief List archived block ranges
     * @return Vector of archived block ranges
     */
    std::vector<std::pair<uint64_t, uint64_t>> listArchivedRanges() const;

    /**
     * @brief Get storage statistics
     * @return JSON string with storage statistics
     */
    std::string getStorageStatistics() const;
    
    /**
     * @brief Get pruning statistics
     * @return JSON string with pruning statistics
     */
    std::string getPruningStatistics() const;
    
    /**
     * @brief Estimate storage savings from pruning
     * @param config Pruning configuration to test
     * @return Estimated storage savings in bytes
     */
    uint64_t estimateStorageSavings(const PruningConfig& config) const;

    /**
     * @brief Start automatic pruning
     * @param interval_seconds Pruning interval in seconds
     * @return True if automatic pruning was started
     */
    bool startAutomaticPruning(uint64_t interval_seconds);
    
    /**
     * @brief Stop automatic pruning
     */
    void stopAutomaticPruning();
    
    /**
     * @brief Check if automatic pruning is running
     * @return True if automatic pruning is active
     */
    bool isAutomaticPruningActive() const;

private:
    std::shared_ptr<LevelDBBlockStorage> block_storage_;
    std::shared_ptr<LevelDBStateStorage> state_storage_;
    
    PruningConfig config_;
    mutable std::mutex config_mutex_;
    
    // Automatic pruning
    std::atomic<bool> auto_pruning_active_{false};
    std::thread auto_pruning_thread_;
    std::atomic<uint64_t> auto_pruning_interval_{0};
    std::condition_variable auto_pruning_cv_;
    std::mutex auto_pruning_mutex_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    uint64_t total_blocks_pruned_{0};
    uint64_t total_state_entries_pruned_{0};
    uint64_t total_blocks_archived_{0};
    std::chrono::system_clock::time_point last_pruning_time_;
    
    /**
     * @brief Automatic pruning loop
     */
    void automaticPruningLoop();
    
    /**
     * @brief Check if storage size limits are exceeded
     * @return True if limits are exceeded
     */
    bool isStorageLimitExceeded() const;
    
    /**
     * @brief Calculate storage size
     * @return Storage size in bytes
     */
    uint64_t calculateStorageSize() const;
    
    /**
     * @brief Get block age in hours
     * @param block Block to check
     * @return Age in hours
     */
    std::chrono::hours getBlockAge(const std::shared_ptr<core::Block>& block) const;
    
    /**
     * @brief Update pruning statistics
     * @param blocks_pruned Number of blocks pruned
     * @param state_entries_pruned Number of state entries pruned
     */
    void updateStatistics(uint64_t blocks_pruned, uint64_t state_entries_pruned);
    
    /**
     * @brief Create archive directory if it doesn't exist
     * @return True if directory was created successfully
     */
    bool createArchiveDirectory() const;
    
    /**
     * @brief Serialize block for archival
     * @param block Block to serialize
     * @return Serialized block data
     */
    std::string serializeBlockForArchive(const std::shared_ptr<core::Block>& block) const;
    
    /**
     * @brief Deserialize block from archive
     * @param data Serialized block data
     * @return Deserialized block
     */
    std::shared_ptr<core::Block> deserializeBlockFromArchive(const std::string& data) const;
};

/**
 * @brief State snapshot manager
 * 
 * This class manages state snapshots for efficient state restoration
 * and hybrid pruning modes.
 */
class StateSnapshotManager {
public:
    /**
     * @brief Constructor
     * @param state_storage Reference to state storage
     */
    explicit StateSnapshotManager(std::shared_ptr<LevelDBStateStorage> state_storage);
    
    /**
     * @brief Destructor
     */
    ~StateSnapshotManager();

    /**
     * @brief Initialize the snapshot manager
     * @param snapshot_directory Directory for snapshots
     * @return True if initialization was successful
     */
    bool initialize(const std::string& snapshot_directory);
    
    /**
     * @brief Shutdown the snapshot manager
     */
    void shutdown();

    /**
     * @brief Create a snapshot of current state
     * @param block_height Block height for the snapshot
     * @return True if snapshot was created successfully
     */
    bool createSnapshot(uint64_t block_height);
    
    /**
     * @brief Restore state from a snapshot
     * @param block_height Block height to restore to
     * @return True if restoration was successful
     */
    bool restoreFromSnapshot(uint64_t block_height);
    
    /**
     * @brief Delete a snapshot
     * @param block_height Block height of snapshot to delete
     * @return True if snapshot was deleted successfully
     */
    bool deleteSnapshot(uint64_t block_height);
    
    /**
     * @brief List available snapshots
     * @return Vector of snapshot block heights
     */
    std::vector<uint64_t> listSnapshots() const;
    
    /**
     * @brief Get snapshot information
     * @param block_height Block height of snapshot
     * @return JSON string with snapshot information
     */
    std::string getSnapshotInfo(uint64_t block_height) const;
    
    /**
     * @brief Clean up old snapshots
     * @param keep_count Number of recent snapshots to keep
     * @return Number of snapshots deleted
     */
    uint64_t cleanupOldSnapshots(uint64_t keep_count);

private:
    std::shared_ptr<LevelDBStateStorage> state_storage_;
    std::string snapshot_directory_;
    mutable std::mutex snapshot_mutex_;
    
    /**
     * @brief Get snapshot file path
     * @param block_height Block height
     * @return Snapshot file path
     */
    std::string getSnapshotPath(uint64_t block_height) const;
    
    /**
     * @brief Create snapshot directory if it doesn't exist
     * @return True if directory was created successfully
     */
    bool createSnapshotDirectory() const;
};

} // namespace storage
} // namespace deo
