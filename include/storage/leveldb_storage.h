/**
 * @file leveldb_storage.h
 * @brief LevelDB-based storage system for the Deo Blockchain
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
#include <leveldb/db.h>
#include <leveldb/options.h>
#include <leveldb/write_batch.h>
#include <leveldb/iterator.h>
#include <leveldb/filter_policy.h>
#include <leveldb/cache.h>
#include <set>

#include "core/block.h"
#include "core/transaction.h"
#include "storage/state_storage.h"

namespace deo {
namespace storage {

/**
 * @brief LevelDB-based block storage implementation
 * 
 * This class provides high-performance persistent storage for blockchain blocks
 * using LevelDB, with efficient indexing and retrieval capabilities.
 */
class LevelDBBlockStorage {
public:
    /**
     * @brief Constructor
     * @param data_directory Directory for storing blockchain data
     */
    explicit LevelDBBlockStorage(const std::string& data_directory);
    
    /**
     * @brief Destructor
     */
    ~LevelDBBlockStorage();
    
    // Disable copy and move semantics
    LevelDBBlockStorage(const LevelDBBlockStorage&) = delete;
    LevelDBBlockStorage& operator=(const LevelDBBlockStorage&) = delete;
    LevelDBBlockStorage(LevelDBBlockStorage&&) = delete;
    LevelDBBlockStorage& operator=(LevelDBBlockStorage&&) = delete;

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
    bool storeBlock(const std::shared_ptr<core::Block>& block);
    
    /**
     * @brief Retrieve a block by hash
     * @param block_hash Hash of the block to retrieve
     * @return Block if found, nullptr otherwise
     */
    std::shared_ptr<core::Block> getBlock(const std::string& block_hash);
    
    /**
     * @brief Retrieve a block by height
     * @param height Height of the block to retrieve
     * @return Block if found, nullptr otherwise
     */
    std::shared_ptr<core::Block> getBlockByHeight(uint64_t height);
    
    /**
     * @brief Get the latest block
     * @return Latest block if found, nullptr otherwise
     */
    std::shared_ptr<core::Block> getLatestBlock();
    
    /**
     * @brief Get the genesis block
     * @return Genesis block if found, nullptr otherwise
     */
    std::shared_ptr<core::Block> getGenesisBlock();
    
    /**
     * @brief Get block count
     * @return Number of blocks stored
     */
    uint64_t getBlockCount();
    
    /**
     * @brief Get the current blockchain height
     * @return Current blockchain height
     */
    uint64_t getCurrentHeight();
    
    /**
     * @brief Check if a block exists
     * @param block_hash Hash of the block to check
     * @return True if block exists
     */
    bool hasBlock(const std::string& block_hash);
    
    /**
     * @brief Get blocks in a height range
     * @param start_height Starting height (inclusive)
     * @param end_height Ending height (inclusive)
     * @return Vector of blocks in the range
     */
    std::vector<std::shared_ptr<core::Block>> getBlocksInRange(uint64_t start_height, uint64_t end_height);
    
    /**
     * @brief Get block hashes in a height range
     * @param start_height Starting height (inclusive)
     * @param end_height Ending height (inclusive)
     * @return Vector of block hashes in the range
     */
    std::vector<std::string> getBlockHashesInRange(uint64_t start_height, uint64_t end_height);
    
    /**
     * @brief Delete blocks from a specific height onwards (for reorgs)
     * @param from_height Height from which to start deleting
     * @return True if deletion was successful
     */
    bool deleteBlocksFromHeight(uint64_t from_height);
    
    /**
     * @brief Get storage statistics
     * @return JSON string with storage statistics
     */
    std::string getStatistics() const;
    
    /**
     * @brief Compact the database
     * @return True if compaction was successful
     */
    bool compactDatabase();
    
    /**
     * @brief Repair the database
     * @return True if repair was successful
     */
    bool repairDatabase();

private:
    std::string data_directory_;
    std::unique_ptr<leveldb::DB> db_;
    std::unique_ptr<leveldb::Cache> block_cache_;
    std::unique_ptr<const leveldb::FilterPolicy> filter_policy_;
    mutable std::mutex storage_mutex_;
    
    // Database options
    leveldb::Options db_options_;
    leveldb::ReadOptions read_options_;
    leveldb::WriteOptions write_options_;
    
    // Key prefixes for different data types
    static constexpr const char* BLOCK_PREFIX = "block:";
    static constexpr const char* HEIGHT_PREFIX = "height:";
    static constexpr const char* LATEST_KEY = "latest";
    static constexpr const char* GENESIS_KEY = "genesis";
    static constexpr const char* COUNT_KEY = "count";
    static constexpr const char* HEIGHT_KEY = "height";
    
    /**
     * @brief Create database key for block by hash
     * @param block_hash Block hash
     * @return Database key
     */
    std::string createBlockKey(const std::string& block_hash);
    
    /**
     * @brief Create database key for block by height
     * @param height Block height
     * @return Database key
     */
    std::string createHeightKey(uint64_t height);
    
    /**
     * @brief Serialize block to string
     * @param block Block to serialize
     * @return Serialized block data
     */
    std::string serializeBlock(const std::shared_ptr<core::Block>& block);
    
    /**
     * @brief Deserialize block from string
     * @param data Serialized block data
     * @return Deserialized block
     */
    std::shared_ptr<core::Block> deserializeBlock(const std::string& data);
    
    /**
     * @brief Update metadata after storing a block
     * @param block Block that was stored
     */
    void updateMetadata(const std::shared_ptr<core::Block>& block);
    
    /**
     * @brief Initialize database options
     */
    void initializeOptions();
    
    /**
     * @brief Create data directory if it doesn't exist
     * @return True if directory creation was successful
     */
    bool createDataDirectory();
};

/**
 * @brief LevelDB-based state storage implementation
 * 
 * This class provides high-performance persistent storage for blockchain state
 * using LevelDB, with efficient account and contract storage management.
 */
class LevelDBStateStorage {
public:
    /**
     * @brief Constructor
     * @param data_directory Directory for storing state data
     */
    explicit LevelDBStateStorage(const std::string& data_directory);
    
    /**
     * @brief Destructor
     */
    ~LevelDBStateStorage();
    
    // Disable copy and move semantics
    LevelDBStateStorage(const LevelDBStateStorage&) = delete;
    LevelDBStateStorage& operator=(const LevelDBStateStorage&) = delete;
    LevelDBStateStorage(LevelDBStateStorage&&) = delete;
    LevelDBStateStorage& operator=(LevelDBStateStorage&&) = delete;

    /**
     * @brief Initialize the storage system
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the storage system
     */
    void shutdown();

    // Account management
    bool storeAccount(const std::string& address, const AccountState& account);
    std::shared_ptr<AccountState> getAccount(const std::string& address);
    bool hasAccount(const std::string& address);
    bool deleteAccount(const std::string& address);
    
    // Contract storage management
    bool storeContractStorage(const std::string& contract_address, const std::string& key, const std::string& value);
    std::string getContractStorage(const std::string& contract_address, const std::string& key);
    bool hasContractStorage(const std::string& contract_address, const std::string& key);
    bool deleteContractStorage(const std::string& contract_address, const std::string& key);
    
    // Batch operations
    bool storeAccountBatch(const std::unordered_map<std::string, AccountState>& accounts);
    bool storeContractStorageBatch(const std::string& contract_address, const std::map<std::string, std::string>& storage);
    
    // Query operations
    std::vector<std::string> getAllAccountAddresses();
    std::vector<std::string> getContractAddresses();
    std::map<std::string, std::string> getAllContractStorage(const std::string& contract_address);
    
    // Statistics
    uint64_t getAccountCount();
    uint64_t getContractCount();
    uint64_t getStorageEntryCount();
    std::string getStatistics() const;
    
    // State pruning
    /**
     * @brief Prune state by removing accounts not referenced in recent blocks
     * @param keep_blocks Number of recent blocks to keep state for
     * @param current_height Current blockchain height
     * @param recent_accounts Set of account addresses referenced in recent blocks (to keep)
     * @return Number of accounts pruned
     */
    uint64_t pruneState(uint64_t keep_blocks, uint64_t current_height, 
                       const std::set<std::string>& recent_accounts = {});
    
    // Maintenance
    bool compactDatabase();
    bool repairDatabase();

private:
    std::string data_directory_;
    std::unique_ptr<leveldb::DB> db_;
    std::unique_ptr<leveldb::Cache> state_cache_;
    std::unique_ptr<const leveldb::FilterPolicy> filter_policy_;
    mutable std::mutex storage_mutex_;
    
    // Database options
    leveldb::Options db_options_;
    leveldb::ReadOptions read_options_;
    leveldb::WriteOptions write_options_;
    
    // Key prefixes for different data types
    static constexpr const char* ACCOUNT_PREFIX = "account:";
    static constexpr const char* CONTRACT_PREFIX = "contract:";
    static constexpr const char* STORAGE_PREFIX = "storage:";
    static constexpr const char* COUNT_PREFIX = "count:";
    
    /**
     * @brief Create database key for account
     * @param address Account address
     * @return Database key
     */
    std::string createAccountKey(const std::string& address);
    
    /**
     * @brief Create database key for contract storage
     * @param contract_address Contract address
     * @param key Storage key
     * @return Database key
     */
    std::string createStorageKey(const std::string& contract_address, const std::string& key);
    
    /**
     * @brief Create database key for contract
     * @param contract_address Contract address
     * @return Database key
     */
    std::string createContractKey(const std::string& contract_address);
    
    /**
     * @brief Serialize account state to string
     * @param account Account state to serialize
     * @return Serialized account data
     */
    std::string serializeAccount(const AccountState& account);
    
    /**
     * @brief Deserialize account state from string
     * @param data Serialized account data
     * @return Deserialized account state
     */
    std::shared_ptr<AccountState> deserializeAccount(const std::string& data);
    
    /**
     * @brief Initialize database options
     */
    void initializeOptions();
    
    /**
     * @brief Create data directory if it doesn't exist
     * @return True if directory creation was successful
     */
    bool createDataDirectory();
    
    /**
     * @brief Update account count
     * @param delta Change in account count (+1 or -1)
     */
    void updateAccountCount(int64_t delta);
    
    /**
     * @brief Update contract count
     * @param delta Change in contract count (+1 or -1)
     */
    void updateContractCount(int64_t delta);
    
    /**
     * @brief Update storage entry count
     * @param delta Change in storage entry count (+1 or -1)
     */
    void updateStorageEntryCount(int64_t delta);
};

} // namespace storage
} // namespace deo
