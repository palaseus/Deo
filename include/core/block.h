/**
 * @file block.h
 * @brief Block data structure and operations for the Deo Blockchain
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

#include "transaction.h"
#include "merkle_tree.h"

namespace deo {
namespace core {

/**
 * @brief Block header structure
 * 
 * Contains all the metadata for a block, including cryptographic
 * links to the previous block and the Merkle root of transactions.
 */
struct BlockHeader {
    uint32_t version;           ///< Block version number
    std::string previous_hash;  ///< Hash of the previous block (32 bytes)
    std::string merkle_root;    ///< Merkle root of transactions (32 bytes)
    uint64_t timestamp;         ///< Unix timestamp when block was created
    uint32_t nonce;            ///< Nonce for proof-of-work
    uint32_t difficulty;       ///< Mining difficulty target
    uint64_t height;           ///< Block height in the chain
    uint32_t transaction_count; ///< Number of transactions in the block
    
    /**
     * @brief Default constructor
     */
    BlockHeader() = default;
    
    /**
     * @brief Constructor with parameters
     * @param ver Block version
     * @param prev_hash Previous block hash
     * @param merkle_root Merkle root of transactions
     * @param ts Timestamp
     * @param n Nonce
     * @param diff Difficulty
     * @param h Height
     * @param tx_count Transaction count
     */
    BlockHeader(uint32_t ver, 
               const std::string& prev_hash,
               const std::string& merkle_root,
               uint64_t ts,
               uint32_t n,
               uint32_t diff,
               uint64_t h,
               uint32_t tx_count = 0)
        : version(ver)
        , previous_hash(prev_hash)
        , merkle_root(merkle_root)
        , timestamp(ts)
        , nonce(n)
        , difficulty(diff)
        , height(h)
        , transaction_count(tx_count) {}
    
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
     * @brief Get serialized data for hashing (deterministic order)
     * @return Serialized data string
     */
    std::string getSerializedData() const;
    
    /**
     * @brief Validate header structure
     * @return True if valid
     */
    bool validate() const;
};

/**
 * @brief Represents a block in the blockchain
 * 
 * A block contains a header with metadata and a body with transactions.
 * Each block is cryptographically linked to the previous block through
 * the previous block hash, forming an immutable chain.
 */
class Block {
public:
    /**
     * @brief Default constructor
     */
    Block();
    
    /**
     * @brief Constructor with header and transactions
     * @param header Block header
     * @param transactions List of transactions
     */
    Block(const BlockHeader& header, std::vector<std::shared_ptr<Transaction>> transactions);
    
    /**
     * @brief Destructor
     */
    ~Block() = default;
    
    // Disable copy and move semantics
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    Block(Block&&) = delete;
    Block& operator=(Block&&) = delete;
    
    /**
     * @brief Get block hash
     * @return SHA-256 hash of the block header
     */
    std::string getHash() const;
    
    /**
     * @brief Calculate block hash
     * @return SHA-256 hash of the block header
     */
    std::string calculateHash() const;
    
    /**
     * @brief Get block header
     * @return Reference to block header
     */
    const BlockHeader& getHeader() const { return header_; }
    
    /**
     * @brief Set block header
     * @param header New block header
     */
    void setHeader(const BlockHeader& header);
    
    /**
     * @brief Get transactions
     * @return Reference to transactions vector
     */
    const std::vector<std::shared_ptr<Transaction>>& getTransactions() const { return transactions_; }
    
    /**
     * @brief Add transaction to block
     * @param transaction Transaction to add
     * @return True if successful
     */
    bool addTransaction(std::shared_ptr<Transaction> transaction);
    
    /**
     * @brief Remove transaction from block
     * @param transaction_id Transaction ID to remove
     * @return True if successful
     */
    bool removeTransaction(const std::string& transaction_id);
    
    /**
     * @brief Get transaction by ID
     * @param transaction_id Transaction ID
     * @return Transaction pointer or nullptr if not found
     */
    std::shared_ptr<Transaction> getTransaction(const std::string& transaction_id) const;
    
    /**
     * @brief Calculate Merkle root
     * @return Merkle root of all transactions
     */
    std::string calculateMerkleRoot() const;
    
    /**
     * @brief Update Merkle root in header
     */
    void updateMerkleRoot();
    
    /**
     * @brief Verify block integrity
     * @return True if block is valid
     */
    bool verify() const;
    
    /**
     * @brief Validate block structure
     * @return True if block structure is valid
     */
    bool validate() const;
    
    /**
     * @brief Serialize block to JSON
     * @return JSON representation
     */
    nlohmann::json toJson() const;
    
    /**
     * @brief Deserialize block from JSON
     * @param json JSON representation
     * @return True if successful
     */
    bool fromJson(const nlohmann::json& json);
    
    /**
     * @brief Serialize block to binary format
     * @return Binary data
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief Deserialize block from binary format
     * @param data Binary data
     * @return True if successful
     */
    bool deserialize(const std::vector<uint8_t>& data);
    
    /**
     * @brief Get block size in bytes
     * @return Size in bytes
     */
    size_t getSize() const;
    
    /**
     * @brief Get block weight (for fee calculation)
     * @return Block weight
     */
    uint64_t getWeight() const;
    
    /**
     * @brief Check if block is genesis block
     * @return True if genesis block
     */
    bool isGenesis() const;
    
    /**
     * @brief Get block timestamp
     * @return Timestamp
     */
    std::chrono::system_clock::time_point getTimestamp() const;
    
    /**
     * @brief Set block timestamp
     * @param timestamp New timestamp
     */
    void setTimestamp(const std::chrono::system_clock::time_point& timestamp);
    
    /**
     * @brief Get block version
     * @return Version number
     */
    uint32_t getVersion() const { return header_.version; }
    
    /**
     * @brief Set block version
     * @param version Version number
     */
    void setVersion(uint32_t version) { header_.version = version; }
    
    /**
     * @brief Get previous block hash
     * @return Previous block hash
     */
    const std::string& getPreviousHash() const { return header_.previous_hash; }
    
    /**
     * @brief Set previous block hash
     * @param hash Previous block hash
     */
    void setPreviousHash(const std::string& hash) { header_.previous_hash = hash; }
    
    /**
     * @brief Get block height
     * @return Block height
     */
    uint64_t getHeight() const { return header_.height; }
    
    /**
     * @brief Set block height
     * @param height Block height
     */
    void setHeight(uint64_t height) { header_.height = height; }
    
    /**
     * @brief Get block difficulty
     * @return Difficulty
     */
    uint32_t getDifficulty() const { return header_.difficulty; }
    
    /**
     * @brief Set block difficulty
     * @param difficulty Difficulty
     */
    void setDifficulty(uint32_t difficulty) { header_.difficulty = difficulty; }
    
    /**
     * @brief Get block nonce
     * @return Nonce
     */
    uint32_t getNonce() const { return header_.nonce; }
    
    /**
     * @brief Set block nonce
     * @param nonce Nonce
     */
    void setNonce(uint32_t nonce) { header_.nonce = nonce; }
    
    /**
     * @brief Get transaction count
     * @return Number of transactions
     */
    size_t getTransactionCount() const { return transactions_.size(); }
    
    /**
     * @brief Check if block meets difficulty target
     * @param target_hash Target hash for difficulty
     * @return True if block meets target
     */
    bool meetsDifficultyTarget(const std::string& target_hash) const;

private:
    BlockHeader header_;                                    ///< Block header
    std::vector<std::shared_ptr<Transaction>> transactions_; ///< Block transactions
    std::unique_ptr<MerkleTree> merkle_tree_;              ///< Merkle tree for transactions
    mutable std::string cached_hash_;                      ///< Cached block hash
    mutable bool hash_cached_;                             ///< Whether hash is cached
    
    /**
     * @brief Update cached hash
     */
    void updateHash() const;
    
    /**
     * @brief Validate header
     * @return True if valid
     */
    bool validateHeader() const;
    
    /**
     * @brief Validate transactions
     * @return True if all transactions are valid
     */
    bool validateTransactions() const;
    
    /**
     * @brief Rebuild Merkle tree
     */
    void rebuildMerkleTree();
};

} // namespace core
} // namespace deo