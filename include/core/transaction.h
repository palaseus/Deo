/**
 * @file transaction.h
 * @brief Transaction data structure and operations for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "crypto/signature.h"

namespace deo {
namespace core {

/**
 * @brief Transaction input structure
 * 
 * Represents an input to a transaction, referencing a previous output
 * and providing a signature to prove ownership.
 */
struct TransactionInput {
    std::string previous_tx_hash;    ///< Hash of the previous transaction
    uint32_t output_index;           ///< Index of the output in the previous transaction
    std::string signature;           ///< Digital signature proving ownership
    std::string public_key;          ///< Public key of the input owner
    uint64_t sequence;               ///< Sequence number for transaction replacement
    
    /**
     * @brief Default constructor
     */
    TransactionInput() = default;
    
    /**
     * @brief Constructor with parameters
     * @param prev_tx_hash Hash of the previous transaction
     * @param out_index Index of the output
     * @param sig Digital signature
     * @param pub_key Public key
     * @param seq Sequence number
     */
    TransactionInput(const std::string& prev_tx_hash, 
                    uint32_t out_index,
                    const std::string& sig,
                    const std::string& pub_key,
                    uint64_t seq = 0xFFFFFFFF)
        : previous_tx_hash(prev_tx_hash)
        , output_index(out_index)
        , signature(sig)
        , public_key(pub_key)
        , sequence(seq) {}
    
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
     * @brief Get serialized data for signing
     * @return Serialized data string
     */
    std::string getSerializedData() const;
};

/**
 * @brief Transaction output structure
 * 
 * Represents an output from a transaction, specifying the recipient
 * and the amount to be transferred.
 */
struct TransactionOutput {
    uint64_t value;                  ///< Amount in satoshis
    std::string recipient_address;   ///< Recipient's address
    std::string script_pubkey;       ///< Script for spending this output
    uint32_t output_index;           ///< Index of this output in the transaction
    
    /**
     * @brief Default constructor
     */
    TransactionOutput() = default;
    
    /**
     * @brief Constructor with parameters
     * @param val Amount in satoshis
     * @param recipient Recipient's address
     * @param script Script for spending
     * @param index Output index
     */
    TransactionOutput(uint64_t val, 
                     const std::string& recipient,
                     const std::string& script = "",
                     uint32_t index = 0)
        : value(val)
        , recipient_address(recipient)
        , script_pubkey(script)
        , output_index(index) {}
    
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
     * @brief Get serialized data for hashing
     * @return Serialized data string
     */
    std::string getSerializedData() const;
};

/**
 * @brief Transaction class for the Deo Blockchain
 * 
 * A transaction represents a transfer of value from one or more inputs
 * to one or more outputs. Each transaction is cryptographically signed
 * and can be verified for authenticity and validity.
 */
class Transaction {
public:
    /**
     * @brief Transaction types
     */
    enum class Type {
        REGULAR = 0,        ///< Regular value transfer
        COINBASE = 1,       ///< Coinbase transaction (mining reward)
        CONTRACT = 2        ///< Smart contract transaction
    };
    
    /**
     * @brief Default constructor
     */
    Transaction();
    
    /**
     * @brief Constructor with parameters
     * @param inputs Transaction inputs
     * @param outputs Transaction outputs
     * @param type Transaction type
     */
    Transaction(const std::vector<TransactionInput>& inputs,
               const std::vector<TransactionOutput>& outputs,
               Type type = Type::REGULAR);
    
    /**
     * @brief Destructor
     */
    ~Transaction() = default;
    
    // Disable copy and move semantics
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction&&) = delete;
    
    /**
     * @brief Get transaction ID (hash)
     * @return Transaction hash
     */
    std::string getId() const;
    
    /**
     * @brief Calculate transaction hash
     * @return SHA-256 hash of the transaction
     */
    std::string calculateHash() const;
    
    /**
     * @brief Add input to transaction
     * @param input Transaction input
     * @return True if successful
     */
    bool addInput(const TransactionInput& input);
    
    /**
     * @brief Add output to transaction
     * @param output Transaction output
     * @return True if successful
     */
    bool addOutput(const TransactionOutput& output);
    
    /**
     * @brief Sign the transaction
     * @param private_key Private key for signing
     * @return True if successful
     */
    bool sign(const std::string& private_key);
    
    /**
     * @brief Verify transaction signatures
     * @return True if all signatures are valid
     */
    bool verify() const;
    
    /**
     * @brief Verify transaction structure
     * @return True if transaction structure is valid
     */
    bool validate() const;
    
    /**
     * @brief Serialize transaction to JSON
     * @return JSON representation
     */
    nlohmann::json toJson() const;
    
    /**
     * @brief Deserialize transaction from JSON
     * @param json JSON representation
     * @return True if successful
     */
    bool fromJson(const nlohmann::json& json);
    
    /**
     * @brief Serialize transaction to binary format
     * @return Binary data
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief Deserialize transaction from binary format
     * @param data Binary data
     * @return True if successful
     */
    bool deserialize(const std::vector<uint8_t>& data);
    
    /**
     * @brief Get transaction size in bytes
     * @return Size in bytes
     */
    size_t getSize() const;
    
    /**
     * @brief Get transaction fee
     * @return Fee in satoshis
     */
    uint64_t getFee() const;
    
    /**
     * @brief Check if transaction is coinbase
     * @return True if coinbase transaction
     */
    bool isCoinbase() const;
    
    /**
     * @brief Get inputs
     * @return Reference to inputs vector
     */
    const std::vector<TransactionInput>& getInputs() const { return inputs_; }
    
    /**
     * @brief Get outputs
     * @return Reference to outputs vector
     */
    const std::vector<TransactionOutput>& getOutputs() const { return outputs_; }
    
    /**
     * @brief Get transaction type
     * @return Transaction type
     */
    Type getType() const { return type_; }
    
    /**
     * @brief Get transaction timestamp
     * @return Timestamp
     */
    std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }
    
    /**
     * @brief Get transaction version
     * @return Version number
     */
    uint32_t getVersion() const { return version_; }
    
    /**
     * @brief Set transaction version
     * @param version Version number
     */
    void setVersion(uint32_t version) { version_ = version; }
    
    /**
     * @brief Get lock time
     * @return Lock time
     */
    uint32_t getLockTime() const { return lock_time_; }
    
    /**
     * @brief Set lock time
     * @param lock_time Lock time
     */
    void setLockTime(uint32_t lock_time) { lock_time_ = lock_time; }

private:
    uint32_t version_;                                    ///< Transaction version
    std::vector<TransactionInput> inputs_;               ///< Transaction inputs
    std::vector<TransactionOutput> outputs_;             ///< Transaction outputs
    uint32_t lock_time_;                                 ///< Lock time
    Type type_;                                          ///< Transaction type
    std::chrono::system_clock::time_point timestamp_;    ///< Transaction timestamp
    mutable std::string cached_hash_;                    ///< Cached transaction hash
    mutable bool hash_cached_;                           ///< Whether hash is cached
    
    /**
     * @brief Get serialized data for hashing (deterministic order)
     * @return Serialized data string
     */
    std::string getSerializedData() const;
    
    /**
     * @brief Update cached hash
     */
    void updateHash() const;
    
    /**
     * @brief Validate input
     * @param input Input to validate
     * @return True if valid
     */
    bool validateInput(const TransactionInput& input) const;
    
    /**
     * @brief Validate output
     * @param output Output to validate
     * @return True if valid
     */
    bool validateOutput(const TransactionOutput& output) const;
};

} // namespace core
} // namespace deo