/**
 * @file state_storage.h
 * @brief State storage system for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <mutex>
#include <cstdint>

namespace deo {
namespace storage {

/**
 * @brief Account state information
 */
struct AccountState {
    std::string address;        ///< Account address
    uint64_t balance;          ///< Account balance
    uint64_t nonce;            ///< Account nonce
    std::string code_hash;     ///< Contract code hash (if applicable)
    std::map<std::string, std::string> storage; ///< Contract storage (if applicable)
    uint64_t last_updated;     ///< Last update timestamp
};

/**
 * @brief State storage interface
 * 
 * This class provides persistent storage for blockchain state including
 * account balances, contract storage, and other state information.
 */
class StateStorage {
public:
    /**
     * @brief Constructor
     * @param data_directory Directory for storing state data
     */
    explicit StateStorage(const std::string& data_directory);
    
    /**
     * @brief Destructor
     */
    ~StateStorage();
    
    // Disable copy and move semantics
    StateStorage(const StateStorage&) = delete;
    StateStorage& operator=(const StateStorage&) = delete;
    StateStorage(StateStorage&&) = delete;
    StateStorage& operator=(StateStorage&&) = delete;

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
     * @brief Get account state
     * @param address Account address
     * @return Account state if found, nullptr otherwise
     */
    std::shared_ptr<AccountState> getAccountState(const std::string& address) const;
    
    /**
     * @brief Set account state
     * @param address Account address
     * @param state Account state
     * @return True if setting was successful
     */
    bool setAccountState(const std::string& address, const AccountState& state);
    
    /**
     * @brief Get account balance
     * @param address Account address
     * @return Account balance
     */
    uint64_t getBalance(const std::string& address) const;
    
    /**
     * @brief Set account balance
     * @param address Account address
     * @param balance New balance
     * @return True if setting was successful
     */
    bool setBalance(const std::string& address, uint64_t balance);
    
    /**
     * @brief Get account nonce
     * @param address Account address
     * @return Account nonce
     */
    uint64_t getNonce(const std::string& address) const;
    
    /**
     * @brief Set account nonce
     * @param address Account address
     * @param nonce New nonce
     * @return True if setting was successful
     */
    bool setNonce(const std::string& address, uint64_t nonce);
    
    /**
     * @brief Increment account nonce
     * @param address Account address
     * @return New nonce value
     */
    uint64_t incrementNonce(const std::string& address);
    
    /**
     * @brief Get contract storage value
     * @param address Contract address
     * @param key Storage key
     * @return Storage value
     */
    std::string getStorageValue(const std::string& address, const std::string& key) const;
    
    /**
     * @brief Set contract storage value
     * @param address Contract address
     * @param key Storage key
     * @param value Storage value
     * @return True if setting was successful
     */
    bool setStorageValue(const std::string& address, const std::string& key, const std::string& value);
    
    /**
     * @brief Get all contract storage
     * @param address Contract address
     * @return Map of storage key-value pairs
     */
    std::map<std::string, std::string> getAllStorage(const std::string& address) const;
    
    /**
     * @brief Check if account exists
     * @param address Account address
     * @return True if account exists
     */
    bool hasAccount(const std::string& address) const;
    
    /**
     * @brief Delete account
     * @param address Account address
     * @return True if deletion was successful
     */
    bool deleteAccount(const std::string& address);
    
    /**
     * @brief Get all accounts
     * @return List of account addresses
     */
    std::vector<std::string> getAllAccounts() const;
    
    /**
     * @brief Get storage statistics
     * @return Storage statistics as JSON string
     */
    std::string getStatistics() const;
    
    /**
     * @brief Create a state snapshot
     * @param snapshot_id Snapshot identifier
     * @return True if snapshot creation was successful
     */
    bool createSnapshot(const std::string& snapshot_id);
    
    /**
     * @brief Restore from a state snapshot
     * @param snapshot_id Snapshot identifier
     * @return True if restore was successful
     */
    bool restoreSnapshot(const std::string& snapshot_id);
    
    /**
     * @brief Delete a state snapshot
     * @param snapshot_id Snapshot identifier
     * @return True if deletion was successful
     */
    bool deleteSnapshot(const std::string& snapshot_id);
    
    /**
     * @brief Get list of available snapshots
     * @return List of snapshot identifiers
     */
    std::vector<std::string> getSnapshots() const;
    
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
    std::unordered_map<std::string, std::shared_ptr<AccountState>> account_cache_; ///< Account cache
    mutable std::mutex storage_mutex_;                             ///< Mutex for thread safety
    
    /**
     * @brief Load state from disk
     * @return True if loading was successful
     */
    bool loadState();
    
    /**
     * @brief Save state to disk
     * @return True if saving was successful
     */
    bool saveState();
    
    /**
     * @brief Get account file path
     * @param address Account address
     * @return File path for the account
     */
    std::string getAccountFilePath(const std::string& address) const;
    
    /**
     * @brief Get state index file path
     * @return Index file path
     */
    std::string getStateIndexFilePath() const;
    
    /**
     * @brief Get snapshot directory path
     * @param snapshot_id Snapshot identifier
     * @return Snapshot directory path
     */
    std::string getSnapshotDirectoryPath(const std::string& snapshot_id) const;
    
    /**
     * @brief Update the state index
     * @param address Account address
     */
    void updateIndex(const std::string& address);
    
    /**
     * @brief Remove from state index
     * @param address Account address to remove
     */
    void removeFromIndex(const std::string& address);
};

} // namespace storage
} // namespace deo
