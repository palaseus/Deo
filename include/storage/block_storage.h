/**
 * @file block_storage.h
 * @brief Block storage system for the Deo Blockchain
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

#include "core/block.h"

namespace deo {
namespace storage {

/**
 * @brief Block storage interface
 * 
 * This class provides persistent storage for blockchain blocks with
 * efficient indexing and retrieval capabilities.
 */
class BlockStorage {
public:
    /**
     * @brief Constructor
     * @param data_directory Directory for storing blockchain data
     */
    explicit BlockStorage(const std::string& data_directory);
    
    /**
     * @brief Destructor
     */
    ~BlockStorage();
    
    // Disable copy and move semantics
    BlockStorage(const BlockStorage&) = delete;
    BlockStorage& operator=(const BlockStorage&) = delete;
    BlockStorage(BlockStorage&&) = delete;
    BlockStorage& operator=(BlockStorage&&) = delete;

    /**
     * @brief Initialize the storage system
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the storage system
     */
    void shutdown();
    
    /**
     * @brief Store a block
     * @param block Block to store
     * @return True if storage was successful
     */
    bool storeBlock(std::shared_ptr<core::Block> block);
    
    /**
     * @brief Retrieve a block by hash
     * @param hash Block hash
     * @return Block if found, nullptr otherwise
     */
    std::shared_ptr<core::Block> getBlock(const std::string& hash) const;
    
    /**
     * @brief Retrieve a block by height
     * @param height Block height
     * @return Block if found, nullptr otherwise
     */
    std::shared_ptr<core::Block> getBlock(uint64_t height) const;
    
    /**
     * @brief Get the latest block
     * @return Latest block if available, nullptr otherwise
     */
    std::shared_ptr<core::Block> getLatestBlock() const;
    
    /**
     * @brief Get the genesis block
     * @return Genesis block if available, nullptr otherwise
     */
    std::shared_ptr<core::Block> getGenesisBlock() const;
    
    /**
     * @brief Get blockchain height
     * @return Current blockchain height
     */
    uint64_t getHeight() const;
    
    /**
     * @brief Check if a block exists
     * @param hash Block hash
     * @return True if block exists
     */
    bool hasBlock(const std::string& hash) const;
    
    /**
     * @brief Get blocks in a height range
     * @param start_height Starting height
     * @param end_height Ending height
     * @return List of blocks in the range
     */
    std::vector<std::shared_ptr<core::Block>> getBlocks(uint64_t start_height, uint64_t end_height) const;
    
    /**
     * @brief Load all blocks from storage
     * @return List of all blocks
     */
    std::vector<std::shared_ptr<core::Block>> loadAllBlocks() const;
    
    /**
     * @brief Get block hashes in a height range
     * @param start_height Starting height
     * @param end_height Ending height
     * @return List of block hashes in the range
     */
    std::vector<std::string> getBlockHashes(uint64_t start_height, uint64_t end_height) const;
    
    /**
     * @brief Delete a block
     * @param hash Block hash to delete
     * @return True if deletion was successful
     */
    bool deleteBlock(const std::string& hash);
    
    /**
     * @brief Get storage statistics
     * @return Storage statistics as JSON string
     */
    std::string getStatistics() const;
    
    /**
     * @brief Compact the storage
     * @return True if compaction was successful
     */
    bool compact();
    
    /**
     * @brief Backup the storage
     * @param backup_path Backup file path
     * @return True if backup was successful
     */
    bool backup(const std::string& backup_path) const;
    
    /**
     * @brief Restore from backup
     * @param backup_path Backup file path
     * @return True if restore was successful
     */
    bool restore(const std::string& backup_path);

private:
    std::string data_directory_;                                    ///< Data directory path
    std::unordered_map<std::string, std::shared_ptr<core::Block>> block_cache_; ///< Block cache
    std::unordered_map<uint64_t, std::string> height_to_hash_;     ///< Height to hash mapping
    mutable std::mutex storage_mutex_;                             ///< Mutex for thread safety
    
    /**
     * @brief Load blocks from disk
     * @return True if loading was successful
     */
    bool loadBlocks();
    
    /**
     * @brief Save blocks to disk
     * @return True if saving was successful
     */
    bool saveBlocks();
    
    /**
     * @brief Get block file path
     * @param hash Block hash
     * @return File path for the block
     */
    std::string getBlockFilePath(const std::string& hash) const;
    
    /**
     * @brief Get index file path
     * @return Index file path
     */
    std::string getIndexFilePath() const;
    
    /**
     * @brief Update the index
     * @param block Block to index
     */
    void updateIndex(std::shared_ptr<core::Block> block);
    
    /**
     * @brief Remove from index
     * @param hash Block hash to remove
     */
    void removeFromIndex(const std::string& hash);
};

} // namespace storage
} // namespace deo
