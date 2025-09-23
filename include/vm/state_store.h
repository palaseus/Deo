/**
 * @file state_store.h
 * @brief Persistent state store for blockchain state management
 * @author Deo Blockchain Team
 * @date 2024
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "vm/uint256.h"

namespace deo {
namespace vm {

/**
 * @brief Contract state information
 */
struct ContractState {
    std::vector<uint8_t> bytecode;           ///< Contract bytecode
    std::map<uint256_t, uint256_t> storage;  ///< Contract storage (key-value pairs)
    uint256_t balance;                       ///< Contract balance
    uint64_t nonce;                          ///< Contract nonce
    bool is_deployed;                        ///< Whether contract is deployed
    uint64_t deployment_block;               ///< Block number when deployed
    std::string deployer_address;            ///< Address of the deployer
};

/**
 * @brief Account state information
 */
struct AccountState {
    uint256_t balance;                       ///< Account balance
    uint64_t nonce;                          ///< Account nonce
    bool is_contract;                        ///< Whether this is a contract account
};

/**
 * @brief State store interface for persistent blockchain state
 */
class StateStore {
public:
    /**
     * @brief Constructor
     * @param db_path Path to the database directory
     */
    explicit StateStore(const std::string& db_path);
    
    /**
     * @brief Destructor
     */
    ~StateStore();
    
    /**
     * @brief Initialize the state store
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the state store
     */
    void shutdown();
    
    // Account management
    /**
     * @brief Get account state
     * @param address Account address
     * @return Account state or nullptr if not found
     */
    std::shared_ptr<AccountState> getAccountState(const std::string& address);
    
    /**
     * @brief Set account state
     * @param address Account address
     * @param state Account state
     * @return True if successful
     */
    bool setAccountState(const std::string& address, const AccountState& state);
    
    /**
     * @brief Get account balance
     * @param address Account address
     * @return Account balance (0 if not found)
     */
    uint256_t getBalance(const std::string& address);
    
    /**
     * @brief Set account balance
     * @param address Account address
     * @param balance New balance
     * @return True if successful
     */
    bool setBalance(const std::string& address, const uint256_t& balance);
    
    /**
     * @brief Get account nonce
     * @param address Account address
     * @return Account nonce (0 if not found)
     */
    uint64_t getNonce(const std::string& address);
    
    /**
     * @brief Set account nonce
     * @param address Account address
     * @param nonce New nonce
     * @return True if successful
     */
    bool setNonce(const std::string& address, uint64_t nonce);
    
    /**
     * @brief Increment account nonce
     * @param address Account address
     * @return New nonce value
     */
    uint64_t incrementNonce(const std::string& address);
    
    // Contract management
    /**
     * @brief Get contract state
     * @param address Contract address
     * @return Contract state or nullptr if not found
     */
    std::shared_ptr<ContractState> getContractState(const std::string& address);
    
    /**
     * @brief Set contract state
     * @param address Contract address
     * @param state Contract state
     * @return True if successful
     */
    bool setContractState(const std::string& address, const ContractState& state);
    
    /**
     * @brief Deploy contract
     * @param address Contract address
     * @param bytecode Contract bytecode
     * @param deployer_address Deployer address
     * @param deployment_block Block number
     * @return True if successful
     */
    bool deployContract(const std::string& address, 
                       const std::vector<uint8_t>& bytecode,
                       const std::string& deployer_address,
                       uint64_t deployment_block);
    
    /**
     * @brief Check if contract exists
     * @param address Contract address
     * @return True if contract exists
     */
    bool contractExists(const std::string& address);
    
    // Storage management
    /**
     * @brief Get contract storage value
     * @param contract_address Contract address
     * @param key Storage key
     * @return Storage value (0 if not found)
     */
    uint256_t getStorageValue(const std::string& contract_address, const uint256_t& key);
    
    /**
     * @brief Set contract storage value
     * @param contract_address Contract address
     * @param key Storage key
     * @param value Storage value
     * @return True if successful
     */
    bool setStorageValue(const std::string& contract_address, 
                        const uint256_t& key, 
                        const uint256_t& value);
    
    // Transaction management
    /**
     * @brief Begin transaction (for atomic operations)
     */
    void beginTransaction();
    
    /**
     * @brief Commit transaction
     * @return True if successful
     */
    bool commitTransaction();
    
    /**
     * @brief Rollback transaction
     */
    void rollbackTransaction();
    
    // Statistics
    /**
     * @brief Get total number of accounts
     * @return Number of accounts
     */
    uint64_t getAccountCount() const;
    
    /**
     * @brief Get total number of contracts
     * @return Number of contracts
     */
    uint64_t getContractCount() const;
    
    /**
     * @brief Get total storage entries
     * @return Number of storage entries
     */
    uint64_t getStorageEntryCount() const;

private:
    std::string db_path_;                    ///< Database path
    void* db_;                               ///< Database handle (opaque pointer)
    std::mutex mutex_;                       ///< Thread safety mutex
    bool initialized_;                       ///< Initialization flag
    
    // Transaction state
    bool in_transaction_;                    ///< Whether we're in a transaction
    std::map<std::string, std::string> transaction_cache_; ///< Transaction cache
    
    /**
     * @brief Get value from database
     * @param key Database key
     * @return Value or empty string if not found
     */
    std::string getValue(const std::string& key);
    
    /**
     * @brief Set value in database
     * @param key Database key
     * @param value Value to set
     * @return True if successful
     */
    bool setValue(const std::string& key, const std::string& value);
    
    /**
     * @brief Delete value from database
     * @param key Database key
     * @return True if successful
     */
    bool deleteValue(const std::string& key);
    
    /**
     * @brief Generate account key
     * @param address Account address
     * @return Database key
     */
    std::string getAccountKey(const std::string& address);
    
    /**
     * @brief Generate contract key
     * @param address Contract address
     * @return Database key
     */
    std::string getContractKey(const std::string& address);
    
    /**
     * @brief Generate storage key
     * @param contract_address Contract address
     * @param storage_key Storage key
     * @return Database key
     */
    std::string getStorageKey(const std::string& contract_address, const uint256_t& storage_key);
    
    /**
     * @brief Serialize account state to string
     * @param state Account state
     * @return Serialized string
     */
    std::string serializeAccountState(const AccountState& state);
    
    /**
     * @brief Deserialize account state from string
     * @param data Serialized data
     * @return Account state
     */
    AccountState deserializeAccountState(const std::string& data);
    
    /**
     * @brief Serialize contract state to string
     * @param state Contract state
     * @return Serialized string
     */
    std::string serializeContractState(const ContractState& state);
    
    /**
     * @brief Deserialize contract state from string
     * @param data Serialized data
     * @return Contract state
     */
    ContractState deserializeContractState(const std::string& data);
};

} // namespace vm
} // namespace deo
