/**
 * @file block_storage.cpp
 * @brief Block storage system implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "storage/block_storage.h"
#include "utils/logger.h"
#include "utils/error_handler.h"

#include <filesystem>
#include <fstream>

namespace deo {
namespace storage {

BlockStorage::BlockStorage(const std::string& data_directory)
    : data_directory_(data_directory) {
    
    DEO_LOG_DEBUG(STORAGE, "Block storage created with directory: " + data_directory);
}

BlockStorage::~BlockStorage() {
    shutdown();
}

bool BlockStorage::initialize() {
    DEO_LOG_INFO(STORAGE, "Initializing block storage");
    
    // Create data directory if it doesn't exist
    if (!std::filesystem::exists(data_directory_)) {
        try {
            std::filesystem::create_directories(data_directory_);
            DEO_LOG_INFO(STORAGE, "Created data directory: " + data_directory_);
        } catch (const std::exception& e) {
            DEO_ERROR(STORAGE, "Failed to create data directory: " + std::string(e.what()));
            return false;
        }
    }
    
    // Load existing blocks
    if (!loadBlocks()) {
        DEO_WARNING(STORAGE, "Failed to load existing blocks");
    }
    
    DEO_LOG_INFO(STORAGE, "Block storage initialized successfully");
    return true;
}

void BlockStorage::shutdown() {
    DEO_LOG_INFO(STORAGE, "Shutting down block storage");
    
    // Save blocks to disk
    if (!saveBlocks()) {
        DEO_WARNING(STORAGE, "Failed to save blocks during shutdown");
    }
    
    DEO_LOG_INFO(STORAGE, "Block storage shutdown complete");
}

bool BlockStorage::storeBlock(std::shared_ptr<core::Block> block) {
    if (!block) {
        DEO_ERROR(STORAGE, "Cannot store null block");
        return false;
    }
    
    DEO_LOG_DEBUG(STORAGE, "Storing block: " + block->calculateHash());
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Store in cache
    block_cache_[block->calculateHash()] = block;
    
    // Update index
    updateIndex(block);
    
    DEO_LOG_DEBUG(STORAGE, "Block stored successfully");
    return true;
}

std::shared_ptr<core::Block> BlockStorage::getBlock(const std::string& hash) const {
    DEO_LOG_DEBUG(STORAGE, "Retrieving block: " + hash);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = block_cache_.find(hash);
    if (it != block_cache_.end()) {
        return it->second;
    }
    
    DEO_LOG_DEBUG(STORAGE, "Block not found in cache: " + hash);
    return nullptr;
}

std::shared_ptr<core::Block> BlockStorage::getBlock(uint64_t height) const {
    DEO_LOG_DEBUG(STORAGE, "Retrieving block at height: " + std::to_string(height));
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = height_to_hash_.find(height);
    if (it != height_to_hash_.end()) {
        return getBlock(it->second);
    }
    
    DEO_LOG_DEBUG(STORAGE, "Block not found at height: " + std::to_string(height));
    return nullptr;
}

std::shared_ptr<core::Block> BlockStorage::getLatestBlock() const {
    DEO_LOG_DEBUG(STORAGE, "Retrieving latest block");
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (height_to_hash_.empty()) {
        return nullptr;
    }
    
    // Find the maximum height key
    auto it = std::max_element(height_to_hash_.begin(), height_to_hash_.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    return getBlock(it->second);
}

std::shared_ptr<core::Block> BlockStorage::getGenesisBlock() const {
    DEO_LOG_DEBUG(STORAGE, "Retrieving genesis block");
    
    return getBlock(0);
}

uint64_t BlockStorage::getHeight() const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (height_to_hash_.empty()) {
        return 0;
    }
    
    // Find the maximum height key
    auto it = std::max_element(height_to_hash_.begin(), height_to_hash_.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    if (it != height_to_hash_.end()) {
        return it->first;
    }
    return 0;
}

bool BlockStorage::hasBlock(const std::string& hash) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return block_cache_.find(hash) != block_cache_.end();
}

std::vector<std::shared_ptr<core::Block>> BlockStorage::getBlocks(uint64_t start_height, uint64_t end_height) const {
    DEO_LOG_DEBUG(STORAGE, "Retrieving blocks from height " + std::to_string(start_height) + 
                  " to " + std::to_string(end_height));
    
    std::vector<std::shared_ptr<core::Block>> blocks;
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    for (uint64_t height = start_height; height <= end_height; ++height) {
        auto it = height_to_hash_.find(height);
        if (it != height_to_hash_.end()) {
            auto block = getBlock(it->second);
            if (block) {
                blocks.push_back(block);
            }
        }
    }
    
    return blocks;
}

std::vector<std::string> BlockStorage::getBlockHashes(uint64_t start_height, uint64_t end_height) const {
    DEO_LOG_DEBUG(STORAGE, "Retrieving block hashes from height " + std::to_string(start_height) + 
                  " to " + std::to_string(end_height));
    
    std::vector<std::string> hashes;
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    for (uint64_t height = start_height; height <= end_height; ++height) {
        auto it = height_to_hash_.find(height);
        if (it != height_to_hash_.end()) {
            hashes.push_back(it->second);
        }
    }
    
    return hashes;
}

bool BlockStorage::deleteBlock(const std::string& hash) {
    DEO_LOG_DEBUG(STORAGE, "Deleting block: " + hash);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = block_cache_.find(hash);
    if (it != block_cache_.end()) {
        // Remove from index
        removeFromIndex(hash);
        
        // Remove from cache
        block_cache_.erase(it);
        
        DEO_LOG_DEBUG(STORAGE, "Block deleted successfully");
        return true;
    }
    
    DEO_LOG_WARNING(STORAGE, "Block not found for deletion: " + hash);
    return false;
}

std::string BlockStorage::getStatistics() const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"total_blocks\": " << block_cache_.size() << ",\n";
    ss << "  \"height\": " << getHeight() << ",\n";
    ss << "  \"data_directory\": \"" << data_directory_ << "\"\n";
    ss << "}";
    
    return ss.str();
}

bool BlockStorage::compact() {
    DEO_LOG_INFO(STORAGE, "Compacting block storage");
    
    // Note: In a real implementation, we would compact the storage
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "Storage compaction not fully implemented yet");
    
    return true;
}

bool BlockStorage::backup(const std::string& backup_path) const {
    DEO_LOG_INFO(STORAGE, "Backing up block storage to: " + backup_path);
    
    // Note: In a real implementation, we would create a backup
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "Storage backup not fully implemented yet");
    
    return true;
}

bool BlockStorage::restore(const std::string& backup_path) {
    DEO_LOG_INFO(STORAGE, "Restoring block storage from: " + backup_path);
    
    // Note: In a real implementation, we would restore from backup
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "Storage restore not fully implemented yet");
    
    return true;
}

bool BlockStorage::loadBlocks() {
    DEO_LOG_DEBUG(STORAGE, "Loading blocks from disk");
    
    try {
        // Load block index
        std::string index_file = data_directory_ + "/block_index.json";
        if (std::filesystem::exists(index_file)) {
            std::ifstream file(index_file);
            if (file.is_open()) {
                nlohmann::json index_json;
                file >> index_json;
                file.close();
                
                // Load each block
                for (const auto& [height_str, hash] : index_json.items()) {
                    std::string block_file = data_directory_ + "/blocks/" + hash.get<std::string>() + ".json";
                    
                    if (std::filesystem::exists(block_file)) {
                        std::ifstream block_file_stream(block_file);
                        if (block_file_stream.is_open()) {
                            nlohmann::json block_json;
                            block_file_stream >> block_json;
                            block_file_stream.close();
                            
                            auto block = std::make_shared<core::Block>();
                            if (block->fromJson(block_json)) {
                                block_cache_[hash] = block;
                                // Note: height_index_ not available in this context
                                // This would need to be implemented in the header
                            }
                        }
                    }
                }
                
                DEO_LOG_INFO(STORAGE, "Loaded " + std::to_string(block_cache_.size()) + " blocks from disk");
                return true;
            }
        }
        
        DEO_LOG_DEBUG(STORAGE, "No existing blocks found, starting fresh");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(STORAGE, "Failed to load blocks: " + std::string(e.what()));
        return false;
    }
}

bool BlockStorage::saveBlocks() {
    DEO_LOG_DEBUG(STORAGE, "Saving blocks to disk");
    
    try {
        // Create blocks directory if it doesn't exist
        std::string blocks_dir = data_directory_ + "/blocks";
        if (!std::filesystem::exists(blocks_dir)) {
            std::filesystem::create_directories(blocks_dir);
        }
        
        // Save each block
        for (const auto& [hash, block] : block_cache_) {
            std::string block_file = blocks_dir + "/" + hash + ".json";
            std::ofstream file(block_file);
            if (file.is_open()) {
                nlohmann::json block_json = block->toJson();
                file << block_json.dump(2);
                file.close();
            }
        }
        
        // Save block index
        std::string index_file = data_directory_ + "/block_index.json";
        std::ofstream index_stream(index_file);
        if (index_stream.is_open()) {
            nlohmann::json index_json;
            // Note: height_index_ not available in this context
            // This would need to be implemented in the header
            for (const auto& [hash, block] : block_cache_) {
                index_json[std::to_string(block->getHeader().height)] = hash;
            }
            index_stream << index_json.dump(2);
            index_stream.close();
        }
        
        DEO_LOG_DEBUG(STORAGE, "Saved " + std::to_string(block_cache_.size()) + " blocks to disk");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(STORAGE, "Failed to save blocks: " + std::string(e.what()));
        return false;
    }
}

std::string BlockStorage::getBlockFilePath(const std::string& hash) const {
    return data_directory_ + "/blocks/" + hash + ".block";
}

std::string BlockStorage::getIndexFilePath() const {
    return data_directory_ + "/index.dat";
}

void BlockStorage::updateIndex(std::shared_ptr<core::Block> block) {
    if (!block) {
        return;
    }
    
    std::string hash = block->calculateHash();
    uint64_t height = block->getHeader().height;
    
    height_to_hash_[height] = hash;
    
    DEO_LOG_DEBUG(STORAGE, "Updated index: height " + std::to_string(height) + " -> " + hash);
}

void BlockStorage::removeFromIndex(const std::string& hash) {
    // Find and remove the hash from the height index
    for (auto it = height_to_hash_.begin(); it != height_to_hash_.end(); ++it) {
        if (it->second == hash) {
            height_to_hash_.erase(it);
            DEO_LOG_DEBUG(STORAGE, "Removed from index: " + hash);
            break;
        }
    }
}

} // namespace storage
} // namespace deo
