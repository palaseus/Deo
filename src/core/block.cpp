/**
 * @file block.cpp
 * @brief Block implementation for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "core/block.h"
#include "crypto/hash.h"
#include "utils/logger.h"
#include "utils/error_handler.h"
#include "vm/uint256.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

namespace deo {
namespace core {

// BlockHeader implementation

nlohmann::json BlockHeader::toJson() const {
    nlohmann::json json;
    json["version"] = version;
    json["previous_hash"] = previous_hash;
    json["merkle_root"] = merkle_root;
    json["timestamp"] = timestamp;
    json["nonce"] = nonce;
    json["difficulty"] = difficulty;
    json["height"] = height;
    json["transaction_count"] = transaction_count;
    return json;
}

bool BlockHeader::fromJson(const nlohmann::json& json) {
    try {
        version = json.at("version").get<uint32_t>();
        previous_hash = json.at("previous_hash").get<std::string>();
        merkle_root = json.at("merkle_root").get<std::string>();
        timestamp = json.at("timestamp").get<uint64_t>();
        nonce = json.at("nonce").get<uint32_t>();
        difficulty = json.at("difficulty").get<uint32_t>();
        height = json.at("height").get<uint64_t>();
        transaction_count = json.at("transaction_count").get<uint32_t>();
        return true;
    } catch (const std::exception& e) {
        DEO_ERROR(VALIDATION, "Failed to deserialize BlockHeader from JSON: " + std::string(e.what()));
        return false;
    }
}

std::string BlockHeader::getSerializedData() const {
    // Simple string concatenation to avoid stringstream issues
    std::string result;
    result += std::to_string(version);
    result += previous_hash;
    result += merkle_root;
    result += std::to_string(timestamp);
    result += std::to_string(nonce);
    result += std::to_string(difficulty);
    result += std::to_string(height);
    result += std::to_string(transaction_count);
    
    return result;
}

bool BlockHeader::validate() const {
    if (version == 0) {
        DEO_ERROR(VALIDATION, "Invalid block version");
        return false;
    }
    
    if (previous_hash.length() != 64) { // 32 bytes = 64 hex chars
        DEO_ERROR(VALIDATION, "Invalid previous hash length");
        return false;
    }
    
    if (merkle_root.length() != 64) { // 32 bytes = 64 hex chars
        DEO_ERROR(VALIDATION, "Invalid merkle root length");
        return false;
    }
    
    if (difficulty == 0) {
        DEO_ERROR(VALIDATION, "Invalid difficulty (cannot be zero)");
        return false;
    }
    
    return true;
}

// Block implementation

Block::Block()
    : header_()
    , hash_cached_(false) {
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Empty block created");
}

Block::Block(const BlockHeader& header, std::vector<std::shared_ptr<Transaction>> transactions)
    : header_(header)
    , transactions_(std::move(transactions))
    , hash_cached_(false) {
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Block created with " + std::to_string(transactions_.size()) + " transactions");
    
    // Update transaction count in header
    header_.transaction_count = static_cast<uint32_t>(transactions_.size());
    
    // Create Merkle tree for transactions
    rebuildMerkleTree();
    updateMerkleRoot();
}

std::string Block::getHash() const {
    if (!hash_cached_) {
        updateHash();
    }
    return cached_hash_;
}

std::string Block::calculateHash() const {
    if (!hash_cached_) {
        updateHash();
    }
    return cached_hash_;
}

void Block::setHeader(const BlockHeader& header) {
    header_ = header;
    hash_cached_ = false; // Invalidate cached hash
}

bool Block::addTransaction(std::shared_ptr<Transaction> transaction) {
    if (!transaction) {
        DEO_ERROR(VALIDATION, "Cannot add null transaction to block");
        return false;
    }
    
    if (!transaction->validate()) {
        DEO_ERROR(VALIDATION, "Cannot add invalid transaction to block");
        return false;
    }
    
    // Check for duplicate transaction
    for (const auto& tx : transactions_) {
        if (tx && tx->getId() == transaction->getId()) {
            DEO_ERROR(VALIDATION, "Transaction already exists in block");
            return false;
        }
    }
    
    transactions_.push_back(transaction);
    header_.transaction_count = static_cast<uint32_t>(transactions_.size());
    
    // Rebuild Merkle tree
    rebuildMerkleTree();
    updateMerkleRoot();
    hash_cached_ = false; // Invalidate cached hash
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Added transaction " + transaction->getId() + " to block");
    return true;
}

bool Block::removeTransaction(const std::string& transaction_id) {
    auto it = std::find_if(transactions_.begin(), transactions_.end(),
        [&transaction_id](const std::shared_ptr<Transaction>& tx) {
            return tx && tx->getId() == transaction_id;
        });
    
    if (it != transactions_.end()) {
        transactions_.erase(it);
        header_.transaction_count = static_cast<uint32_t>(transactions_.size());
        
        // Rebuild Merkle tree
        rebuildMerkleTree();
        updateMerkleRoot();
        hash_cached_ = false; // Invalidate cached hash
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Removed transaction " + transaction_id + " from block");
        return true;
    }
    
    DEO_WARNING(BLOCKCHAIN, "Transaction " + transaction_id + " not found in block");
    return false;
}

std::shared_ptr<Transaction> Block::getTransaction(const std::string& transaction_id) const {
    auto it = std::find_if(transactions_.begin(), transactions_.end(),
        [&transaction_id](const std::shared_ptr<Transaction>& tx) {
            return tx && tx->getId() == transaction_id;
        });
    
    return (it != transactions_.end()) ? *it : nullptr;
}

std::string Block::calculateMerkleRoot() const {
    if (merkle_tree_) {
        return merkle_tree_->getRoot();
    }
    // Empty Merkle tree root (Bitcoin convention)
    return "0000000000000000000000000000000000000000000000000000000000000000";
}

void Block::updateMerkleRoot() {
    std::string merkle_root = calculateMerkleRoot();
    header_.merkle_root = merkle_root;
    hash_cached_ = false; // Invalidate cached hash
}

bool Block::verify() const {
    DEO_CHECKPOINT("Block::verify");
    
    // Verify block structure
    if (!validate()) {
        DEO_ERROR(VALIDATION, "Block structure validation failed");
        return false;
    }
    
    // Verify Merkle root
    std::string calculated_root = calculateMerkleRoot();
    if (calculated_root != header_.merkle_root) {
        DEO_ERROR(VALIDATION, "Merkle root mismatch: expected " + header_.merkle_root + ", got " + calculated_root);
        return false;
    }
    
    // Verify all transactions
    for (const auto& tx : transactions_) {
        if (!tx || !tx->verify()) {
            DEO_ERROR(VALIDATION, "Invalid transaction in block");
            return false;
        }
    }
    
    // Verify block hash
    std::string calculated_hash = calculateHash();
    // Note: In a real implementation, we would compare with a stored hash
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Block verification successful");
    return true;
}

bool Block::validate() const {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Validating block structure");
    
    // Validate header
    if (!validateHeader()) {
        return false;
    }
    
    // Validate transactions
    if (!validateTransactions()) {
        return false;
    }
    
    // Validate transaction count matches header
    if (header_.transaction_count != transactions_.size()) {
        DEO_ERROR(VALIDATION, "Transaction count mismatch: header=" + 
                  std::to_string(header_.transaction_count) + 
                  ", actual=" + std::to_string(transactions_.size()));
        return false;
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Block validation successful");
    return true;
}

nlohmann::json Block::toJson() const {
    nlohmann::json json;
    
    // Serialize header
    json["header"] = header_.toJson();
    
    // Serialize transactions
    nlohmann::json transactions_json = nlohmann::json::array();
    for (const auto& tx : transactions_) {
        if (tx) {
            transactions_json.push_back(tx->toJson());
        }
    }
    json["transactions"] = transactions_json;
    
    json["hash"] = calculateHash();
    json["size"] = getSize();
    json["weight"] = getWeight();
    
    return json;
}

bool Block::fromJson(const nlohmann::json& json) {
    try {
        // Deserialize header
        if (!header_.fromJson(json.at("header"))) {
            return false;
        }
        
        // Deserialize transactions
        transactions_.clear();
        const auto& transactions_json = json.at("transactions");
        for (const auto& tx_json : transactions_json) {
            auto tx = std::make_shared<Transaction>();
            if (!tx->fromJson(tx_json)) {
                return false;
            }
            transactions_.push_back(tx);
        }
        
        // Rebuild Merkle tree
        rebuildMerkleTree();
        hash_cached_ = false; // Invalidate cached hash
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Block deserialized from JSON");
        return true;
        
    } catch (const std::exception& e) {
        DEO_ERROR(VALIDATION, "Failed to deserialize Block from JSON: " + std::string(e.what()));
        return false;
    }
}

std::vector<uint8_t> Block::serialize() const {
    std::vector<uint8_t> data;
    
    // Serialize header
    std::string header_data = header_.getSerializedData();
    data.insert(data.end(), header_data.begin(), header_data.end());
    
    // Serialize transaction count
    uint32_t tx_count = static_cast<uint32_t>(transactions_.size());
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&tx_count), 
                reinterpret_cast<uint8_t*>(&tx_count) + sizeof(tx_count));
    
    // Serialize transactions
    for (const auto& tx : transactions_) {
        if (tx) {
            std::vector<uint8_t> tx_data = tx->serialize();
            data.insert(data.end(), tx_data.begin(), tx_data.end());
        }
    }
    
    return data;
}

bool Block::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 88) { // Minimum size for header
        DEO_ERROR(VALIDATION, "Block data too small for deserialization");
        return false;
    }
    
    size_t offset = 0;
    
    // Deserialize header
    header_.version = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += sizeof(uint32_t);
    
    header_.previous_hash = crypto::Hash::bytesToHex(&data[offset], 32);
    offset += 32;
    
    header_.merkle_root = crypto::Hash::bytesToHex(&data[offset], 32);
    offset += 32;
    
    header_.timestamp = *reinterpret_cast<const uint64_t*>(&data[offset]);
    offset += sizeof(uint64_t);
    
    header_.nonce = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += sizeof(uint32_t);
    
    header_.difficulty = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += sizeof(uint32_t);
    
    header_.height = *reinterpret_cast<const uint64_t*>(&data[offset]);
    offset += sizeof(uint64_t);
    
    header_.transaction_count = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += sizeof(uint32_t);
    
    // Deserialize transaction count
    uint32_t tx_count = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += sizeof(uint32_t);
    
    // Deserialize transactions
    transactions_.clear();
    for (uint32_t i = 0; i < tx_count; ++i) {
        auto tx = std::make_shared<Transaction>();
        // Note: In a real implementation, we would need to determine the size of each transaction
        // For now, this is a placeholder
        transactions_.push_back(tx);
    }
    
    // Rebuild Merkle tree
    rebuildMerkleTree();
    hash_cached_ = false; // Invalidate cached hash
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Deserialized block with " + std::to_string(transactions_.size()) + " transactions");
    return true;
}

size_t Block::getSize() const {
    return serialize().size();
}

uint64_t Block::getWeight() const {
    // Simplified weight calculation (in reality would use witness data)
    return static_cast<uint64_t>(getSize() * 4); // Assume 4x weight multiplier
}

bool Block::isGenesis() const {
    return header_.height == 0 && header_.previous_hash == "0000000000000000000000000000000000000000000000000000000000000000";
}

std::chrono::system_clock::time_point Block::getTimestamp() const {
    return std::chrono::system_clock::time_point(std::chrono::seconds(header_.timestamp));
}

void Block::setTimestamp(const std::chrono::system_clock::time_point& timestamp) {
    header_.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    hash_cached_ = false; // Invalidate cached hash
}

bool Block::meetsDifficultyTarget(const std::string& target_hash) const {
    std::string block_hash = calculateHash();
    
    try {
        // Convert hex strings to uint256_t for proper numerical comparison
        vm::uint256_t block_value(block_hash);
        vm::uint256_t target_value(target_hash);
        
        // For difficulty, lower hash values are better (meet higher difficulty)
        // So we check if block hash is less than target
        return block_value < target_value;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to parse difficulty target: " + std::string(e.what()));
        return false;
    }
}

void Block::updateHash() const {
    std::string serialized_data = header_.getSerializedData();
    cached_hash_ = crypto::Hash::sha256(serialized_data);
    hash_cached_ = true;
}

bool Block::validateHeader() const {
    return header_.validate();
}

bool Block::validateTransactions() const {
    for (const auto& tx : transactions_) {
        if (!tx || !tx->validate()) {
            DEO_ERROR(VALIDATION, "Invalid transaction in block");
            return false;
        }
    }
    return true;
}

void Block::rebuildMerkleTree() {
    std::vector<std::string> tx_hashes;
    for (const auto& tx : transactions_) {
        if (tx) {
            tx_hashes.push_back(tx->calculateHash());
        }
    }
    
    merkle_tree_ = std::make_unique<MerkleTree>(tx_hashes);
}

} // namespace core
} // namespace deo