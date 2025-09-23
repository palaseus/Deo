/**
 * @file state_trie.h
 * @brief State trie implementation for persistent contract storage
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace deo {
namespace core {

/**
 * @brief State trie node for storing key-value pairs
 */
struct StateTrieNode {
    std::string key;                    ///< Node key (address or storage key)
    std::string value;                  ///< Node value (balance, storage value, etc.)
    std::string hash;                   ///< Node hash for integrity
    std::unordered_map<std::string, std::shared_ptr<StateTrieNode>> children; ///< Child nodes
    bool is_leaf;                       ///< Whether this is a leaf node
    uint64_t timestamp;                 ///< Last modification timestamp
    
    /**
     * @brief Default constructor
     */
    StateTrieNode() : is_leaf(false), timestamp(0) {}
    
    /**
     * @brief Constructor with key and value
     * @param k Node key
     * @param v Node value
     */
    StateTrieNode(const std::string& k, const std::string& v)
        : key(k), value(v), is_leaf(true), timestamp(0) {
        updateHash();
    }
    
    /**
     * @brief Update node hash
     */
    void updateHash();
    
    /**
     * @brief Serialize to JSON
     * @return JSON representation
     */
    nlohmann::json toJson() const;
    
    /**
     * @brief Deserialize from JSON
     * @param json JSON representation
     * @return True if successful
     */
    bool fromJson(const nlohmann::json& json);
};

/**
 * @brief State trie for managing blockchain state
 * 
 * The state trie stores all account balances, contract storage,
 * and other blockchain state in a tree structure for efficient
 * access and integrity verification.
 */
class StateTrie {
public:
    /**
     * @brief Default constructor
     */
    StateTrie();
    
    /**
     * @brief Destructor
     */
    ~StateTrie() = default;
    
    // Disable copy and move semantics
    StateTrie(const StateTrie&) = delete;
    StateTrie& operator=(const StateTrie&) = delete;
    StateTrie(StateTrie&&) = delete;
    StateTrie& operator=(StateTrie&&) = delete;
    
    /**
     * @brief Initialize state trie
     * @param db_path Path to persistent storage
     * @return True if successful
     */
    bool initialize(const std::string& db_path);
    
    /**
     * @brief Shutdown state trie
     */
    void shutdown();
    
    /**
     * @brief Get value for key
     * @param key Key to lookup
     * @return Value or empty string if not found
     */
    std::string getValue(const std::string& key) const;
    
    /**
     * @brief Set value for key
     * @param key Key to set
     * @param value Value to set
     * @return True if successful
     */
    bool setValue(const std::string& key, const std::string& value);
    
    /**
     * @brief Delete key
     * @param key Key to delete
     * @return True if successful
     */
    bool deleteValue(const std::string& key);
    
    /**
     * @brief Check if key exists
     * @param key Key to check
     * @return True if key exists
     */
    bool hasKey(const std::string& key) const;
    
    /**
     * @brief Get account balance
     * @param address Account address
     * @return Balance in wei
     */
    uint64_t getBalance(const std::string& address) const;
    
    /**
     * @brief Set account balance
     * @param address Account address
     * @param balance Balance in wei
     * @return True if successful
     */
    bool setBalance(const std::string& address, uint64_t balance);
    
    /**
     * @brief Get account nonce
     * @param address Account address
     * @return Nonce value
     */
    uint64_t getNonce(const std::string& address) const;
    
    /**
     * @brief Set account nonce
     * @param address Account address
     * @param nonce Nonce value
     * @return True if successful
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
     * @param contract_address Contract address
     * @param storage_key Storage key
     * @return Storage value
     */
    std::string getStorageValue(const std::string& contract_address, const std::string& storage_key) const;
    
    /**
     * @brief Set contract storage value
     * @param contract_address Contract address
     * @param storage_key Storage key
     * @param value Storage value
     * @return True if successful
     */
    bool setStorageValue(const std::string& contract_address, const std::string& storage_key, const std::string& value);
    
    /**
     * @brief Get contract code
     * @param contract_address Contract address
     * @return Contract bytecode
     */
    std::vector<uint8_t> getContractCode(const std::string& contract_address) const;
    
    /**
     * @brief Set contract code
     * @param contract_address Contract address
     * @param code Contract bytecode
     * @return True if successful
     */
    bool setContractCode(const std::string& contract_address, const std::vector<uint8_t>& code);
    
    /**
     * @brief Get state root hash
     * @return Root hash of the state trie
     */
    std::string getStateRoot() const;
    
    /**
     * @brief Create state snapshot
     * @return Snapshot ID
     */
    std::string createSnapshot();
    
    /**
     * @brief Restore state from snapshot
     * @param snapshot_id Snapshot ID
     * @return True if successful
     */
    bool restoreSnapshot(const std::string& snapshot_id);
    
    /**
     * @brief Delete snapshot
     * @param snapshot_id Snapshot ID
     * @return True if successful
     */
    bool deleteSnapshot(const std::string& snapshot_id);
    
    /**
     * @brief Get all account addresses
     * @return Vector of account addresses
     */
    std::vector<std::string> getAllAccounts() const;
    
    /**
     * @brief Get all contract addresses
     * @return Vector of contract addresses
     */
    std::vector<std::string> getAllContracts() const;
    
    /**
     * @brief Get state statistics
     * @return JSON with statistics
     */
    nlohmann::json getStatistics() const;
    
    /**
     * @brief Serialize state trie to JSON
     * @return JSON representation
     */
    nlohmann::json toJson() const;
    
    /**
     * @brief Deserialize state trie from JSON
     * @param json JSON representation
     * @return True if successful
     */
    bool fromJson(const nlohmann::json& json);
    
    /**
     * @brief Validate state trie integrity
     * @return True if valid
     */
    bool validate() const;

private:
    std::shared_ptr<StateTrieNode> root_;           ///< Root node of the trie
    std::string db_path_;                           ///< Path to persistent storage
    mutable std::mutex mutex_;                      ///< Mutex for thread safety
    std::unordered_map<std::string, std::shared_ptr<StateTrieNode>> snapshots_; ///< State snapshots
    
    /**
     * @brief Get node for key path
     * @param key Key to lookup
     * @return Node pointer or nullptr if not found
     */
    std::shared_ptr<StateTrieNode> getNode(const std::string& key) const;
    
    /**
     * @brief Set node for key path
     * @param key Key to set
     * @param value Value to set
     * @return True if successful
     */
    bool setNode(const std::string& key, const std::string& value);
    
    /**
     * @brief Delete node for key path
     * @param key Key to delete
     * @return True if successful
     */
    bool deleteNode(const std::string& key);
    
    /**
     * @brief Load state from persistent storage
     * @return True if successful
     */
    bool loadFromStorage();
    
    /**
     * @brief Save state to persistent storage
     * @return True if successful
     */
    bool saveToStorage() const;
    
    /**
     * @brief Update all hashes in the trie
     */
    void updateHashes();
    
    /**
     * @brief Get key path for account
     * @param address Account address
     * @return Key path
     */
    std::string getAccountKey(const std::string& address) const;
    
    /**
     * @brief Get key path for contract storage
     * @param contract_address Contract address
     * @param storage_key Storage key
     * @return Key path
     */
    std::string getStorageKey(const std::string& contract_address, const std::string& storage_key) const;
    
    /**
     * @brief Get key path for contract code
     * @param contract_address Contract address
     * @return Key path
     */
    std::string getCodeKey(const std::string& contract_address) const;
};

} // namespace core
} // namespace deo
