/**
 * @file node_runtime.cpp
 * @brief Long-running node runtime implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "node/node_runtime.h"
#include "utils/logger.h"
#include "utils/performance_monitor.h"
#include "crypto/hash.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <atomic>

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
    // Constructor is minimal - no logging to avoid potential initialization deadlocks
}

NodeRuntime::~NodeRuntime() {
    stop();
    DEO_LOG_DEBUG(BLOCKCHAIN, "NodeRuntime destroyed");
}

bool NodeRuntime::initialize() {
    DEO_LOG_INFO(BLOCKCHAIN, "Initializing node runtime");
    
    try {
        // Create data directories
        DEO_LOG_INFO(BLOCKCHAIN, "Creating data directories");
        std::filesystem::create_directories(config_.data_directory);
        std::filesystem::create_directories(config_.state_directory);
        DEO_LOG_INFO(BLOCKCHAIN, "Directories created");
        
        // Initialize state store with storage backend
        DEO_LOG_INFO(BLOCKCHAIN, "Creating StateStore");
        state_store_ = std::make_shared<vm::StateStore>(config_.state_directory + "/state", config_.storage_backend);
        DEO_LOG_INFO(BLOCKCHAIN, "StateStore created, calling initialize()");
        if (!state_store_->initialize()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize state store");
            return false;
        } else {
            DEO_LOG_INFO(BLOCKCHAIN, "State store initialized with backend: " + config_.storage_backend);
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
        blockchain_config.enable_networking = config_.enable_p2p;
        blockchain_config.storage_backend = config_.storage_backend; // Pass storage backend to blockchain
        blockchain_ = std::make_unique<core::Blockchain>(blockchain_config);
        DEO_LOG_INFO(BLOCKCHAIN, "Blockchain object created, initializing...");
        if (!blockchain_->initialize()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize blockchain");
            return false;
        }
        DEO_LOG_INFO(BLOCKCHAIN, "Blockchain initialized, loading genesis block...");
        if (!loadBlockchain()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to load blockchain");
            return false;
        }
        
        // Initialize VM block validator
        block_validator_ = std::make_unique<vm::VMBlockValidator>(state_store_);
        
        // Initialize consensus engine (Proof of Work)
        consensus_engine_ = std::make_shared<consensus::ProofOfWork>(config_.mining_difficulty);
        if (!consensus_engine_->initialize()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize consensus engine");
            return false;
        }
        
        // Initialize mempool
        mempool_ = std::make_unique<TransactionMempool>();
        
        // Initialize P2P network manager if enabled
        if (config_.enable_p2p) {
            // Create a shared_ptr wrapper for blockchain (network manager needs shared ownership)
            // The unique_ptr will continue to own the blockchain, shared_ptr is just for sharing
            auto blockchain_shared = std::shared_ptr<core::Blockchain>(
                blockchain_.get(),
                [](core::Blockchain*) { /* No-op deleter, unique_ptr manages lifetime */ }
            );
            
            p2p_network_ = std::make_shared<network::P2PNetworkManager>(config_.p2p_port);
            if (!p2p_network_->initialize(blockchain_shared, consensus_engine_)) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize P2P network manager");
                return false;
            }
            
            // Set up event handlers for incoming transactions and blocks
            p2p_network_->setTransactionHandler([this](std::shared_ptr<core::Transaction> tx) {
                this->handleIncomingTransaction(tx);
            });
            
            p2p_network_->setBlockHandler([this](std::shared_ptr<core::Block> block) {
                this->handleIncomingBlock(block);
            });
            
            // Add bootstrap nodes
            for (const auto& bootstrap : config_.bootstrap_nodes) {
                // Bootstrap nodes are in format "address:port"
                size_t colon_pos = bootstrap.find(':');
                if (colon_pos != std::string::npos) {
                    std::string address = bootstrap.substr(0, colon_pos);
                    uint16_t port = static_cast<uint16_t>(std::stoul(bootstrap.substr(colon_pos + 1)));
                    p2p_network_->addBootstrapNode(address, port);
                }
            }
            
            DEO_LOG_INFO(BLOCKCHAIN, "P2P network manager initialized on port " + std::to_string(config_.p2p_port));
            
            // Initialize chain synchronization manager if LevelDB storage is available
            // FastSyncManager requires LevelDB storage AND P2P network
            if (config_.storage_backend == "leveldb" && config_.enable_p2p) {
                // Get LevelDB storage instances from blockchain
                auto leveldb_block_storage = blockchain_->getLevelDBBlockStorage();
                auto leveldb_state_storage = blockchain_->getLevelDBStateStorage();
                
                if (leveldb_block_storage && leveldb_state_storage && p2p_network_) {
                    // Get PeerManager from P2PNetworkManager
                    auto peer_manager = p2p_network_->getPeerManager();
                    if (peer_manager) {
                        // Initialize sync manager with LevelDB storage instances and PeerManager
                        sync::SyncConfig sync_config;
                        sync_config.mode = sync::SyncMode::FAST_SYNC;
                        
                        sync_manager_ = std::make_unique<sync::FastSyncManager>(
                            peer_manager,
                            leveldb_block_storage,
                            leveldb_state_storage
                        );
                        
                        if (!sync_manager_->initialize(sync_config)) {
                            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize fast sync manager");
                            return false;
                        }
                        
                        DEO_LOG_INFO(BLOCKCHAIN, "Fast sync manager initialized with LevelDB backend");
                    } else {
                        DEO_LOG_WARNING(BLOCKCHAIN, "PeerManager not available, sync manager not initialized");
                    }
                } else {
                    DEO_LOG_WARNING(BLOCKCHAIN, "LevelDB storage instances or P2P network not available, sync manager not initialized");
                }
            } else {
                if (config_.storage_backend != "leveldb") {
                    DEO_LOG_INFO(BLOCKCHAIN, "Sync manager requires LevelDB backend (currently using: " + config_.storage_backend + ")");
                }
                if (!config_.enable_p2p) {
                    DEO_LOG_INFO(BLOCKCHAIN, "Sync manager requires P2P networking (currently disabled)");
                }
            }
        }
        
        // Initialize JSON-RPC server if enabled
        if (config_.enable_json_rpc) {
            json_rpc_server_ = std::make_unique<api::JsonRpcServer>(
                config_.json_rpc_port,
                config_.json_rpc_host,
                this,
                config_.json_rpc_username,
                config_.json_rpc_password
            );
            if (!json_rpc_server_->initialize()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize JSON-RPC server");
                return false;
            }
            DEO_LOG_INFO(BLOCKCHAIN, "JSON-RPC server initialized on " + config_.json_rpc_host + 
                        ":" + std::to_string(config_.json_rpc_port));
        }
        
        // Initialize statistics
        statistics_ = NodeStatistics{};
        start_time_ = std::chrono::system_clock::now();
        tps_window_start_ = start_time_;
        transaction_count_window_ = 0;
        recent_block_times_.clear();
        
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
        
        // Start P2P network if enabled
        if (p2p_network_ && p2p_network_->isRunning()) {
            // Attempt to connect to bootstrap nodes
            for (const auto& bootstrap : config_.bootstrap_nodes) {
                size_t colon_pos = bootstrap.find(':');
                if (colon_pos != std::string::npos) {
                    std::string address = bootstrap.substr(0, colon_pos);
                    uint16_t port = static_cast<uint16_t>(std::stoul(bootstrap.substr(colon_pos + 1)));
                    p2p_network_->connectToPeer(address, port);
                }
            }
            DEO_LOG_INFO(BLOCKCHAIN, "P2P network started and listening on port " + std::to_string(config_.p2p_port));
        } else if (config_.enable_p2p && p2p_network_) {
            DEO_LOG_WARNING(BLOCKCHAIN, "P2P network configured but not running");
        }
        
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
    
    // Stop sync manager
    if (sync_manager_) {
        sync_manager_->shutdown();
    }
    
    // Stop P2P network
    if (p2p_network_) {
        p2p_network_->shutdown();
    }
    
    // Stop consensus engine
    if (consensus_engine_) {
        consensus_engine_->shutdown();
    }
    
    // Stop JSON-RPC server
    if (json_rpc_server_) {
        json_rpc_server_->stop();
    }
    
    // Wait briefly for threads to check stop flags and exit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Join threads with timeout protection using helper threads
    // join() can block indefinitely, so we use a helper thread with a timeout
    if (block_production_thread_.joinable()) {
        std::atomic<bool> join_complete(false);
        std::thread join_helper([this, &join_complete]() {
            if (block_production_thread_.joinable()) {
                block_production_thread_.join();
            }
            join_complete = true;
        });
        
        // Wait up to 500ms for join to complete
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (!join_complete && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (!join_complete) {
            // Join timed out - thread may be stuck, detach to avoid hanging
            DEO_LOG_WARNING(BLOCKCHAIN, "Block production thread join timed out, detaching");
            if (block_production_thread_.joinable()) {
                block_production_thread_.detach();
            }
            join_helper.detach();
        } else {
            if (join_helper.joinable()) {
                join_helper.join();
            }
        }
    }
    
    if (mempool_thread_.joinable()) {
        std::atomic<bool> join_complete(false);
        std::thread join_helper([this, &join_complete]() {
            if (mempool_thread_.joinable()) {
                mempool_thread_.join();
            }
            join_complete = true;
        });
        
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (!join_complete && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (!join_complete) {
            DEO_LOG_WARNING(BLOCKCHAIN, "Mempool thread join timed out, detaching");
            if (mempool_thread_.joinable()) {
                mempool_thread_.detach();
            }
            join_helper.detach();
        } else {
            if (join_helper.joinable()) {
                join_helper.join();
            }
        }
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
        
        // Update transaction count for TPS calculation
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            transaction_count_window_++;
        }
        
        // Broadcast transaction to network if P2P is enabled
        if (p2p_network_ && p2p_network_->isRunning()) {
            broadcastTransaction(transaction);
        }
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
    
    // Calculate TPS (transactions per second) over last window (read-only calculation)
    auto now = std::chrono::system_clock::now();
    auto window_duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - tps_window_start_).count();
    if (window_duration > 0) {
        stats.transactions_per_second = 
            static_cast<double>(transaction_count_window_) / window_duration;
    } else {
        stats.transactions_per_second = 0.0;
    }
    
    // Calculate average block time from recent blocks
    if (!recent_block_times_.empty()) {
        double sum = 0.0;
        for (double time : recent_block_times_) {
            sum += time;
        }
        stats.avg_block_time_seconds = sum / recent_block_times_.size();
    } else {
        stats.avg_block_time_seconds = 0.0;
    }
    
    // Update sync status if sync manager is available
    if (sync_manager_) {
        stats.sync_status = sync_manager_->getSyncStatus();
        stats.sync_progress = static_cast<uint64_t>(sync_manager_->getSyncProgress() * 100.0);
        stats.current_sync_height = sync_manager_->getCurrentHeight();
        stats.target_sync_height = sync_manager_->getTargetHeight();
        stats.is_syncing = sync_manager_->isSyncActive();
        
        // Calculate sync speed
        auto sync_stats = sync_manager_->getSyncStatistics();
        auto sync_duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - sync_stats.start_time).count();
        if (sync_duration > 0 && sync_stats.blocks_downloaded > 0) {
            stats.sync_speed_blocks_per_sec = 
                static_cast<double>(sync_stats.blocks_downloaded) / sync_duration;
        } else {
            stats.sync_speed_blocks_per_sec = 0.0;
        }
    } else {
        stats.sync_status = sync::SyncStatus::IDLE;
        stats.sync_progress = 0;
        stats.current_sync_height = 0;
        stats.target_sync_height = 0;
        stats.is_syncing = false;
        stats.sync_speed_blocks_per_sec = 0.0;
    }
    
    // Update network statistics
    if (p2p_network_ && p2p_network_->isRunning()) {
        auto network_stats = p2p_network_->getNetworkStats();
        stats.total_network_messages = 
            network_stats.tcp_stats.messages_sent + network_stats.tcp_stats.messages_received;
    } else {
        stats.total_network_messages = 0;
    }
    
    // Get performance monitor metrics for storage operations
    auto& perf_monitor = utils::PerformanceMonitor::getInstance();
    auto all_metrics = perf_monitor.getAllMetrics();
    uint64_t storage_ops = 0;
    for (const auto& [name, metrics] : all_metrics) {
        if (name.find("storage") != std::string::npos || 
            name.find("store") != std::string::npos ||
            name.find("getBlock") != std::string::npos) {
            storage_ops += metrics.call_count;
        }
    }
    stats.total_storage_operations = storage_ops;
    
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

std::shared_ptr<core::Block> NodeRuntime::getBlock(const std::string& hash) const {
    if (!blockchain_) {
        return nullptr;
    }
    return blockchain_->getBlock(hash);
}

std::shared_ptr<core::Block> NodeRuntime::getBlock(uint64_t height) const {
    if (!blockchain_) {
        return nullptr;
    }
    return blockchain_->getBlock(height);
}

std::shared_ptr<core::Transaction> NodeRuntime::getTransaction(const std::string& tx_id) const {
    if (!blockchain_) {
        return nullptr;
    }
    return blockchain_->getTransaction(tx_id);
}

std::vector<std::shared_ptr<core::Transaction>> NodeRuntime::getMempoolTransactions(size_t max_count) const {
    if (!mempool_) {
        return {};
    }
    // Use the same mempool that addTransaction() uses
    return mempool_->getTransactionsForBlock(max_count > 0 ? max_count : mempool_->size());
}

uint64_t NodeRuntime::getBalance(const std::string& address) const {
    if (!blockchain_) {
        return 0;
    }
    return blockchain_->getBalance(address);
}

size_t NodeRuntime::getMempoolSize() const {
    if (!mempool_) {
        return 0;
    }
    return mempool_->size();
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
            
            // Wait before next iteration, but check stop flag frequently
            // Use shorter sleeps to ensure quick shutdown
            for (int i = 0; i < 10 && !stop_threads_ && running_; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
        } catch (const std::exception& e) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Error in block production loop: " + std::string(e.what()));
            // Check stop flag more frequently during error recovery
            for (int i = 0; i < 10 && !stop_threads_ && running_; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Block production loop stopped");
}

void NodeRuntime::mempoolLoop() {
    DEO_LOG_INFO(BLOCKCHAIN, "Mempool management loop started");
    
    while (!stop_threads_ && running_) {
        // Check stop flags first before doing any work
        if (stop_threads_ || !running_) {
            break;
        }
        
        try {
            // Update statistics
            updateStatistics();
            
            // Wait before next iteration, but check stop flag frequently
            // Use shorter sleeps to ensure quick shutdown - check every 100ms
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Error in mempool loop: " + std::string(e.what()));
            // Check stop flag more frequently during error recovery
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
    if (!consensus_engine_) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Consensus engine not initialized");
        return false;
    }
    
    try {
        DEO_LOG_DEBUG(BLOCKCHAIN, "Starting block mining");
        
        // Cast to ProofOfWork for mining (since we're using PoW for now)
        auto pow_engine = std::dynamic_pointer_cast<consensus::ProofOfWork>(consensus_engine_);
        if (!pow_engine) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Consensus engine is not ProofOfWork");
            return false;
        }
        
        // Mine the block
        bool mined = pow_engine->mineBlock(block);
        
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
            
            // Update performance metrics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                auto now = std::chrono::system_clock::now();
                if (last_block_time_.time_since_epoch().count() > 0) {
                    auto block_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last_block_time_).count() / 1000.0;
                    recent_block_times_.push_back(block_time);
                    if (recent_block_times_.size() > MAX_RECENT_BLOCK_TIMES) {
                        recent_block_times_.erase(recent_block_times_.begin());
                    }
                }
                last_block_time_ = now;
            }
            
            // Broadcast block to network if P2P is enabled
            if (p2p_network_ && p2p_network_->isRunning()) {
                broadcastBlock(block);
            }
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
        statistics_.is_syncing = blockchain_state.is_syncing;
    }
    
    // Calculate TPS (transactions per second) over last window
    auto now = std::chrono::system_clock::now();
    auto window_duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - tps_window_start_).count();
    if (window_duration > 0) {
        statistics_.transactions_per_second = 
            static_cast<double>(transaction_count_window_) / window_duration;
        // Reset window every 10 seconds
        if (window_duration >= 10) {
            transaction_count_window_ = 0;
            tps_window_start_ = now;
        }
    } else {
        statistics_.transactions_per_second = 0.0;
    }
    
    // Calculate average block time from recent blocks
    if (!recent_block_times_.empty()) {
        double sum = 0.0;
        for (double time : recent_block_times_) {
            sum += time;
        }
        statistics_.avg_block_time_seconds = sum / recent_block_times_.size();
    } else {
        statistics_.avg_block_time_seconds = 0.0;
    }
    
    // Update sync status if sync manager is available
    if (sync_manager_) {
        statistics_.sync_status = sync_manager_->getSyncStatus();
        statistics_.sync_progress = static_cast<uint64_t>(sync_manager_->getSyncProgress() * 100.0);
        statistics_.current_sync_height = sync_manager_->getCurrentHeight();
        statistics_.target_sync_height = sync_manager_->getTargetHeight();
        statistics_.is_syncing = sync_manager_->isSyncActive();
        
        // Calculate sync speed
        auto sync_stats = sync_manager_->getSyncStatistics();
        auto sync_duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - sync_stats.start_time).count();
        if (sync_duration > 0 && sync_stats.blocks_downloaded > 0) {
            statistics_.sync_speed_blocks_per_sec = 
                static_cast<double>(sync_stats.blocks_downloaded) / sync_duration;
        } else {
            statistics_.sync_speed_blocks_per_sec = 0.0;
        }
    } else {
        statistics_.sync_status = sync::SyncStatus::IDLE;
        statistics_.sync_progress = 0;
        statistics_.current_sync_height = 0;
        statistics_.target_sync_height = 0;
        statistics_.sync_speed_blocks_per_sec = 0.0;
    }
    
    // Update network statistics
    if (p2p_network_ && p2p_network_->isRunning()) {
        auto network_stats = p2p_network_->getNetworkStats();
        statistics_.total_network_messages = 
            network_stats.tcp_stats.messages_sent + network_stats.tcp_stats.messages_received;
    } else {
        statistics_.total_network_messages = 0;
    }
    
    // Get performance monitor metrics for storage operations
    auto& perf_monitor = utils::PerformanceMonitor::getInstance();
    auto all_metrics = perf_monitor.getAllMetrics();
    uint64_t storage_ops = 0;
    for (const auto& [name, metrics] : all_metrics) {
        if (name.find("storage") != std::string::npos || 
            name.find("store") != std::string::npos ||
            name.find("getBlock") != std::string::npos) {
            storage_ops += metrics.call_count;
        }
    }
    statistics_.total_storage_operations = storage_ops;
}

bool NodeRuntime::loadBlockchain() {
    try {
        // Blockchain::initialize() already creates the genesis block if blocks are empty
        // So we don't need to create it again here - just verify it exists
        if (!blockchain_ || !blockchain_->isInitialized()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Blockchain not initialized");
            return false;
        }
        
        auto state = blockchain_->getState();
        if (state.best_block_hash.empty()) {
            // No genesis block found, create one
            DEO_LOG_INFO(BLOCKCHAIN, "No genesis block found, creating one...");
            if (!initializeGenesisBlock()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize genesis block");
                return false;
            }
        } else {
            DEO_LOG_INFO(BLOCKCHAIN, "Genesis block already exists at height " + std::to_string(state.height));
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
        // Check if genesis block already exists
        auto state = blockchain_->getState();
        if (!state.best_block_hash.empty()) {
            DEO_LOG_INFO(BLOCKCHAIN, "Genesis block already exists");
            return true;
        }
        
        // Create genesis block manually (since createGenesisBlock() is private)
        core::BlockHeader genesis_header(
            1, // version
            "0000000000000000000000000000000000000000000000000000000000000000", // previous hash
            "0000000000000000000000000000000000000000000000000000000000000000", // merkle root
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count(), // timestamp
            0, // nonce
            config_.mining_difficulty, // difficulty (from config)
            0, // height
            0 // transaction count
        );
        
        auto genesis_block = std::make_shared<core::Block>(genesis_header, std::vector<std::shared_ptr<core::Transaction>>());
        
        // Add genesis block to blockchain
        // This should work now that we skip difficulty validation for genesis blocks
        bool added = blockchain_->addBlock(genesis_block);
        
        if (added) {
            DEO_LOG_INFO(BLOCKCHAIN, "Genesis block initialized");
        } else {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to add genesis block to blockchain");
        }
        
        return added;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to initialize genesis block: " + std::string(e.what()));
        return false;
    }
}

std::shared_ptr<network::P2PNetworkManager> NodeRuntime::getP2PNetworkManager() const {
    return p2p_network_;
}

void NodeRuntime::broadcastTransaction(std::shared_ptr<core::Transaction> transaction) {
    if (!p2p_network_ || !p2p_network_->isRunning()) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Cannot broadcast transaction: P2P network not running");
        return;
    }
    
    try {
        p2p_network_->broadcastTransaction(transaction);
        DEO_LOG_DEBUG(BLOCKCHAIN, "Broadcast transaction to network: " + transaction->getId());
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to broadcast transaction: " + std::string(e.what()));
    }
}

void NodeRuntime::broadcastBlock(std::shared_ptr<core::Block> block) {
    if (!p2p_network_ || !p2p_network_->isRunning()) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Cannot broadcast block: P2P network not running");
        return;
    }
    
    try {
        p2p_network_->broadcastBlock(block);
        DEO_LOG_INFO(BLOCKCHAIN, "Broadcast block to network: " + block->calculateHash());
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to broadcast block: " + std::string(e.what()));
    }
}

void NodeRuntime::handleIncomingTransaction(std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Received null transaction from network");
        return;
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Received transaction from network: " + transaction->getId());
    
    // Validate the transaction
    if (!transaction->validate()) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Invalid transaction received from network: " + transaction->getId());
        return;
    }
    
    // Check if we already have this transaction
    if (mempool_->contains(transaction->getId())) {
        DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction already in mempool: " + transaction->getId());
        return;
    }
    
    // Check if transaction is already in blockchain
    if (blockchain_->getTransaction(transaction->getId())) {
        DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction already in blockchain: " + transaction->getId());
        return;
    }
    
    // Add to mempool
    if (mempool_->addTransaction(transaction)) {
        DEO_LOG_INFO(BLOCKCHAIN, "Added incoming transaction to mempool: " + transaction->getId());
    } else {
        DEO_LOG_WARNING(BLOCKCHAIN, "Failed to add incoming transaction to mempool: " + transaction->getId());
    }
}

void NodeRuntime::handleIncomingBlock(std::shared_ptr<core::Block> block) {
    if (!block) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Received null block from network");
        return;
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Received block from network: " + block->calculateHash());
    
    // Check if we already have this block
    auto existing_block = blockchain_->getBlock(block->calculateHash());
    if (existing_block) {
        DEO_LOG_DEBUG(BLOCKCHAIN, "Block already in blockchain: " + block->calculateHash());
        return;
    }
    
    // Validate the block
    if (!block_validator_) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Block validator not initialized");
        return;
    }
    
    // Validate block with VM
    auto validation_result = block_validator_->validateBlock(block);
    if (!validation_result.success) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Invalid block received from network: " + validation_result.error_message);
        return;
    }
    
    // Check consensus validation
    if (consensus_engine_ && !consensus_engine_->validateBlock(block)) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Block failed consensus validation: " + block->calculateHash());
        return;
    }
    
    // Add block to blockchain
    if (validateAndAddBlock(block)) {
        DEO_LOG_INFO(BLOCKCHAIN, "Added incoming block to blockchain: " + block->calculateHash());
        
        // Remove transactions from mempool that are now in the block
        for (const auto& tx : block->getTransactions()) {
            mempool_->removeTransaction(tx->getId());
        }
    } else {
        DEO_LOG_WARNING(BLOCKCHAIN, "Failed to add incoming block to blockchain: " + block->calculateHash());
    }
}

bool NodeRuntime::startSync() {
    if (!p2p_network_ || !p2p_network_->isRunning()) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Cannot start sync: P2P network not running");
        return false;
    }
    
    if (!sync_manager_) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Cannot start sync: Sync manager not initialized (requires LevelDB storage)");
        return false;
    }
    
    try {
        if (sync_manager_->startSync()) {
            DEO_LOG_INFO(BLOCKCHAIN, "Chain synchronization started");
            return true;
        } else {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to start chain synchronization");
            return false;
        }
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Error starting sync: " + std::string(e.what()));
        return false;
    }
}

void NodeRuntime::stopSync() {
    if (!sync_manager_) {
        return;
    }
    
    try {
        sync_manager_->stopSync();
        DEO_LOG_INFO(BLOCKCHAIN, "Chain synchronization stopped");
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Error stopping sync: " + std::string(e.what()));
    }
}

sync::SyncStatus NodeRuntime::getSyncStatus() const {
    if (!sync_manager_) {
        return sync::SyncStatus::IDLE;
    }
    
    return sync_manager_->getSyncStatus();
}

double NodeRuntime::getSyncProgress() const {
    if (!sync_manager_) {
        return 0.0;
    }
    
    return sync_manager_->getSyncProgress();
}

} // namespace node
} // namespace deo
