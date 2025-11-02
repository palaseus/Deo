/**
 * @file block_pruning_manager.cpp
 * @brief Block pruning and archival manager implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "storage/block_pruning.h"
#include "utils/logger.h"
#include "crypto/hash.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <condition_variable>
#include <set>

namespace deo {
namespace storage {

BlockPruningManager::BlockPruningManager(std::shared_ptr<LevelDBBlockStorage> block_storage,
                                         std::shared_ptr<LevelDBStateStorage> state_storage)
    : block_storage_(block_storage), state_storage_(state_storage) {
}

BlockPruningManager::~BlockPruningManager() {
    shutdown();
}

bool BlockPruningManager::initialize(const PruningConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_ = config;
    
    // Create archive directory if archival is enabled
    if (config_.enable_archival && !config_.archive_path.empty()) {
        if (!createArchiveDirectory()) {
            return false;
        }
    }
    
    return true;
}

void BlockPruningManager::shutdown() {
    stopAutomaticPruning();
}

bool BlockPruningManager::updateConfig(const PruningConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Validate configuration
    if (config.mode == PruningMode::CUSTOM && !config.custom_prune_func) {
        return false;
    }
    
    config_ = config;
    
    // Create archive directory if archival is enabled
    if (config_.enable_archival && !config_.archive_path.empty()) {
        if (!createArchiveDirectory()) {
            return false;
        }
    }
    
    return true;
}

const PruningConfig& BlockPruningManager::getConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

bool BlockPruningManager::shouldPruneBlock(const std::shared_ptr<core::Block>& block, uint64_t current_height) const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!block) {
        return false;
    }
    
    uint64_t block_height = block->getHeader().height;
    
    // Never prune genesis block
    if (block_height == 0) {
        return false;
    }
    
    switch (config_.mode) {
        case PruningMode::FULL_ARCHIVE:
            return false;
            
        case PruningMode::PRUNED:
            if (config_.keep_blocks > 0) {
                return (current_height - block_height) > config_.keep_blocks;
            }
            break;
            
        case PruningMode::HYBRID:
            // Keep recent blocks + snapshots
            if (config_.keep_blocks > 0) {
                uint64_t age = current_height - block_height;
                if (age > config_.keep_blocks) {
                    // Check if this block is at a snapshot interval
                    if (config_.snapshot_interval > 0) {
                        return (block_height % config_.snapshot_interval) != 0;
                    }
                    return true;
                }
            }
            break;
            
        case PruningMode::CUSTOM:
            if (config_.custom_prune_func) {
                return config_.custom_prune_func(block, current_height);
            }
            break;
    }
    
    // Check size limits
    if (config_.max_block_count > 0) {
        uint64_t total_blocks = current_height + 1;
        if (total_blocks > config_.max_block_count) {
            return (current_height - block_height) >= config_.max_block_count;
        }
    }
    
    // Check age limits
    if (config_.max_age_hours.count() > 0) {
        auto age = getBlockAge(block);
        if (age > config_.max_age_hours) {
            return true;
        }
    }
    
    return false;
}

bool BlockPruningManager::shouldPruneState(uint64_t block_height, uint64_t current_height) const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (config_.mode == PruningMode::FULL_ARCHIVE) {
        return false;
    }
    
    if (config_.keep_state_blocks > 0) {
        return (current_height - block_height) > config_.keep_state_blocks;
    }
    
    return false;
}

uint64_t BlockPruningManager::performPruning(uint64_t current_height) {
    if (!block_storage_) {
        return 0;
    }
    
    uint64_t blocks_pruned = 0;
    uint64_t state_entries_pruned = 0;
    
    try {
        // Get all blocks to check for pruning
        std::vector<std::string> block_hashes = block_storage_->getBlockHashesInRange(0, current_height);
        
        std::vector<std::string> blocks_to_prune;
        std::vector<uint64_t> state_to_prune;
        
        for (const auto& block_hash : block_hashes) {
            auto block = block_storage_->getBlock(block_hash);
            if (block && shouldPruneBlock(block, current_height)) {
                blocks_to_prune.push_back(block_hash);
                state_to_prune.push_back(block->getHeader().height);
            }
        }
        
        // Group blocks by height and find the lowest height to prune
        // For pruning, we typically delete from lowest height up to keep_blocks threshold
        if (!blocks_to_prune.empty()) {
            // Find the lowest height to prune
            uint64_t lowest_prune_height = UINT64_MAX;
            for (const auto& block_hash : blocks_to_prune) {
                auto block = block_storage_->getBlock(block_hash);
                if (block) {
                    uint64_t height = block->getHeader().height;
                    if (height < lowest_prune_height) {
                        lowest_prune_height = height;
                    }
                    
                    // Archive block if archival is enabled
                    if (config_.enable_archival) {
                        // Archive block (implementation would save to external storage)
                        DEO_LOG_DEBUG(STORAGE, "Would archive block at height " + std::to_string(height));
                    }
                }
            }
            
            // Delete blocks from the lowest prune height onwards
            // This efficiently deletes all blocks that should be pruned
            if (lowest_prune_height != UINT64_MAX && lowest_prune_height > 0) {
                // Calculate how many blocks we're about to delete
                uint64_t storage_current_height = block_storage_->getCurrentHeight();
                if (storage_current_height >= lowest_prune_height) {
                    blocks_pruned = storage_current_height - lowest_prune_height + 1;
                    
                    // Use the existing deletion method
                    if (block_storage_->deleteBlocksFromHeight(lowest_prune_height)) {
                        DEO_LOG_INFO(STORAGE, "Pruned " + std::to_string(blocks_pruned) + 
                                    " blocks from height " + std::to_string(lowest_prune_height));
                    } else {
                        DEO_LOG_ERROR(STORAGE, "Failed to prune blocks from height " + std::to_string(lowest_prune_height));
                        blocks_pruned = 0;
                    }
                }
            }
        }
        
        // Prune state (coordinated with block pruning)
        if (!state_to_prune.empty() && state_storage_) {
            state_entries_pruned = performStatePruning(current_height);
        }
        
        updateStatistics(blocks_pruned, state_entries_pruned);
        
    } catch (const std::exception& e) {
        // Log error but don't fail the operation
    }
    
    return blocks_pruned;
}

// Helper function to extract account addresses from a block
static std::set<std::string> extractAccountAddresses(const std::shared_ptr<core::Block>& block) {
    std::set<std::string> addresses;
    
    if (!block) {
        return addresses;
    }
    
    // Extract addresses from all transactions in the block
    for (const auto& tx : block->getTransactions()) {
        if (!tx) {
            continue;
        }
        
        // Extract from inputs - note: inputs reference previous outputs in UTXO model
        // For account-based model, sender address might be derived from public_key
        // For now, we focus on outputs which have explicit addresses
        // TODO: Derive sender addresses from input public keys if needed
        
        // Extract from outputs (recipient addresses)
        for (const auto& output : tx->getOutputs()) {
            if (!output.recipient_address.empty()) {
                addresses.insert(output.recipient_address);
            }
        }
        
        // Check for contract addresses in transaction data
        // Contract deployment/execution might reference addresses
        // This is simplified - in practice you'd parse contract calls
    }
    
    return addresses;
}

uint64_t BlockPruningManager::performStatePruning(uint64_t current_height) {
    if (!state_storage_ || !block_storage_) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Get the number of blocks to keep state for
    uint64_t keep_state_blocks = config_.keep_state_blocks > 0 ? config_.keep_state_blocks : config_.keep_blocks;
    
    if (keep_state_blocks == 0) {
        // Don't prune state if keep_state_blocks is 0 (keep all state)
        return 0;
    }
    
    try {
        // Collect account addresses from recent blocks (the ones we're keeping)
        std::set<std::string> recent_accounts;
        
        // Calculate the starting height for recent blocks
        uint64_t start_height = (current_height >= keep_state_blocks) ? 
                               (current_height - keep_state_blocks + 1) : 0;
        
        // Get all blocks in the range we're keeping
        std::vector<std::shared_ptr<core::Block>> recent_blocks = 
            block_storage_->getBlocksInRange(start_height, current_height);
        
        // Extract all account addresses from recent blocks
        for (const auto& block : recent_blocks) {
            if (block) {
                auto block_accounts = extractAccountAddresses(block);
                recent_accounts.insert(block_accounts.begin(), block_accounts.end());
            }
        }
        
        DEO_LOG_DEBUG(STORAGE, "Found " + std::to_string(recent_accounts.size()) + 
                     " unique accounts in recent " + std::to_string(keep_state_blocks) + " blocks");
        
        // Use LevelDBStateStorage's pruneState method with recent accounts
        uint64_t accounts_pruned = state_storage_->pruneState(keep_state_blocks, current_height, recent_accounts);
        
        DEO_LOG_INFO(STORAGE, "State pruning completed: " + std::to_string(accounts_pruned) + 
                    " accounts pruned (keeping state for " + std::to_string(keep_state_blocks) + " blocks, " +
                    std::to_string(recent_accounts.size()) + " accounts preserved)");
        
        return accounts_pruned;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(STORAGE, "Failed to perform state pruning: " + std::string(e.what()));
        return 0;
    }
}

bool BlockPruningManager::createSnapshot(uint64_t block_height) {
    if (!state_storage_) {
        return false;
    }
    
    try {
        // Create snapshot directory
        std::string snapshot_dir = config_.archive_path + "/snapshots";
        std::filesystem::create_directories(snapshot_dir);
        
        // Create snapshot file
        std::string snapshot_file = snapshot_dir + "/snapshot_" + std::to_string(block_height) + ".json";
        
        nlohmann::json snapshot_data;
        snapshot_data["block_height"] = block_height;
        snapshot_data["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Get all accounts
        auto account_addresses = state_storage_->getAllAccountAddresses();
        nlohmann::json accounts_json = nlohmann::json::array();
        
        for (const auto& address : account_addresses) {
            auto account = state_storage_->getAccount(address);
            if (account) {
                nlohmann::json account_json;
                account_json["address"] = account->address;
                account_json["balance"] = account->balance;
                account_json["nonce"] = account->nonce;
                account_json["code_hash"] = account->code_hash;
                account_json["last_updated"] = account->last_updated;
                
                // Serialize storage
                nlohmann::json storage_json;
                for (const auto& [key, value] : account->storage) {
                    storage_json[key] = value;
                }
                account_json["storage"] = storage_json;
                
                accounts_json.push_back(account_json);
            }
        }
        
        snapshot_data["accounts"] = accounts_json;
        
        // Write snapshot to file
        std::ofstream file(snapshot_file);
        if (file.is_open()) {
            file << snapshot_data.dump(2);
            file.close();
            return true;
        }
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return false;
}

bool BlockPruningManager::restoreFromSnapshot(uint64_t block_height) {
    if (!state_storage_) {
        return false;
    }
    
    try {
        std::string snapshot_file = config_.archive_path + "/snapshots/snapshot_" + 
                                   std::to_string(block_height) + ".json";
        
        if (!std::filesystem::exists(snapshot_file)) {
            return false;
        }
        
        // Read snapshot file
        std::ifstream file(snapshot_file);
        if (!file.is_open()) {
            return false;
        }
        
        nlohmann::json snapshot_data;
        file >> snapshot_data;
        file.close();
        
        // Restore accounts
        if (snapshot_data.contains("accounts")) {
            for (const auto& account_json : snapshot_data["accounts"]) {
                AccountState account;
                account.address = account_json["address"];
                account.balance = account_json["balance"];
                account.nonce = account_json["nonce"];
                account.code_hash = account_json["code_hash"];
                account.last_updated = account_json["last_updated"];
                
                // Restore storage
                if (account_json.contains("storage")) {
                    for (auto it = account_json["storage"].begin(); 
                         it != account_json["storage"].end(); ++it) {
                        account.storage[it.key()] = it.value();
                    }
                }
                
                state_storage_->storeAccount(account.address, account);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return false;
}

std::vector<uint64_t> BlockPruningManager::listSnapshots() const {
    std::vector<uint64_t> snapshots;
    
    try {
        std::string snapshot_dir = config_.archive_path + "/snapshots";
        
        if (std::filesystem::exists(snapshot_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(snapshot_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string filename = entry.path().filename().string();
                    if (filename.substr(0, 9) == "snapshot_") {
                        std::string height_str = filename.substr(9, filename.length() - 14); // Remove "snapshot_" and ".json"
                        try {
                            uint64_t height = std::stoull(height_str);
                            snapshots.push_back(height);
                        } catch (const std::exception&) {
                            // Skip invalid filenames
                        }
                    }
                }
            }
        }
        
        std::sort(snapshots.begin(), snapshots.end());
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return snapshots;
}

uint64_t BlockPruningManager::archiveBlocks(uint64_t from_height, uint64_t to_height) {
    if (!block_storage_ || !config_.enable_archival) {
        return 0;
    }
    
    uint64_t blocks_archived = 0;
    
    try {
        // Create archive directory
        if (!createArchiveDirectory()) {
            return 0;
        }
        
        // Archive blocks in the range
        for (uint64_t height = from_height; height <= to_height; ++height) {
            auto block = block_storage_->getBlockByHeight(height);
            if (block) {
                std::string archive_file = config_.archive_path + "/blocks/block_" + 
                                          std::to_string(height) + ".json";
                
                std::string serialized_block = serializeBlockForArchive(block);
                
                std::ofstream file(archive_file);
                if (file.is_open()) {
                    file << serialized_block;
                    file.close();
                    blocks_archived++;
                }
            }
        }
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        total_blocks_archived_ += blocks_archived;
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return blocks_archived;
}

uint64_t BlockPruningManager::restoreArchivedBlocks(uint64_t from_height, uint64_t to_height) {
    if (!block_storage_ || !config_.enable_archival) {
        return 0;
    }
    
    uint64_t blocks_restored = 0;
    
    try {
        for (uint64_t height = from_height; height <= to_height; ++height) {
            std::string archive_file = config_.archive_path + "/blocks/block_" + 
                                      std::to_string(height) + ".json";
            
            if (std::filesystem::exists(archive_file)) {
                std::ifstream file(archive_file);
                if (file.is_open()) {
                    std::string serialized_block((std::istreambuf_iterator<char>(file)),
                                                std::istreambuf_iterator<char>());
                    file.close();
                    
                    auto block = deserializeBlockFromArchive(serialized_block);
                    if (block && block_storage_->storeBlock(block)) {
                        blocks_restored++;
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return blocks_restored;
}

std::vector<std::pair<uint64_t, uint64_t>> BlockPruningManager::listArchivedRanges() const {
    std::vector<std::pair<uint64_t, uint64_t>> ranges;
    
    try {
        std::string archive_dir = config_.archive_path + "/blocks";
        
        if (std::filesystem::exists(archive_dir)) {
            std::vector<uint64_t> heights;
            
            for (const auto& entry : std::filesystem::directory_iterator(archive_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string filename = entry.path().filename().string();
                    if (filename.substr(0, 6) == "block_") {
                        std::string height_str = filename.substr(6, filename.length() - 11); // Remove "block_" and ".json"
                        try {
                            uint64_t height = std::stoull(height_str);
                            heights.push_back(height);
                        } catch (const std::exception&) {
                            // Skip invalid filenames
                        }
                    }
                }
            }
            
            std::sort(heights.begin(), heights.end());
            
            // Group consecutive heights into ranges
            if (!heights.empty()) {
                uint64_t range_start = heights[0];
                uint64_t range_end = heights[0];
                
                for (size_t i = 1; i < heights.size(); ++i) {
                    if (heights[i] == range_end + 1) {
                        range_end = heights[i];
                    } else {
                        ranges.emplace_back(range_start, range_end);
                        range_start = heights[i];
                        range_end = heights[i];
                    }
                }
                
                ranges.emplace_back(range_start, range_end);
            }
        }
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return ranges;
}

std::string BlockPruningManager::getStorageStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    nlohmann::json stats;
    stats["total_blocks_pruned"] = total_blocks_pruned_;
    stats["total_state_entries_pruned"] = total_state_entries_pruned_;
    stats["total_blocks_archived"] = total_blocks_archived_;
    
    if (last_pruning_time_ != std::chrono::system_clock::time_point{}) {
        stats["last_pruning_time"] = std::chrono::duration_cast<std::chrono::seconds>(
            last_pruning_time_.time_since_epoch()).count();
    }
    
    stats["storage_size_bytes"] = calculateStorageSize();
    stats["auto_pruning_active"] = auto_pruning_active_.load();
    
    return stats.dump(2);
}

std::string BlockPruningManager::getPruningStatistics() const {
    std::lock_guard<std::mutex> config_lock(config_mutex_);
    
    nlohmann::json stats;
    stats["mode"] = static_cast<int>(config_.mode);
    stats["keep_blocks"] = config_.keep_blocks;
    stats["keep_state_blocks"] = config_.keep_state_blocks;
    stats["snapshot_interval"] = config_.snapshot_interval;
    stats["max_storage_size_mb"] = config_.max_storage_size_mb;
    stats["max_block_count"] = config_.max_block_count;
    stats["max_age_hours"] = config_.max_age_hours.count();
    stats["enable_archival"] = config_.enable_archival;
    stats["archive_path"] = config_.archive_path;
    stats["archive_after_blocks"] = config_.archive_after_blocks;
    
    return stats.dump(2);
}

uint64_t BlockPruningManager::estimateStorageSavings(const PruningConfig& config) const {
    // This is a simplified estimation - in practice, you'd need more sophisticated calculations
    uint64_t estimated_savings = 0;
    
    if (block_storage_) {
        uint64_t total_blocks = block_storage_->getBlockCount();
        
        if (config.keep_blocks > 0 && total_blocks > config.keep_blocks) {
            uint64_t blocks_to_prune = total_blocks - config.keep_blocks;
            // Estimate 1MB per block (this is a rough estimate)
            estimated_savings += blocks_to_prune * 1024 * 1024;
        }
    }
    
    return estimated_savings;
}

bool BlockPruningManager::startAutomaticPruning(uint64_t interval_seconds) {
    if (auto_pruning_active_.load()) {
        return false; // Already running
    }
    
    auto_pruning_interval_.store(interval_seconds);
    auto_pruning_active_.store(true);
    
    auto_pruning_thread_ = std::thread(&BlockPruningManager::automaticPruningLoop, this);
    
    return true;
}

void BlockPruningManager::stopAutomaticPruning() {
    if (auto_pruning_active_.load()) {
        auto_pruning_active_.store(false);
        auto_pruning_cv_.notify_all();
        
        if (auto_pruning_thread_.joinable()) {
            auto_pruning_thread_.join();
        }
    }
}

bool BlockPruningManager::isAutomaticPruningActive() const {
    return auto_pruning_active_.load();
}

void BlockPruningManager::automaticPruningLoop() {
    std::unique_lock<std::mutex> lock(auto_pruning_mutex_);
    
    while (auto_pruning_active_.load()) {
        auto_pruning_cv_.wait_for(lock, std::chrono::seconds(auto_pruning_interval_.load()));
        
        if (auto_pruning_active_.load()) {
            if (block_storage_) {
                uint64_t current_height = block_storage_->getCurrentHeight();
                performPruning(current_height);
            }
        }
    }
}

bool BlockPruningManager::isStorageLimitExceeded() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (config_.max_storage_size_mb == 0) {
        return false; // No limit set
    }
    
    uint64_t current_size = calculateStorageSize();
    uint64_t limit_bytes = config_.max_storage_size_mb * 1024 * 1024;
    
    return current_size > limit_bytes;
}

uint64_t BlockPruningManager::calculateStorageSize() const {
    // This is a simplified calculation - in practice, you'd need to calculate actual disk usage
    uint64_t size = 0;
    
    if (block_storage_) {
        size += block_storage_->getBlockCount() * 1024 * 1024; // Rough estimate
    }
    
    if (state_storage_) {
        size += state_storage_->getAccountCount() * 1024; // Rough estimate
        size += state_storage_->getStorageEntryCount() * 256; // Rough estimate
    }
    
    return size;
}

std::chrono::hours BlockPruningManager::getBlockAge(const std::shared_ptr<core::Block>& block) const {
    if (!block) {
        return std::chrono::hours(0);
    }
    
    auto block_time = std::chrono::system_clock::time_point(std::chrono::seconds(block->getHeader().timestamp));
    auto now = std::chrono::system_clock::now();
    
    return std::chrono::duration_cast<std::chrono::hours>(now - block_time);
}

void BlockPruningManager::updateStatistics(uint64_t blocks_pruned, uint64_t state_entries_pruned) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    total_blocks_pruned_ += blocks_pruned;
    total_state_entries_pruned_ += state_entries_pruned;
    last_pruning_time_ = std::chrono::system_clock::now();
}

bool BlockPruningManager::createArchiveDirectory() const {
    try {
        std::filesystem::create_directories(config_.archive_path);
        std::filesystem::create_directories(config_.archive_path + "/blocks");
        std::filesystem::create_directories(config_.archive_path + "/snapshots");
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::string BlockPruningManager::serializeBlockForArchive(const std::shared_ptr<core::Block>& block) const {
    // Use the same serialization as LevelDBBlockStorage
    // This is a simplified version - in practice, you'd reuse the existing serialization
    nlohmann::json block_json;
    
    const auto& header = block->getHeader();
    nlohmann::json header_json;
    header_json["version"] = header.version;
    header_json["previous_hash"] = header.previous_hash;
    header_json["merkle_root"] = header.merkle_root;
    header_json["timestamp"] = header.timestamp;
    header_json["nonce"] = header.nonce;
    header_json["difficulty"] = header.difficulty;
    header_json["height"] = header.height;
    
    block_json["header"] = header_json;
    block_json["transactions"] = nlohmann::json::array(); // Simplified for now
    
    return block_json.dump();
}

std::shared_ptr<core::Block> BlockPruningManager::deserializeBlockFromArchive(const std::string& data) const {
    // Use the same deserialization as LevelDBBlockStorage
    // This is a simplified version - in practice, you'd reuse the existing deserialization
    try {
        nlohmann::json block_json = nlohmann::json::parse(data);
        
        const auto& header_json = block_json["header"];
        core::BlockHeader header;
        header.version = header_json["version"];
        header.previous_hash = header_json["previous_hash"];
        header.merkle_root = header_json["merkle_root"];
        header.timestamp = header_json["timestamp"];
        header.nonce = header_json["nonce"];
        header.difficulty = header_json["difficulty"];
        header.height = header_json["height"];
        
        // Create empty transaction list for now
        std::vector<std::shared_ptr<core::Transaction>> transactions;
        
        return std::make_shared<core::Block>(header, transactions);
        
    } catch (const std::exception& e) {
        return nullptr;
    }
}

} // namespace storage
} // namespace deo
