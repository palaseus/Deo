/**
 * @file fast_sync_manager.cpp
 * @brief Fast sync manager implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "sync/fast_sync.h"
#include "utils/logger.h"
#include "crypto/hash.h"
#include "consensus/consensus_engine.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <random>
#include <thread>
#include <chrono>

namespace deo {
namespace sync {

FastSyncManager::FastSyncManager(std::shared_ptr<network::PeerManager> peer_manager,
                                 std::shared_ptr<storage::LevelDBBlockStorage> block_storage,
                                 std::shared_ptr<storage::LevelDBStateStorage> state_storage)
    : peer_manager_(peer_manager), block_storage_(block_storage), state_storage_(state_storage) {
    
    stats_.start_time = std::chrono::system_clock::now();
    stats_.last_update = stats_.start_time;
}

FastSyncManager::~FastSyncManager() {
    shutdown();
}

bool FastSyncManager::initialize(const SyncConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_ = config;
    
    // Validate configuration
    if (config_.max_peers < config_.min_peers) {
        return false;
    }
    
    if (config_.batch_size == 0 || config_.max_concurrent_downloads == 0) {
        return false;
    }
    
    return true;
}

void FastSyncManager::shutdown() {
    stopSync();
    
    // Wait for all worker threads to finish
    workers_active_.store(false);
    verification_active_.store(false);
    
    worker_cv_.notify_all();
    verification_cv_.notify_all();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    for (auto& thread : verification_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
    verification_threads_.clear();
}

bool FastSyncManager::startSync() {
    if (sync_active_.load()) {
        return false; // Already syncing
    }
    
    if (!peer_manager_ || !block_storage_ || !state_storage_) {
        return false;
    }
    
    sync_active_.store(true);
    sync_paused_.store(false);
    sync_status_.store(SyncStatus::CONNECTING);
    
    // Reset statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = SyncStatistics{};
        stats_.start_time = std::chrono::system_clock::now();
        stats_.last_update = stats_.start_time;
    }
    
    // Start worker threads
    workers_active_.store(true);
    verification_active_.store(true);
    
    // Start main sync thread
    worker_threads_.emplace_back(&FastSyncManager::syncLoop, this);
    
    // Start download workers
    for (uint32_t i = 0; i < config_.max_concurrent_downloads; ++i) {
        worker_threads_.emplace_back(&FastSyncManager::headerDownloadWorker, this);
        worker_threads_.emplace_back(&FastSyncManager::blockDownloadWorker, this);
    }
    
    // Start verification workers
    for (uint32_t i = 0; i < config_.verification_workers; ++i) {
        verification_threads_.emplace_back(&FastSyncManager::verificationWorker, this);
    }
    
    return true;
}

void FastSyncManager::stopSync() {
    if (!sync_active_.load()) {
        return;
    }
    
    sync_active_.store(false);
    sync_paused_.store(false);
    sync_status_.store(SyncStatus::IDLE);
    
    workers_active_.store(false);
    verification_active_.store(false);
    
    worker_cv_.notify_all();
    verification_cv_.notify_all();
}

void FastSyncManager::pauseSync() {
    if (sync_active_.load()) {
        sync_paused_.store(true);
        sync_status_.store(SyncStatus::PAUSED);
    }
}

void FastSyncManager::resumeSync() {
    if (sync_active_.load() && sync_paused_.load()) {
        sync_paused_.store(false);
        sync_status_.store(SyncStatus::DOWNLOADING_HEADERS);
        worker_cv_.notify_all();
    }
}

SyncStatus FastSyncManager::getSyncStatus() const {
    return sync_status_.load();
}

double FastSyncManager::getSyncProgress() const {
    uint64_t target = target_height_.load();
    uint64_t current = current_height_.load();
    
    if (target == 0) {
        return 0.0;
    }
    
    return static_cast<double>(current) / static_cast<double>(target);
}

SyncStatistics FastSyncManager::getSyncStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

bool FastSyncManager::isSyncActive() const {
    return sync_active_.load();
}

bool FastSyncManager::isSyncCompleted() const {
    return sync_status_.load() == SyncStatus::COMPLETED;
}

bool FastSyncManager::updateConfig(const SyncConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Validate configuration
    if (config.max_peers < config.min_peers) {
        return false;
    }
    
    if (config.batch_size == 0 || config.max_concurrent_downloads == 0) {
        return false;
    }
    
    config_ = config;
    return true;
}

const SyncConfig& FastSyncManager::getConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

uint64_t FastSyncManager::getTargetHeight() const {
    return target_height_.load();
}

uint64_t FastSyncManager::getCurrentHeight() const {
    return current_height_.load();
}

double FastSyncManager::getEstimatedTimeToCompletion() const {
    return estimateTimeToCompletion();
}

bool FastSyncManager::forceSyncFromHeight(uint64_t from_height) {
    if (sync_active_.load()) {
        return false; // Can't change sync while active
    }
    
    sync_start_height_.store(from_height);
    current_height_.store(from_height);
    
    return true;
}

void FastSyncManager::resetSyncState() {
    if (sync_active_.load()) {
        return; // Can't reset while active
    }
    
    target_height_.store(0);
    current_height_.store(0);
    sync_start_height_.store(0);
    
    // Clear queues
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!header_queue_.empty()) {
            header_queue_.pop();
        }
        while (!block_queue_.empty()) {
            block_queue_.pop();
        }
    }
    
    // Clear in-flight tracking
    {
        std::lock_guard<std::mutex> lock(in_flight_mutex_);
        headers_in_flight_.clear();
        blocks_in_flight_.clear();
    }
    
    // Clear peer tracking
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        peer_heights_.clear();
        peer_last_seen_.clear();
    }
}

void FastSyncManager::syncLoop() {
    try {
        logMessage("Starting sync loop");
        
        // Phase 1: Connect to peers
        if (!connectToPeers()) {
            handleSyncError("Failed to connect to sufficient peers");
            return;
        }
        
        // Phase 2: Discover target height
        if (!discoverTargetHeight()) {
            handleSyncError("Failed to discover target height");
            return;
        }
        
        // Phase 3: Download headers
        sync_status_.store(SyncStatus::DOWNLOADING_HEADERS);
        if (!downloadHeaders()) {
            handleSyncError("Failed to download headers");
            return;
        }
        
        // Phase 4: Download blocks (if not headers-only mode)
        if (config_.mode != SyncMode::LIGHT_SYNC) {
            sync_status_.store(SyncStatus::DOWNLOADING_BLOCKS);
            if (!downloadBlocks()) {
                handleSyncError("Failed to download blocks");
                return;
            }
        }
        
        // Phase 5: Verify state (if enabled)
        if (config_.verify_state && config_.mode != SyncMode::LIGHT_SYNC) {
            sync_status_.store(SyncStatus::VERIFYING_STATE);
            if (!verifyState()) {
                handleSyncError("Failed to verify state");
                return;
            }
        }
        
        // Sync completed successfully
        sync_status_.store(SyncStatus::COMPLETED);
        logMessage("Sync completed successfully");
        
    } catch (const std::exception& e) {
        handleSyncError("Sync loop exception: " + std::string(e.what()));
    }
}

bool FastSyncManager::connectToPeers() {
    logMessage("Connecting to peers...");
    
    // Get available peers from peer manager
    auto peers = peer_manager_->getConnectedPeers();
    
    if (peers.size() < config_.min_peers) {
        logMessage("Insufficient peers available: " + std::to_string(peers.size()) + 
                  " < " + std::to_string(config_.min_peers));
        return false;
    }
    
    // Select best peers
    auto selected_peers = selectBestPeers();
    
    if (selected_peers.size() < config_.min_peers) {
        logMessage("Insufficient selected peers: " + std::to_string(selected_peers.size()) + 
                  " < " + std::to_string(config_.min_peers));
        return false;
    }
    
    logMessage("Connected to " + std::to_string(selected_peers.size()) + " peers");
    return true;
}

bool FastSyncManager::discoverTargetHeight() {
    logMessage("Discovering target height...");
    
    // Get heights from all connected peers
    auto peers = peer_manager_->getConnectedPeers();
    std::vector<uint64_t> peer_heights;
    
    for (const auto& peer_address : peers) {
        // In a real implementation, this would request the peer's height
        // For now, we'll use a placeholder
        uint64_t height = 1000; // Placeholder
        peer_heights.push_back(height);
        updatePeerInfo(peer_address, height);
    }
    
    if (peer_heights.empty()) {
        return false;
    }
    
    // Use the median height as target (to avoid outliers)
    std::sort(peer_heights.begin(), peer_heights.end());
    uint64_t median_height = peer_heights[peer_heights.size() / 2];
    
    target_height_.store(median_height);
    logMessage("Target height set to: " + std::to_string(median_height));
    
    return true;
}

bool FastSyncManager::downloadHeaders() {
    logMessage("Starting header download...");
    
    uint64_t start_height = current_height_.load() + 1;
    uint64_t target = target_height_.load();
    
    if (start_height > target) {
        return true; // Already up to date
    }
    
    // Populate header queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        for (uint64_t height = start_height; height <= target; ++height) {
            header_queue_.push(height);
        }
    }
    
    // Wait for headers to be downloaded
    while (sync_active_.load() && !sync_paused_.load()) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (header_queue_.empty()) {
            break;
        }
        
        // Check if we have headers in flight
        std::lock_guard<std::mutex> in_flight_lock(in_flight_mutex_);
        if (headers_in_flight_.size() < config_.max_headers_in_flight) {
            // Request more headers
            uint32_t batch_size = std::min(config_.batch_size, 
                                         static_cast<uint32_t>(header_queue_.size()));
            
            if (batch_size > 0) {
                uint64_t batch_start = header_queue_.front();
                if (requestHeaders(batch_start, batch_size)) {
                    // Move headers from queue to in-flight
                    for (uint32_t i = 0; i < batch_size; ++i) {
                        if (!header_queue_.empty()) {
                            uint64_t height = header_queue_.front();
                            header_queue_.pop();
                            headers_in_flight_.insert(height);
                        }
                    }
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Wait for all headers to be processed
    while (sync_active_.load() && !sync_paused_.load()) {
        std::lock_guard<std::mutex> lock(in_flight_mutex_);
        if (headers_in_flight_.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    logMessage("Header download completed");
    return true;
}

bool FastSyncManager::downloadBlocks() {
    logMessage("Starting block download...");
    
    uint64_t start_height = current_height_.load() + 1;
    uint64_t target = target_height_.load();
    
    if (start_height > target) {
        return true; // Already up to date
    }
    
    // Populate block queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        for (uint64_t height = start_height; height <= target; ++height) {
            block_queue_.push(height);
        }
    }
    
    // Wait for blocks to be downloaded
    while (sync_active_.load() && !sync_paused_.load()) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (block_queue_.empty()) {
            break;
        }
        
        // Check if we have blocks in flight
        std::lock_guard<std::mutex> in_flight_lock(in_flight_mutex_);
        if (blocks_in_flight_.size() < config_.max_blocks_in_flight) {
            // Request more blocks
            uint32_t batch_size = std::min(config_.batch_size, 
                                         static_cast<uint32_t>(block_queue_.size()));
            
            if (batch_size > 0) {
                uint64_t batch_start = block_queue_.front();
                if (requestBlocks(batch_start, batch_size)) {
                    // Move blocks from queue to in-flight
                    for (uint32_t i = 0; i < batch_size; ++i) {
                        if (!block_queue_.empty()) {
                            uint64_t height = block_queue_.front();
                            block_queue_.pop();
                            blocks_in_flight_.insert(height);
                        }
                    }
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Wait for all blocks to be processed
    while (sync_active_.load() && !sync_paused_.load()) {
        std::lock_guard<std::mutex> lock(in_flight_mutex_);
        if (blocks_in_flight_.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    logMessage("Block download completed");
    return true;
}

bool FastSyncManager::verifyState() {
    logMessage("Starting state verification...");
    
    // In a real implementation, this would verify state transitions
    // For now, we'll just simulate the process
    
    uint64_t start_height = sync_start_height_.load();
    uint64_t target = target_height_.load();
    
    for (uint64_t height = start_height; height <= target; ++height) {
        if (!sync_active_.load() || sync_paused_.load()) {
            break;
        }
        
        // Simulate state verification
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Update progress
        current_height_.store(height);
        updateProgress();
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.state_verified++;
        }
    }
    
    logMessage("State verification completed");
    return true;
}

void FastSyncManager::headerDownloadWorker() {
    while (workers_active_.load()) {
        std::unique_lock<std::mutex> lock(worker_mutex_);
        worker_cv_.wait(lock, [this] { 
            return !workers_active_.load() || !sync_paused_.load(); 
        });
        
        if (!workers_active_.load()) {
            break;
        }
        
        // Simulate header download work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Update statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.headers_downloaded++;
        }
    }
}

void FastSyncManager::blockDownloadWorker() {
    while (workers_active_.load()) {
        std::unique_lock<std::mutex> lock(worker_mutex_);
        worker_cv_.wait(lock, [this] { 
            return !workers_active_.load() || !sync_paused_.load(); 
        });
        
        if (!workers_active_.load()) {
            break;
        }
        
        // Simulate block download work
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Update statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.blocks_downloaded++;
        }
    }
}

void FastSyncManager::verificationWorker() {
    while (verification_active_.load()) {
        std::unique_lock<std::mutex> lock(verification_mutex_);
        verification_cv_.wait(lock, [this] { 
            return !verification_active_.load() || !verification_queue_.empty(); 
        });
        
        if (!verification_active_.load()) {
            break;
        }
        
        if (!verification_queue_.empty()) {
            auto block = verification_queue_.front();
            verification_queue_.pop();
            lock.unlock();
            
            // Simulate verification work
            if (verifyBlock(block)) {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.blocks_verified++;
            }
        }
    }
}

bool FastSyncManager::requestHeaders(uint64_t start_height, uint32_t count) {
    // In a real implementation, this would send network requests
    // For now, we'll just simulate the request
    logMessage("Requesting headers from height " + std::to_string(start_height) + 
              " count " + std::to_string(count));
    
    return true;
}

bool FastSyncManager::requestBlocks(uint64_t start_height, uint32_t count) {
    // In a real implementation, this would send network requests
    // For now, we'll just simulate the request
    logMessage("Requesting blocks from height " + std::to_string(start_height) + 
              " count " + std::to_string(count));
    
    return true;
}

bool FastSyncManager::processHeaders(const std::vector<std::shared_ptr<core::Block>>& headers) {
    if (headers.empty()) {
        return true;
    }
    
    // Verify header chain
    if (config_.verify_headers && !verifyHeaderChain(headers)) {
        return false;
    }
    
    // Store headers (in a real implementation, this would store to LevelDB)
    for (const auto& header : headers) {
        (void)header; // Suppress unused variable warning
        // Simulate header storage
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.headers_verified += headers.size();
    }
    
    return true;
}

bool FastSyncManager::processBlocks(const std::vector<std::shared_ptr<core::Block>>& blocks) {
    if (blocks.empty()) {
        return true;
    }
    
    // Verify blocks
    if (config_.verify_blocks) {
        for (const auto& block : blocks) {
            if (!verifyBlock(block)) {
                return false;
            }
        }
    }
    
    // Store blocks
    for (const auto& block : blocks) {
        if (block_storage_ && !block_storage_->storeBlock(block)) {
            return false;
        }
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.blocks_verified += blocks.size();
    }
    
    return true;
}

bool FastSyncManager::verifyHeaderChain(const std::vector<std::shared_ptr<core::Block>>& headers) {
    if (headers.empty()) {
        return true;
    }
    
    // Verify chain continuity
    for (size_t i = 1; i < headers.size(); ++i) {
        if (headers[i]->getHeader().previous_hash != headers[i-1]->calculateHash()) {
            return false;
        }
        
        if (headers[i]->getHeader().height != headers[i-1]->getHeader().height + 1) {
            return false;
        }
    }
    
    return true;
}

bool FastSyncManager::verifyBlock(const std::shared_ptr<core::Block>& block) {
    if (!block) {
        return false;
    }
    
    // Verify block hash
    std::string calculated_hash = block->calculateHash();
    // In a real implementation, this would compare with expected hash
    
    // Verify transactions
    for (const auto& tx : block->getTransactions()) {
        if (!tx->validate()) {
            return false;
        }
    }
    
    return true;
}

bool FastSyncManager::verifyStateTransition(const std::shared_ptr<core::Block>& block) {
    if (!block || !state_storage_) {
        return false;
    }
    
    // In a real implementation, this would verify state transitions
    // For now, we'll just simulate the verification
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    return true;
}

void FastSyncManager::updateProgress() {
    if (config_.progress_callback) {
        config_.progress_callback(current_height_.load(), target_height_.load(), 
                                 sync_status_.load());
    }
    
    updateStatistics();
}

void FastSyncManager::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto now = std::chrono::system_clock::now();
    stats_.last_update = now;
    
    // Calculate rates
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - stats_.start_time);
    if (duration.count() > 0) {
        stats_.download_rate_mbps = static_cast<double>(stats_.bytes_downloaded) / 
                                   (duration.count() * 1024.0 * 1024.0);
        stats_.verification_rate_hps = static_cast<double>(stats_.headers_verified) / 
                                      duration.count();
    }
    
    // Estimate completion time
    stats_.estimated_completion_time_seconds = estimateTimeToCompletion();
}

double FastSyncManager::calculateDownloadRate() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.download_rate_mbps;
}

double FastSyncManager::calculateVerificationRate() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.verification_rate_hps;
}

double FastSyncManager::estimateTimeToCompletion() const {
    uint64_t remaining = target_height_.load() - current_height_.load();
    if (remaining == 0) {
        return 0.0;
    }
    
    double rate = calculateVerificationRate();
    if (rate <= 0.0) {
        return -1.0; // Unknown
    }
    
    return static_cast<double>(remaining) / rate;
}

std::vector<std::string> FastSyncManager::selectBestPeers() const {
    auto peers = peer_manager_->getConnectedPeers();
    std::vector<std::string> selected_peers;
    
    // Sort peers by height (highest first)
    std::sort(peers.begin(), peers.end(), [this](const std::string& a, const std::string& b) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(peer_mutex_));
        auto it_a = peer_heights_.find(a);
        auto it_b = peer_heights_.find(b);
        
        uint64_t height_a = (it_a != peer_heights_.end()) ? it_a->second : 0;
        uint64_t height_b = (it_b != peer_heights_.end()) ? it_b->second : 0;
        
        return height_a > height_b;
    });
    
    // Select top peers up to max_peers
    uint32_t count = std::min(static_cast<uint32_t>(peers.size()), config_.max_peers);
    for (uint32_t i = 0; i < count; ++i) {
        selected_peers.push_back(peers[i]);
    }
    
    return selected_peers;
}

bool FastSyncManager::isPeerAvailable(const std::string& peer_address) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(peer_mutex_));
    auto it = peer_last_seen_.find(peer_address);
    
    if (it == peer_last_seen_.end()) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_since_seen = std::chrono::duration_cast<std::chrono::minutes>(now - it->second);
    
    return time_since_seen.count() < 5; // Consider peer available if seen within 5 minutes
}

void FastSyncManager::updatePeerInfo(const std::string& peer_address, uint64_t height) {
    std::lock_guard<std::mutex> lock(peer_mutex_);
    
    peer_heights_[peer_address] = height;
    peer_last_seen_[peer_address] = std::chrono::system_clock::now();
}

void FastSyncManager::cleanupCompletedDownloads() {
    std::lock_guard<std::mutex> lock(in_flight_mutex_);
    
    // In a real implementation, this would clean up completed downloads
    // For now, we'll just clear the in-flight sets periodically
    if (headers_in_flight_.size() > config_.max_headers_in_flight * 2) {
        headers_in_flight_.clear();
    }
    
    if (blocks_in_flight_.size() > config_.max_blocks_in_flight * 2) {
        blocks_in_flight_.clear();
    }
}

void FastSyncManager::handleSyncError(const std::string& error_message) {
    logMessage("Sync error: " + error_message);
    
    sync_status_.store(SyncStatus::FAILED);
    sync_active_.store(false);
    
    if (config_.error_callback) {
        config_.error_callback(error_message);
    }
}

void FastSyncManager::logMessage(const std::string& message) {
    if (config_.log_callback) {
        config_.log_callback(message);
    }
}

// SyncProgressTracker implementation
SyncProgressTracker::SyncProgressTracker() {
    start_time_ = std::chrono::system_clock::now();
    last_update_time_ = start_time_;
}

SyncProgressTracker::~SyncProgressTracker() {
    // Nothing to do
}

void SyncProgressTracker::startTracking(uint64_t target_height) {
    target_height_.store(target_height);
    current_height_.store(0);
    current_status_.store(SyncStatus::IDLE);
    
    start_time_ = std::chrono::system_clock::now();
    last_update_time_ = start_time_;
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    progress_history_.clear();
}

void SyncProgressTracker::stopTracking() {
    current_status_.store(SyncStatus::IDLE);
}

void SyncProgressTracker::updateProgress(uint64_t current_height, SyncStatus status) {
    current_height_.store(current_height);
    current_status_.store(status);
    
    last_update_time_ = std::chrono::system_clock::now();
    updateProgressHistory();
}

double SyncProgressTracker::getProgress() const {
    uint64_t target = target_height_.load();
    uint64_t current = current_height_.load();
    
    if (target == 0) {
        return 0.0;
    }
    
    return static_cast<double>(current) / static_cast<double>(target);
}

double SyncProgressTracker::getEstimatedTimeToCompletion() const {
    double rate = getSyncRate();
    if (rate <= 0.0) {
        return -1.0; // Unknown
    }
    
    uint64_t remaining = target_height_.load() - current_height_.load();
    return static_cast<double>(remaining) / rate;
}

double SyncProgressTracker::getSyncRate() const {
    return calculateSyncRate();
}

std::string SyncProgressTracker::getDetailedStatistics() const {
    nlohmann::json stats;
    
    stats["target_height"] = target_height_.load();
    stats["current_height"] = current_height_.load();
    stats["progress"] = getProgress();
    stats["status"] = static_cast<int>(current_status_.load());
    stats["sync_rate"] = getSyncRate();
    stats["estimated_completion_time"] = getEstimatedTimeToCompletion();
    
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    stats["elapsed_time_seconds"] = duration.count();
    
    return stats.dump(2);
}

double SyncProgressTracker::calculateSyncRate() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (progress_history_.size() < 2) {
        return 0.0;
    }
    
    // Calculate rate from recent history
    auto recent_start = progress_history_.end() - std::min(progress_history_.size(), size_t(10));
    auto recent_end = progress_history_.end();
    
    if (recent_start == recent_end) {
        return 0.0;
    }
    
    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
        (recent_end - 1)->first - recent_start->first).count();
    
    if (time_diff <= 0) {
        return 0.0;
    }
    
    uint64_t height_diff = (recent_end - 1)->second - recent_start->second;
    return static_cast<double>(height_diff) / static_cast<double>(time_diff);
}

void SyncProgressTracker::updateProgressHistory() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto now = std::chrono::system_clock::now();
    progress_history_.emplace_back(now, current_height_.load());
    
    // Keep only recent history (last 100 points)
    if (progress_history_.size() > 100) {
        progress_history_.erase(progress_history_.begin());
    }
}

} // namespace sync
} // namespace deo
