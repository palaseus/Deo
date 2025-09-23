/**
 * @file leveldb_block_storage.cpp
 * @brief LevelDB-based block storage implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "storage/leveldb_storage.h"
#include "utils/logger.h"
#include "crypto/hash.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace deo {
namespace storage {

LevelDBBlockStorage::LevelDBBlockStorage(const std::string& data_directory)
    : data_directory_(data_directory) {
    initializeOptions();
}

LevelDBBlockStorage::~LevelDBBlockStorage() {
    shutdown();
}

bool LevelDBBlockStorage::initialize() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Check if already initialized
    if (db_) {
        return true;
    }
    
    if (!createDataDirectory()) {
        return false;
    }
    
    std::string db_path = data_directory_ + "/blocks";
    
    // Open the database
    leveldb::DB* db_ptr = nullptr;
    leveldb::Status status = leveldb::DB::Open(db_options_, db_path, &db_ptr);
    if (status.ok()) {
        db_.reset(db_ptr);
        return true;
    } else {
        return false;
    }
}

void LevelDBBlockStorage::shutdown() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (db_) {
        db_.reset();
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "LevelDB block storage shutdown");
}

bool LevelDBBlockStorage::storeBlock(const std::shared_ptr<core::Block>& block) {
    if (!block || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string block_hash = block->calculateHash();
        std::string block_key = createBlockKey(block_hash);
        std::string height_key = createHeightKey(block->getHeader().height);
        std::string serialized_block = serializeBlock(block);
        
        // Use write batch for atomic operations
        leveldb::WriteBatch batch;
        batch.Put(block_key, serialized_block);
        batch.Put(height_key, block_hash);
        
        // Update metadata
        batch.Put(LATEST_KEY, block_hash);
        batch.Put(COUNT_KEY, std::to_string(getBlockCount() + 1));
        batch.Put(HEIGHT_KEY, std::to_string(block->getHeader().height));
        
        // If this is the genesis block, mark it
        if (block->getHeader().height == 0) {
            batch.Put(GENESIS_KEY, block_hash);
        }
        
        leveldb::Status status = db_->Write(write_options_, &batch);
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to store block: " + status.ToString());
            return false;
        }
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Stored block at height " + std::to_string(block->getHeader().height) + 
                      " with hash: " + block_hash);
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while storing block: " + std::string(e.what()));
        return false;
    }
}

std::shared_ptr<core::Block> LevelDBBlockStorage::getBlock(const std::string& block_hash) {
    if (block_hash.empty() || !db_) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string block_key = createBlockKey(block_hash);
        std::string serialized_block;
        
        leveldb::Status status = db_->Get(read_options_, block_key, &serialized_block);
        if (!status.ok()) {
            if (!status.IsNotFound()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get block: " + status.ToString());
            }
            return nullptr;
        }
        
        return deserializeBlock(serialized_block);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting block: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<core::Block> LevelDBBlockStorage::getBlockByHeight(uint64_t height) {
    if (!db_) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string height_key = createHeightKey(height);
        std::string block_hash;
        
        leveldb::Status status = db_->Get(read_options_, height_key, &block_hash);
        if (!status.ok()) {
            if (!status.IsNotFound()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get block hash by height: " + status.ToString());
            }
            return nullptr;
        }
        
        return getBlock(block_hash);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting block by height: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<core::Block> LevelDBBlockStorage::getLatestBlock() {
    if (!db_) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string latest_hash;
        leveldb::Status status = db_->Get(read_options_, LATEST_KEY, &latest_hash);
        if (!status.ok()) {
            if (!status.IsNotFound()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get latest block hash: " + status.ToString());
            }
            return nullptr;
        }
        
        return getBlock(latest_hash);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting latest block: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<core::Block> LevelDBBlockStorage::getGenesisBlock() {
    if (!db_) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string genesis_hash;
        leveldb::Status status = db_->Get(read_options_, GENESIS_KEY, &genesis_hash);
        if (!status.ok()) {
            if (!status.IsNotFound()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get genesis block hash: " + status.ToString());
            }
            return nullptr;
        }
        
        return getBlock(genesis_hash);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting genesis block: " + std::string(e.what()));
        return nullptr;
    }
}

uint64_t LevelDBBlockStorage::getBlockCount() {
    if (!db_) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string count_str;
        leveldb::Status status = db_->Get(read_options_, COUNT_KEY, &count_str);
        if (!status.ok()) {
            return 0;
        }
        
        return std::stoull(count_str);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting block count: " + std::string(e.what()));
        return 0;
    }
}

uint64_t LevelDBBlockStorage::getCurrentHeight() {
    if (!db_) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string height_str;
        leveldb::Status status = db_->Get(read_options_, HEIGHT_KEY, &height_str);
        if (!status.ok()) {
            return 0;
        }
        
        return std::stoull(height_str);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting current height: " + std::string(e.what()));
        return 0;
    }
}

bool LevelDBBlockStorage::hasBlock(const std::string& block_hash) {
    if (block_hash.empty() || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string block_key = createBlockKey(block_hash);
        std::string value;
        
        leveldb::Status status = db_->Get(read_options_, block_key, &value);
        return status.ok();
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while checking block existence: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::shared_ptr<core::Block>> LevelDBBlockStorage::getBlocksInRange(uint64_t start_height, uint64_t end_height) {
    std::vector<std::shared_ptr<core::Block>> blocks;
    
    if (!db_ || start_height > end_height) {
        return blocks;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        for (uint64_t height = start_height; height <= end_height; ++height) {
            auto block = getBlockByHeight(height);
            if (block) {
                blocks.push_back(block);
            }
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting blocks in range: " + std::string(e.what()));
    }
    
    return blocks;
}

std::vector<std::string> LevelDBBlockStorage::getBlockHashesInRange(uint64_t start_height, uint64_t end_height) {
    std::vector<std::string> hashes;
    
    if (!db_ || start_height > end_height) {
        return hashes;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        for (uint64_t height = start_height; height <= end_height; ++height) {
            std::string height_key = createHeightKey(height);
            std::string block_hash;
            
            leveldb::Status status = db_->Get(read_options_, height_key, &block_hash);
            if (status.ok()) {
                hashes.push_back(block_hash);
            }
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting block hashes in range: " + std::string(e.what()));
    }
    
    return hashes;
}

bool LevelDBBlockStorage::deleteBlocksFromHeight(uint64_t from_height) {
    if (!db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        leveldb::WriteBatch batch;
        uint64_t current_height = getCurrentHeight();
        
        // Delete blocks from the specified height onwards
        for (uint64_t height = from_height; height <= current_height; ++height) {
            std::string height_key = createHeightKey(height);
            std::string block_hash;
            
            leveldb::Status status = db_->Get(read_options_, height_key, &block_hash);
            if (status.ok()) {
                std::string block_key = createBlockKey(block_hash);
                batch.Delete(block_key);
                batch.Delete(height_key);
            }
        }
        
        // Update metadata
        if (from_height > 0) {
            batch.Put(COUNT_KEY, std::to_string(from_height));
            batch.Put(HEIGHT_KEY, std::to_string(from_height - 1));
            
            // Update latest block
            if (from_height > 1) {
                auto latest_block = getBlockByHeight(from_height - 1);
                if (latest_block) {
                    batch.Put(LATEST_KEY, latest_block->calculateHash());
                }
            }
        } else {
            // Delete all blocks
            batch.Delete(COUNT_KEY);
            batch.Delete(HEIGHT_KEY);
            batch.Delete(LATEST_KEY);
        }
        
        leveldb::Status status = db_->Write(write_options_, &batch);
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to delete blocks from height: " + status.ToString());
            return false;
        }
        
        DEO_LOG_INFO(BLOCKCHAIN, "Deleted blocks from height " + std::to_string(from_height));
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while deleting blocks from height: " + std::string(e.what()));
        return false;
    }
}

std::string LevelDBBlockStorage::getStatistics() const {
    if (!db_) {
        return "{}";
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        nlohmann::json stats;
        stats["block_count"] = const_cast<LevelDBBlockStorage*>(this)->getBlockCount();
        stats["current_height"] = const_cast<LevelDBBlockStorage*>(this)->getCurrentHeight();
        stats["data_directory"] = data_directory_;
        stats["database_size"] = "N/A"; // LevelDB doesn't provide direct size info
        
        return stats.dump(2);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting statistics: " + std::string(e.what()));
        return "{}";
    }
}

bool LevelDBBlockStorage::compactDatabase() {
    if (!db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        db_->CompactRange(nullptr, nullptr);
        DEO_LOG_INFO(BLOCKCHAIN, "Database compaction completed");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception during database compaction: " + std::string(e.what()));
        return false;
    }
}

bool LevelDBBlockStorage::repairDatabase() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string db_path = data_directory_ + "/blocks";
        leveldb::Status status = leveldb::RepairDB(db_path, db_options_);
        
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to repair database: " + status.ToString());
            return false;
        }
        
        DEO_LOG_INFO(BLOCKCHAIN, "Database repair completed");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception during database repair: " + std::string(e.what()));
        return false;
    }
}

std::string LevelDBBlockStorage::createBlockKey(const std::string& block_hash) {
    return std::string(BLOCK_PREFIX) + block_hash;
}

std::string LevelDBBlockStorage::createHeightKey(uint64_t height) {
    return std::string(HEIGHT_PREFIX) + std::to_string(height);
}

std::string LevelDBBlockStorage::serializeBlock(const std::shared_ptr<core::Block>& block) {
    try {
        nlohmann::json block_json;
        
        // Serialize block header
        const auto& header = block->getHeader();
        nlohmann::json header_json;
        header_json["version"] = header.version;
        header_json["previous_hash"] = header.previous_hash;
        header_json["merkle_root"] = header.merkle_root;
        header_json["timestamp"] = header.timestamp;
        header_json["nonce"] = header.nonce;
        header_json["difficulty"] = header.difficulty;
        header_json["height"] = header.height;
        
        block_json["header"] = header_json;
        
        // Serialize transactions
        nlohmann::json transactions_json = nlohmann::json::array();
        for (const auto& tx : block->getTransactions()) {
            nlohmann::json tx_json;
            tx_json["hash"] = tx->calculateHash();
            tx_json["version"] = tx->getVersion();
            tx_json["type"] = static_cast<int>(tx->getType());
            tx_json["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                tx->getTimestamp().time_since_epoch()).count();
            
            // Serialize inputs
            nlohmann::json inputs_json = nlohmann::json::array();
            for (const auto& input : tx->getInputs()) {
                nlohmann::json input_json;
                input_json["previous_tx_hash"] = input.previous_tx_hash;
                input_json["output_index"] = input.output_index;
                input_json["signature"] = input.signature;
                input_json["public_key"] = input.public_key;
                input_json["sequence"] = input.sequence;
                inputs_json.push_back(input_json);
            }
            tx_json["inputs"] = inputs_json;
            
            // Serialize outputs
            nlohmann::json outputs_json = nlohmann::json::array();
            for (const auto& output : tx->getOutputs()) {
                nlohmann::json output_json;
                output_json["value"] = output.value;
                output_json["recipient_address"] = output.recipient_address;
                outputs_json.push_back(output_json);
            }
            tx_json["outputs"] = outputs_json;
            
            transactions_json.push_back(tx_json);
        }
        
        block_json["transactions"] = transactions_json;
        
        return block_json.dump();
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while serializing block: " + std::string(e.what()));
        return "";
    }
}

std::shared_ptr<core::Block> LevelDBBlockStorage::deserializeBlock(const std::string& data) {
    try {
        nlohmann::json block_json = nlohmann::json::parse(data);
        
        // Deserialize block header
        const auto& header_json = block_json["header"];
        core::BlockHeader header;
        header.version = header_json["version"];
        header.previous_hash = header_json["previous_hash"];
        header.merkle_root = header_json["merkle_root"];
        header.timestamp = header_json["timestamp"];
        header.nonce = header_json["nonce"];
        header.difficulty = header_json["difficulty"];
        header.height = header_json["height"];
        
        // Deserialize transactions
        std::vector<std::shared_ptr<core::Transaction>> transactions;
        for (const auto& tx_json : block_json["transactions"]) {
            auto tx = std::make_shared<core::Transaction>();
            tx->setVersion(tx_json["version"]);
            // Note: Transaction type is set during construction, not via setter
            
            // Note: Transaction timestamp is set during construction, not via setter
            // Timestamp is handled by the Transaction constructor
            
            // Deserialize inputs
            for (const auto& input_json : tx_json["inputs"]) {
                core::TransactionInput input(
                    input_json["previous_tx_hash"],
                    input_json["output_index"],
                    input_json["signature"],
                    input_json["public_key"],
                    input_json["sequence"]
                );
                tx->addInput(input);
            }
            
            // Deserialize outputs
            for (const auto& output_json : tx_json["outputs"]) {
                core::TransactionOutput output(
                    output_json["value"],
                    output_json["recipient_address"]
                );
                tx->addOutput(output);
            }
            
            transactions.push_back(tx);
        }
        
        return std::make_shared<core::Block>(header, transactions);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while deserializing block: " + std::string(e.what()));
        return nullptr;
    }
}

void LevelDBBlockStorage::updateMetadata(const std::shared_ptr<core::Block>& /* block */) {
    // This method is called from storeBlock, so metadata is already updated there
    // This is a placeholder for any additional metadata updates that might be needed
}

void LevelDBBlockStorage::initializeOptions() {
    // Create cache for better performance
    block_cache_.reset(leveldb::NewLRUCache(100 * 1024 * 1024)); // 100MB cache
    
    // Create filter policy for better performance
    filter_policy_.reset(leveldb::NewBloomFilterPolicy(10));
    
    // Configure database options
    db_options_.create_if_missing = true;
    db_options_.error_if_exists = false;
    db_options_.paranoid_checks = true;
    db_options_.write_buffer_size = 64 * 1024 * 1024; // 64MB write buffer
    db_options_.max_open_files = 1000;
    db_options_.block_size = 4096;
    db_options_.block_restart_interval = 16;
    db_options_.max_file_size = 2 * 1024 * 1024; // 2MB max file size
    db_options_.compression = leveldb::kSnappyCompression;
    db_options_.block_cache = block_cache_.get();
    db_options_.filter_policy = filter_policy_.get();
    
    // Configure read options
    read_options_.verify_checksums = true;
    read_options_.fill_cache = true;
    
    // Configure write options
    write_options_.sync = false; // For better performance, set to true for durability
}

bool LevelDBBlockStorage::createDataDirectory() {
    try {
        std::filesystem::path dir_path(data_directory_);
        if (!std::filesystem::exists(dir_path)) {
            std::filesystem::create_directories(dir_path);
        }
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to create data directory: " + std::string(e.what()));
        return false;
    }
}

} // namespace storage
} // namespace deo
