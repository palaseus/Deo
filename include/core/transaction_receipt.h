/**
 * @file transaction_receipt.h
 * @brief Transaction receipt structure for blockchain state tracking
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace deo {
namespace core {

/**
 * @brief Transaction receipt containing execution results
 * 
 * A receipt is generated after a transaction is executed and contains
 * information about the execution result, gas usage, and state changes.
 */
struct TransactionReceipt {
    std::string transaction_hash;     ///< Hash of the transaction
    std::string block_hash;          ///< Hash of the block containing this transaction
    uint64_t block_number;           ///< Block number
    uint32_t transaction_index;      ///< Index of transaction in block
    std::string from_address;        ///< Address that sent the transaction
    std::string to_address;          ///< Address that received the transaction (empty for contract creation)
    uint64_t gas_used;               ///< Gas consumed by transaction
    uint64_t gas_price;              ///< Gas price paid
    uint64_t cumulative_gas_used;    ///< Cumulative gas used in block up to this transaction
    bool success;                    ///< Whether transaction was successful
    std::string error_message;       ///< Error message if transaction failed
    std::vector<uint8_t> return_data; ///< Return data from contract execution
    std::string contract_address;    ///< Address of created contract (if applicable)
    std::vector<std::string> logs;   ///< Event logs emitted during execution
    uint64_t timestamp;              ///< Timestamp when transaction was processed
    
    /**
     * @brief Default constructor
     */
    TransactionReceipt() = default;
    
    /**
     * @brief Constructor with basic parameters
     * @param tx_hash Transaction hash
     * @param blk_hash Block hash
     * @param blk_num Block number
     * @param tx_idx Transaction index
     * @param from From address
     * @param to To address
     * @param gas_used Gas used
     * @param gas_price Gas price
     * @param success Success status
     */
    TransactionReceipt(const std::string& tx_hash,
                      const std::string& blk_hash,
                      uint64_t blk_num,
                      uint32_t tx_idx,
                      const std::string& from,
                      const std::string& to,
                      uint64_t gas_used,
                      uint64_t gas_price,
                      bool success)
        : transaction_hash(tx_hash)
        , block_hash(blk_hash)
        , block_number(blk_num)
        , transaction_index(tx_idx)
        , from_address(from)
        , to_address(to)
        , gas_used(gas_used)
        , gas_price(gas_price)
        , cumulative_gas_used(0)
        , success(success)
        , timestamp(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) {}
    
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
    
    /**
     * @brief Get transaction fee
     * @return Fee in wei (gas_used * gas_price)
     */
    uint64_t getFee() const { return gas_used * gas_price; }
    
    /**
     * @brief Validate receipt structure
     * @return True if valid
     */
    bool validate() const;
    
    /**
     * @brief Get receipt hash for indexing
     * @return SHA-256 hash of receipt
     */
    std::string getHash() const;
};

/**
 * @brief Block receipt containing all transaction receipts for a block
 */
struct BlockReceipt {
    std::string block_hash;                              ///< Block hash
    uint64_t block_number;                               ///< Block number
    uint64_t total_gas_used;                             ///< Total gas used in block
    uint32_t transaction_count;                          ///< Number of transactions
    std::vector<TransactionReceipt> receipts;            ///< Transaction receipts
    std::string state_root;                              ///< State root after block execution
    uint64_t timestamp;                                  ///< Block timestamp
    
    /**
     * @brief Default constructor
     */
    BlockReceipt() = default;
    
    /**
     * @brief Constructor with parameters
     * @param blk_hash Block hash
     * @param blk_num Block number
     * @param tx_count Transaction count
     */
    BlockReceipt(const std::string& blk_hash, uint64_t blk_num, uint32_t tx_count)
        : block_hash(blk_hash)
        , block_number(blk_num)
        , total_gas_used(0)
        , transaction_count(tx_count)
        , timestamp(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) {}
    
    /**
     * @brief Add transaction receipt
     * @param receipt Transaction receipt to add
     */
    void addReceipt(const TransactionReceipt& receipt);
    
    /**
     * @brief Get receipt by transaction hash
     * @param tx_hash Transaction hash
     * @return Receipt pointer or nullptr if not found
     */
    const TransactionReceipt* getReceipt(const std::string& tx_hash) const;
    
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
    
    /**
     * @brief Validate block receipt
     * @return True if valid
     */
    bool validate() const;
};

} // namespace core
} // namespace deo
