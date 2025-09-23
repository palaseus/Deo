/**
 * @file fast_sync.h
 * @brief Fast sync with headers-first download for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cstdint>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>

#include "core/block.h"
#include "network/peer_manager.h"
#include "storage/leveldb_storage.h"

namespace deo {
namespace sync {

/**
 * @brief Sync mode enumeration
 */
enum class SyncMode {
    FULL_SYNC,      ///< Download all blocks from genesis
    FAST_SYNC,      ///< Headers-first sync with state verification
    LIGHT_SYNC,     ///< Headers-only sync for light clients
    CUSTOM          ///< Custom sync strategy
};

/**
 * @brief Sync status enumeration
 */
enum class SyncStatus {
    IDLE,           ///< Not syncing
    CONNECTING,     ///< Connecting to peers
    DOWNLOADING_HEADERS, ///< Downloading block headers
    DOWNLOADING_BLOCKS,  ///< Downloading full blocks
    VERIFYING_STATE,     ///< Verifying state transitions
    COMPLETED,      ///< Sync completed successfully
    FAILED,         ///< Sync failed
    PAUSED          ///< Sync paused
};

/**
 * @brief Sync configuration
 */
struct SyncConfig {
    SyncMode mode = SyncMode::FAST_SYNC;
    
    // Connection settings
    uint32_t max_peers = 8;              ///< Maximum number of peers to sync from
    uint32_t min_peers = 3;              ///< Minimum number of peers required
    uint32_t connection_timeout_ms = 30000; ///< Connection timeout in milliseconds
    
    // Download settings
    uint32_t max_concurrent_downloads = 4; ///< Maximum concurrent downloads per peer
    uint32_t batch_size = 100;            ///< Number of headers/blocks per batch
    uint32_t max_headers_in_flight = 1000; ///< Maximum headers in flight
    uint32_t max_blocks_in_flight = 100;   ///< Maximum blocks in flight
    
    // Verification settings
    bool verify_headers = true;           ///< Verify header chain integrity
    bool verify_blocks = true;            ///< Verify block contents
    bool verify_state = true;             ///< Verify state transitions
    uint32_t verification_workers = 2;    ///< Number of verification worker threads
    
    // Timeout settings
    uint32_t header_timeout_ms = 10000;   ///< Header download timeout
    uint32_t block_timeout_ms = 30000;    ///< Block download timeout
    uint32_t state_timeout_ms = 60000;    ///< State verification timeout
    
    // Retry settings
    uint32_t max_retries = 3;             ///< Maximum retry attempts
    uint32_t retry_delay_ms = 1000;       ///< Delay between retries
    
    // Progress reporting
    std::function<void(uint64_t, uint64_t, SyncStatus)> progress_callback;
    std::function<void(const std::string&)> error_callback;
    std::function<void(const std::string&)> log_callback;
};

/**
 * @brief Sync statistics
 */
struct SyncStatistics {
    uint64_t headers_downloaded = 0;
    uint64_t blocks_downloaded = 0;
    uint64_t bytes_downloaded = 0;
    uint64_t headers_verified = 0;
    uint64_t blocks_verified = 0;
    uint64_t state_verified = 0;
    
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_update;
    
    uint32_t active_peers = 0;
    uint32_t failed_peers = 0;
    uint32_t retry_count = 0;
    
    double download_rate_mbps = 0.0;
    double verification_rate_hps = 0.0; // headers per second
    double estimated_completion_time_seconds = 0.0;
};

/**
 * @brief Fast sync manager
 * 
 * This class implements fast sync with headers-first download for efficient
 * blockchain synchronization. It downloads block headers first, verifies the
 * chain, then selectively downloads full blocks and verifies state.
 */
class FastSyncManager {
public:
    /**
     * @brief Constructor
     * @param peer_manager Reference to peer manager
     * @param block_storage Reference to block storage
     * @param state_storage Reference to state storage
     */
    FastSyncManager(std::shared_ptr<network::PeerManager> peer_manager,
                   std::shared_ptr<storage::LevelDBBlockStorage> block_storage,
                   std::shared_ptr<storage::LevelDBStateStorage> state_storage);
    
    /**
     * @brief Destructor
     */
    ~FastSyncManager();
    
    // Disable copy and move semantics
    FastSyncManager(const FastSyncManager&) = delete;
    FastSyncManager& operator=(const FastSyncManager&) = delete;
    FastSyncManager(FastSyncManager&&) = delete;
    FastSyncManager& operator=(FastSyncManager&&) = delete;

    /**
     * @brief Initialize the sync manager
     * @param config Sync configuration
     * @return True if initialization was successful
     */
    bool initialize(const SyncConfig& config);
    
    /**
     * @brief Shutdown the sync manager
     */
    void shutdown();

    /**
     * @brief Start synchronization
     * @return True if sync was started successfully
     */
    bool startSync();
    
    /**
     * @brief Stop synchronization
     */
    void stopSync();
    
    /**
     * @brief Pause synchronization
     */
    void pauseSync();
    
    /**
     * @brief Resume synchronization
     */
    void resumeSync();

    /**
     * @brief Get current sync status
     * @return Current sync status
     */
    SyncStatus getSyncStatus() const;
    
    /**
     * @brief Get sync progress (0.0 to 1.0)
     * @return Sync progress
     */
    double getSyncProgress() const;
    
    /**
     * @brief Get sync statistics
     * @return Sync statistics
     */
    SyncStatistics getSyncStatistics() const;
    
    /**
     * @brief Check if sync is active
     * @return True if sync is running
     */
    bool isSyncActive() const;
    
    /**
     * @brief Check if sync is completed
     * @return True if sync is completed
     */
    bool isSyncCompleted() const;

    /**
     * @brief Update sync configuration
     * @param config New sync configuration
     * @return True if configuration was updated successfully
     */
    bool updateConfig(const SyncConfig& config);
    
    /**
     * @brief Get current sync configuration
     * @return Current sync configuration
     */
    const SyncConfig& getConfig() const;

    /**
     * @brief Get sync target height
     * @return Target height to sync to
     */
    uint64_t getTargetHeight() const;
    
    /**
     * @brief Get current sync height
     * @return Current sync height
     */
    uint64_t getCurrentHeight() const;
    
    /**
     * @brief Get estimated time to completion
     * @return Estimated time in seconds
     */
    double getEstimatedTimeToCompletion() const;

    /**
     * @brief Force sync from a specific height
     * @param from_height Height to start sync from
     * @return True if sync was restarted successfully
     */
    bool forceSyncFromHeight(uint64_t from_height);
    
    /**
     * @brief Reset sync state
     */
    void resetSyncState();

private:
    std::shared_ptr<network::PeerManager> peer_manager_;
    std::shared_ptr<storage::LevelDBBlockStorage> block_storage_;
    std::shared_ptr<storage::LevelDBStateStorage> state_storage_;
    
    SyncConfig config_;
    mutable std::mutex config_mutex_;
    
    // Sync state
    std::atomic<SyncStatus> sync_status_{SyncStatus::IDLE};
    std::atomic<bool> sync_active_{false};
    std::atomic<bool> sync_paused_{false};
    std::atomic<uint64_t> target_height_{0};
    std::atomic<uint64_t> current_height_{0};
    std::atomic<uint64_t> sync_start_height_{0};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    SyncStatistics stats_;
    
    // Download queues
    std::queue<uint64_t> header_queue_;
    std::queue<uint64_t> block_queue_;
    std::mutex queue_mutex_;
    
    // In-flight tracking
    std::unordered_set<uint64_t> headers_in_flight_;
    std::unordered_set<uint64_t> blocks_in_flight_;
    std::mutex in_flight_mutex_;
    
    // Peer tracking
    std::unordered_map<std::string, uint64_t> peer_heights_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> peer_last_seen_;
    std::mutex peer_mutex_;
    
    // Worker threads
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> workers_active_{false};
    std::condition_variable worker_cv_;
    std::mutex worker_mutex_;
    
    // Verification workers
    std::vector<std::thread> verification_threads_;
    std::atomic<bool> verification_active_{false};
    std::queue<std::shared_ptr<core::Block>> verification_queue_;
    std::mutex verification_mutex_;
    std::condition_variable verification_cv_;
    
    /**
     * @brief Main sync loop
     */
    void syncLoop();
    
    /**
     * @brief Connect to peers
     * @return True if enough peers were connected
     */
    bool connectToPeers();
    
    /**
     * @brief Discover sync target height
     * @return True if target height was discovered
     */
    bool discoverTargetHeight();
    
    /**
     * @brief Download headers phase
     * @return True if headers were downloaded successfully
     */
    bool downloadHeaders();
    
    /**
     * @brief Download blocks phase
     * @return True if blocks were downloaded successfully
     */
    bool downloadBlocks();
    
    /**
     * @brief Verify state phase
     * @return True if state was verified successfully
     */
    bool verifyState();
    
    /**
     * @brief Header download worker
     */
    void headerDownloadWorker();
    
    /**
     * @brief Block download worker
     */
    void blockDownloadWorker();
    
    /**
     * @brief Verification worker
     */
    void verificationWorker();
    
    /**
     * @brief Request headers from peers
     * @param start_height Starting height
     * @param count Number of headers to request
     * @return True if request was sent successfully
     */
    bool requestHeaders(uint64_t start_height, uint32_t count);
    
    /**
     * @brief Request blocks from peers
     * @param start_height Starting height
     * @param count Number of blocks to request
     * @return True if request was sent successfully
     */
    bool requestBlocks(uint64_t start_height, uint32_t count);
    
    /**
     * @brief Process received headers
     * @param headers Vector of received headers
     * @return True if headers were processed successfully
     */
    bool processHeaders(const std::vector<std::shared_ptr<core::Block>>& headers);
    
    /**
     * @brief Process received blocks
     * @param blocks Vector of received blocks
     * @return True if blocks were processed successfully
     */
    bool processBlocks(const std::vector<std::shared_ptr<core::Block>>& blocks);
    
    /**
     * @brief Verify header chain
     * @param headers Vector of headers to verify
     * @return True if verification passed
     */
    bool verifyHeaderChain(const std::vector<std::shared_ptr<core::Block>>& headers);
    
    /**
     * @brief Verify block contents
     * @param block Block to verify
     * @return True if verification passed
     */
    bool verifyBlock(const std::shared_ptr<core::Block>& block);
    
    /**
     * @brief Verify state transition
     * @param block Block to verify state for
     * @return True if verification passed
     */
    bool verifyStateTransition(const std::shared_ptr<core::Block>& block);
    
    /**
     * @brief Update sync progress
     */
    void updateProgress();
    
    /**
     * @brief Update statistics
     */
    void updateStatistics();
    
    /**
     * @brief Calculate download rate
     * @return Download rate in MB/s
     */
    double calculateDownloadRate() const;
    
    /**
     * @brief Calculate verification rate
     * @return Verification rate in headers/second
     */
    double calculateVerificationRate() const;
    
    /**
     * @brief Estimate time to completion
     * @return Estimated time in seconds
     */
    double estimateTimeToCompletion() const;
    
    /**
     * @brief Select best peers for sync
     * @return Vector of selected peer addresses
     */
    std::vector<std::string> selectBestPeers() const;
    
    /**
     * @brief Check if peer is available
     * @param peer_address Peer address to check
     * @return True if peer is available
     */
    bool isPeerAvailable(const std::string& peer_address) const;
    
    /**
     * @brief Update peer information
     * @param peer_address Peer address
     * @param height Peer's blockchain height
     */
    void updatePeerInfo(const std::string& peer_address, uint64_t height);
    
    /**
     * @brief Clean up completed downloads
     */
    void cleanupCompletedDownloads();
    
    /**
     * @brief Handle sync error
     * @param error_message Error message
     */
    void handleSyncError(const std::string& error_message);
    
    /**
     * @brief Log sync message
     * @param message Log message
     */
    void logMessage(const std::string& message);
};

/**
 * @brief Sync progress tracker
 * 
 * This class tracks sync progress and provides detailed statistics
 * for monitoring and debugging sync operations.
 */
class SyncProgressTracker {
public:
    /**
     * @brief Constructor
     */
    SyncProgressTracker();
    
    /**
     * @brief Destructor
     */
    ~SyncProgressTracker();

    /**
     * @brief Start tracking
     * @param target_height Target height to sync to
     */
    void startTracking(uint64_t target_height);
    
    /**
     * @brief Stop tracking
     */
    void stopTracking();
    
    /**
     * @brief Update progress
     * @param current_height Current sync height
     * @param status Current sync status
     */
    void updateProgress(uint64_t current_height, SyncStatus status);
    
    /**
     * @brief Get current progress (0.0 to 1.0)
     * @return Current progress
     */
    double getProgress() const;
    
    /**
     * @brief Get estimated time to completion
     * @return Estimated time in seconds
     */
    double getEstimatedTimeToCompletion() const;
    
    /**
     * @brief Get sync rate
     * @return Sync rate in blocks per second
     */
    double getSyncRate() const;
    
    /**
     * @brief Get detailed statistics
     * @return JSON string with detailed statistics
     */
    std::string getDetailedStatistics() const;

private:
    std::atomic<uint64_t> target_height_{0};
    std::atomic<uint64_t> current_height_{0};
    std::atomic<SyncStatus> current_status_{SyncStatus::IDLE};
    
    std::chrono::system_clock::time_point start_time_;
    std::chrono::system_clock::time_point last_update_time_;
    
    mutable std::mutex stats_mutex_;
    std::vector<std::pair<std::chrono::system_clock::time_point, uint64_t>> progress_history_;
    
    /**
     * @brief Calculate sync rate from history
     * @return Sync rate in blocks per second
     */
    double calculateSyncRate() const;
    
    /**
     * @brief Update progress history
     */
    void updateProgressHistory();
};

} // namespace sync
} // namespace deo
