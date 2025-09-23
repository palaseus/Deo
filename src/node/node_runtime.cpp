/**
 * @file node_runtime.cpp
 * @brief Long-running node runtime implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "node/node_runtime.h"
#include "utils/logger.h"
#include "crypto/hash.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace deo {
namespace node {

// TransactionMempool implementation

bool TransactionMempool::addTransaction(std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    // Check if transaction already exists
    if (transactions_.find(transaction->getId()) != transactions_.end()) {
        return false;
    }
    
    // Add transaction
    transactions_[transaction->getId()] = transaction;
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Added transaction to mempool: " + transaction->getId());
    return true;
}

bool TransactionMempool::removeTransaction(const std::string& tx_id) {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    auto it = transactions_.find(tx_id);
    if (it != transactions_.end()) {
        transactions_.erase(it);
        DEO_LOG_DEBUG(BLOCKCHAIN, "Removed transaction from mempool: " + tx_id);
        return true;
    }
    
    return false;
}

std::vector<std::shared_ptr<core::Transaction>> TransactionMempool::getTransactionsForBlock(size_t max_count) {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    std::vector<std::shared_ptr<core::Transaction>> result;
    result.reserve(std::min(max_count, transactions_.size()));
    
    for (const auto& pair : transactions_) {
        if (result.size() >= max_count) {
            break;
        }
        result.push_back(pair.second);
    }
    
    return result;
}

size_t TransactionMempool::size() const {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    return transactions_.size();
}

void TransactionMempool::clear() {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    transactions_.clear();
    DEO_LOG_DEBUG(BLOCKCHAIN, "Mempool cleared");
}

bool TransactionMempool::contains(const std::string& tx_id) const {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    return transactions_.find(tx_id) != transactions_.end();
}

// NodeRuntime implementation

NodeRuntime::NodeRuntime(const NodeConfig& config)
    : config_(config)
    , running_(false)
    , mining_enabled_(config.enable_mining)
    , stop_threads_(false) {
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "NodeRuntime created with config");
}

NodeRuntime::~NodeRuntime() {
    stop();
    DEO_LOG_DEBUG(BLOCKCHAIN, "NodeRuntime destroyed");
}

bool NodeRuntime::initialize() {
    DEO_LOG_INFO(BLOCKCHAIN, "Initializing node runtime");
    
    try {
        // Create data directories
        std::filesystem::create_directories(config_.data_directory);
        std::filesystem::create_directories(config_.state_directory);
        
        // Initialize state store
        state_store_ = std::make_shared<vm::StateStore>(config_.state_directory + "/state");
        if (!state_store_->initialize()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize state store");
            return false;
        }
        
        // Initialize blockchain
        core::BlockchainConfig blockchain_config;
        blockchain_config.network_id = "deo_mainnet";
        blockchain_config.data_directory = config_.data_directory;
        blockchain_config.block_time = 10; // 10 seconds
        blockchain_config.max_block_size = config_.block_size_limit;
        blockchain_config.max_transactions_per_block = 1000;
        blockchain_config.difficulty_adjustment_interval = 2016;
        blockchain_config.genesis_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        blockchain_config.genesis_merkle_root = "0000000000000000000000000000000000000000000000000000000000000000";
        blockchain_config.initial_difficulty = config_.mining_difficulty;
        blockchain_config.enable_mining = config_.enable_mining;
        blockchain_config.enable_networking = false; // Disable for now
        blockchain_ = std::make_unique<core::Blockchain>(blockchain_config);
        if (!loadBlockchain()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to load blockchain");
            return false;
        }
        
        // Initialize VM block validator
        block_validator_ = std::make_unique<vm::VMBlockValidator>(state_store_);
        
        // Initialize proof of work consensus
        pow_consensus_ = std::make_unique<consensus::ProofOfWork>(config_.mining_difficulty);
        
        // Initialize mempool
        mempool_ = std::make_unique<TransactionMempool>();
        
        // Initialize JSON-RPC server if enabled
        if (config_.enable_json_rpc) {
            json_rpc_server_ = std::make_unique<api::JsonRpcServer>(config_.json_rpc_port);
            if (!json_rpc_server_->initialize()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize JSON-RPC server");
                return false;
            }
        }
        
        // Initialize statistics
        statistics_ = NodeStatistics{};
        
        DEO_LOG_INFO(BLOCKCHAIN, "Node runtime initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize node runtime: " + std::string(e.what()));
        return false;
    }
}

bool NodeRuntime::start() {
    if (running_) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Node runtime is already running");
        return true;
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Starting node runtime");
    
    try {
        running_ = true;
        stop_threads_ = false;
        
        // Start block production thread
        if (mining_enabled_) {
            block_production_thread_ = std::thread(&NodeRuntime::blockProductionLoop, this);
            DEO_LOG_INFO(BLOCKCHAIN, "Block production thread started");
        }
        
        // Start mempool management thread
        mempool_thread_ = std::thread(&NodeRuntime::mempoolLoop, this);
        DEO_LOG_INFO(BLOCKCHAIN, "Mempool management thread started");
        
        // Start JSON-RPC server if enabled
        if (json_rpc_server_) {
            if (!json_rpc_server_->start()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to start JSON-RPC server");
                return false;
            }
            DEO_LOG_INFO(BLOCKCHAIN, "JSON-RPC server started on port " + std::to_string(config_.json_rpc_port));
        }
        
        DEO_LOG_INFO(BLOCKCHAIN, "Node runtime started successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to start node runtime: " + std::string(e.what()));
        running_ = false;
        return false;
    }
}

void NodeRuntime::stop() {
    if (!running_) {
        return;
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Stopping node runtime");
    
    running_ = false;
    stop_threads_ = true;
    
    // Notify threads to stop
    block_production_cv_.notify_all();
    
    // Stop JSON-RPC server
    if (json_rpc_server_) {
        json_rpc_server_->stop();
    }
    
    // Wait for threads to finish
    if (block_production_thread_.joinable()) {
        block_production_thread_.join();
    }
    
    if (mempool_thread_.joinable()) {
        mempool_thread_.join();
    }
    
    // Save blockchain
    saveBlockchain();
    
    DEO_LOG_INFO(BLOCKCHAIN, "Node runtime stopped");
}

bool NodeRuntime::isRunning() const {
    return running_;
}

bool NodeRuntime::addTransaction(std::shared_ptr<core::Transaction> transaction) {
    if (!running_) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Cannot add transaction: node not running");
        return false;
    }
    
    if (!transaction) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Cannot add null transaction");
        return false;
    }
    
    // Validate transaction
    if (!transaction->validate()) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Invalid transaction: " + transaction->getId());
        return false;
    }
    
    // Add to mempool
    bool added = mempool_->addTransaction(transaction);
    if (added) {
        DEO_LOG_INFO(BLOCKCHAIN, "Transaction added to mempool: " + transaction->getId());
    } else {
        DEO_LOG_WARNING(BLOCKCHAIN, "Failed to add transaction to mempool: " + transaction->getId());
    }
    
    return added;
}

std::string NodeRuntime::getContractInfo(const std::string& address) {
    if (!block_validator_) {
        return "{}";
    }
    
    try {
        auto contract_state = block_validator_->getStateStore()->getContractState(address);
        if (!contract_state) {
            return "{\"error\": \"Contract not found\"}";
        }
        
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"address\": \"" << address << "\",\n";
        ss << "  \"bytecode_size\": " << contract_state->bytecode.size() << ",\n";
        ss << "  \"balance\": \"" << contract_state->balance.toString() << "\",\n";
        ss << "  \"nonce\": " << contract_state->nonce << ",\n";
        ss << "  \"is_deployed\": " << (contract_state->is_deployed ? "true" : "false") << ",\n";
        ss << "  \"deployment_block\": " << contract_state->deployment_block << ",\n";
        ss << "  \"deployer_address\": \"" << contract_state->deployer_address << "\",\n";
        ss << "  \"storage_entries\": " << contract_state->storage.size() << "\n";
        ss << "}";
        
        return ss.str();
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get contract info: " + std::string(e.what()));
        return "{\"error\": \"Failed to get contract info\"}";
    }
}

NodeStatistics NodeRuntime::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    NodeStatistics stats = statistics_;
    stats.mempool_size = mempool_->size();
    
    if (blockchain_) {
        auto blockchain_state = blockchain_->getState();
        stats.blockchain_height = blockchain_state.height;
        stats.best_block_hash = blockchain_state.best_block_hash;
        stats.is_syncing = blockchain_state.is_syncing;
    }
    
    stats.is_mining = mining_enabled_ && running_;
    
    return stats;
}

core::BlockchainState NodeRuntime::getBlockchainState() const {
    if (blockchain_) {
        return blockchain_->getState();
    }
    return core::BlockchainState{};
}

std::string NodeRuntime::replayBlock(const std::string& block_hash) {
    if (!blockchain_ || !block_validator_) {
        return "{\"error\": \"Blockchain or validator not initialized\"}";
    }
    
    try {
        auto block = blockchain_->getBlock(block_hash);
        if (!block) {
            return "{\"error\": \"Block not found\"}";
        }
        
        // Create a temporary state snapshot
        std::string snapshot_id = block_validator_->createStateSnapshot();
        
        // Replay block validation
        auto result = block_validator_->validateBlock(block);
        
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"block_hash\": \"" << block_hash << "\",\n";
        ss << "  \"validation_success\": " << (result.success ? "true" : "false") << ",\n";
        ss << "  \"total_gas_used\": " << result.total_gas_used << ",\n";
        ss << "  \"transactions_executed\": " << result.gas_used_per_tx.size() << ",\n";
        ss << "  \"contracts_executed\": " << result.executed_contracts.size() << ",\n";
        
        if (!result.success) {
            ss << "  \"error_message\": \"" << result.error_message << "\",\n";
        }
        
        ss << "  \"gas_used_per_transaction\": {\n";
        bool first = true;
        for (const auto& pair : result.gas_used_per_tx) {
            if (!first) ss << ",\n";
            ss << "    \"" << pair.first << "\": " << pair.second;
            first = false;
        }
        ss << "\n  }\n";
        ss << "}";
        
        // Restore state snapshot
        block_validator_->restoreStateSnapshot(snapshot_id);
        
        return ss.str();
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to replay block: " + std::string(e.what()));
        return "{\"error\": \"Failed to replay block: " + std::string(e.what()) + "\"}";
    }
}

std::string NodeRuntime::getJsonRpcStatistics() const {
    if (!json_rpc_server_) {
        return "{\"error\": \"JSON-RPC server not enabled\"}";
    }
    
    try {
        auto stats = json_rpc_server_->getStatistics();
        return stats.dump(2);
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get JSON-RPC statistics: " + std::string(e.what()));
        return "{\"error\": \"Failed to get JSON-RPC statistics\"}";
    }
}

void NodeRuntime::blockProductionLoop() {
    DEO_LOG_INFO(BLOCKCHAIN, "Block production loop started");
    
    while (!stop_threads_ && running_) {
        try {
            // Check if we should produce a block
            if (mempool_->size() > 0) {
                auto block = createBlock();
                if (block) {
                    if (mineBlock(block)) {
                        if (validateAndAddBlock(block)) {
                            DEO_LOG_INFO(BLOCKCHAIN, "Block mined and added: " + block->calculateHash());
                            
                            // Remove transactions from mempool
                            for (const auto& tx : block->getTransactions()) {
                                mempool_->removeTransaction(tx->getId());
                            }
                            
                            // Update statistics
                            statistics_.blocks_mined++;
                        }
                    }
                }
            }
            
            // Wait before next iteration
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
        } catch (const std::exception& e) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Error in block production loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Block production loop stopped");
}

void NodeRuntime::mempoolLoop() {
    DEO_LOG_INFO(BLOCKCHAIN, "Mempool management loop started");
    
    while (!stop_threads_ && running_) {
        try {
            // Update statistics
            updateStatistics();
            
            // Wait before next iteration
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
        } catch (const std::exception& e) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Error in mempool loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Mempool management loop stopped");
}

std::shared_ptr<core::Block> NodeRuntime::createBlock() {
    try {
        // Get transactions from mempool
        auto transactions = mempool_->getTransactionsForBlock(100);
        if (transactions.empty()) {
            return nullptr;
        }
        
        // Get current blockchain state
        auto blockchain_state = blockchain_->getState();
        
        // Create block header
        core::BlockHeader header(
            1, // version
            blockchain_state.best_block_hash, // previous hash
            "", // merkle root (will be calculated)
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count(), // timestamp
            0, // nonce (will be set during mining)
            config_.mining_difficulty, // difficulty
            blockchain_state.height + 1, // height
            static_cast<uint32_t>(transactions.size()) // transaction count
        );
        
        // Create block
        auto block = std::make_shared<core::Block>(header, transactions);
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Created block with " + std::to_string(transactions.size()) + " transactions");
        return block;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to create block: " + std::string(e.what()));
        return nullptr;
    }
}

bool NodeRuntime::mineBlock(std::shared_ptr<core::Block> block) {
    if (!pow_consensus_) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Proof of work consensus not initialized");
        return false;
    }
    
    try {
        DEO_LOG_DEBUG(BLOCKCHAIN, "Starting block mining");
        
        // Mine the block
        bool mined = pow_consensus_->mineBlock(block);
        
        if (mined) {
            DEO_LOG_INFO(BLOCKCHAIN, "Block mined successfully: " + block->calculateHash());
        } else {
            DEO_LOG_WARNING(BLOCKCHAIN, "Block mining failed");
        }
        
        return mined;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Error during block mining: " + std::string(e.what()));
        return false;
    }
}

bool NodeRuntime::validateAndAddBlock(std::shared_ptr<core::Block> block) {
    if (!block_validator_ || !blockchain_) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Block validator or blockchain not initialized");
        return false;
    }
    
    try {
        // Validate block with VM
        auto validation_result = block_validator_->validateBlock(block);
        
        if (!validation_result.success) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Block validation failed: " + validation_result.error_message);
            return false;
        }
        
        // Add block to blockchain
        bool added = blockchain_->addBlock(block);
        
        if (added) {
            DEO_LOG_INFO(BLOCKCHAIN, "Block added to blockchain: " + block->calculateHash());
            
            // Update statistics
            statistics_.transactions_processed += block->getTransactions().size();
            statistics_.total_gas_used += validation_result.total_gas_used;
            statistics_.contracts_deployed += validation_result.executed_contracts.size();
        }
        
        return added;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Error during block validation and addition: " + std::string(e.what()));
        return false;
    }
}

void NodeRuntime::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // Update mempool size
    statistics_.mempool_size = mempool_->size();
    
    // Update blockchain height
    if (blockchain_) {
        auto blockchain_state = blockchain_->getState();
        statistics_.blockchain_height = blockchain_state.height;
        statistics_.best_block_hash = blockchain_state.best_block_hash;
    }
}

bool NodeRuntime::loadBlockchain() {
    try {
        // For now, we'll just initialize the blockchain
        // In a real implementation, you would load from disk
        
        if (!initializeGenesisBlock()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize genesis block");
            return false;
        }
        
        DEO_LOG_INFO(BLOCKCHAIN, "Blockchain loaded successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to load blockchain: " + std::string(e.what()));
        return false;
    }
}

bool NodeRuntime::saveBlockchain() {
    try {
        // For now, this is a placeholder
        // In a real implementation, you would save to disk
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Blockchain saved");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to save blockchain: " + std::string(e.what()));
        return false;
    }
}

bool NodeRuntime::initializeGenesisBlock() {
    try {
        // Create genesis block
        core::BlockHeader genesis_header(
            1, // version
            "0000000000000000000000000000000000000000000000000000000000000000", // previous hash
            "0000000000000000000000000000000000000000000000000000000000000000", // merkle root
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count(), // timestamp
            0, // nonce
            1, // difficulty
            0, // height
            0 // transaction count
        );
        
        auto genesis_block = std::make_shared<core::Block>(genesis_header, std::vector<std::shared_ptr<core::Transaction>>());
        
        // Add genesis block to blockchain
        bool added = blockchain_->addBlock(genesis_block);
        
        if (added) {
            DEO_LOG_INFO(BLOCKCHAIN, "Genesis block initialized");
        }
        
        return added;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize genesis block: " + std::string(e.what()));
        return false;
    }
}

} // namespace node
} // namespace deo
