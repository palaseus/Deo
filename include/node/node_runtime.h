/**
 * @file node_runtime.h
 * @brief Long-running node runtime with persistent state and block production
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>

#include "core/blockchain.h"
#include "core/block.h"
#include "core/transaction.h"
#include "vm/vm_block_validator.h"
#include "vm/state_store.h"
#include "consensus/proof_of_work.h"
#include "consensus/consensus_engine.h"
#include "api/json_rpc_server.h"
#include "network/p2p_network_manager.h"
#include "sync/fast_sync.h"

namespace deo {
namespace node {

/**
 * @brief Transaction mempool for pending transactions
 */
class TransactionMempool {
public:
    /**
     * @brief Add transaction to mempool
     * @param transaction Transaction to add
     * @return True if added successfully
     */
    bool addTransaction(std::shared_ptr<core::Transaction> transaction);
    
    /**
     * @brief Remove transaction from mempool
     * @param tx_id Transaction ID to remove
     * @return True if removed successfully
     */
    bool removeTransaction(const std::string& tx_id);
    
    /**
     * @brief Get transactions for block production
     * @param max_count Maximum number of transactions to return
     * @return Vector of transactions
     */
    std::vector<std::shared_ptr<core::Transaction>> getTransactionsForBlock(size_t max_count = 100);
    
    /**
     * @brief Get mempool size
     * @return Number of transactions in mempool
     */
    size_t size() const;
    
    /**
     * @brief Clear mempool
     */
    void clear();
    
    /**
     * @brief Check if transaction exists in mempool
     * @param tx_id Transaction ID to check
     * @return True if exists
     */
    bool contains(const std::string& tx_id) const;

private:
    mutable std::mutex mempool_mutex_;
    std::map<std::string, std::shared_ptr<core::Transaction>> transactions_;
};

/**
 * @brief Node runtime configuration
 */
struct NodeConfig {
    std::string data_directory;          ///< Directory for blockchain data
    std::string state_directory;         ///< Directory for state data
    uint16_t p2p_port;                   ///< P2P listening port
    bool enable_p2p;                     ///< Whether to enable P2P networking
    bool enable_mining;                  ///< Whether to enable mining
    std::string storage_backend;         ///< Storage backend: "leveldb" or "json" (default: "json")
    uint32_t mining_difficulty;          ///< Mining difficulty target
    uint64_t block_gas_limit;            ///< Maximum gas per block
    uint64_t block_size_limit;           ///< Maximum block size in bytes
    uint32_t max_mempool_size;           ///< Maximum mempool size
    bool enable_json_rpc;                ///< Whether to enable JSON-RPC API
    uint16_t json_rpc_port;              ///< JSON-RPC listening port
    std::string json_rpc_host;           ///< JSON-RPC host address
    std::string json_rpc_username;         ///< JSON-RPC authentication username
    std::string json_rpc_password;       ///< JSON-RPC authentication password
    std::vector<std::string> bootstrap_nodes; ///< Bootstrap nodes for P2P
    
    /**
     * @brief Default constructor with default values
     */
    NodeConfig() 
        : data_directory("./data")
        , state_directory("./state")
        , p2p_port(8333)
        , enable_p2p(false)
        , enable_mining(false)
        , storage_backend("leveldb")  // Default to LevelDB for better performance
        , mining_difficulty(4)
        , block_gas_limit(10000000)
        , block_size_limit(1000000)
        , max_mempool_size(10000)
        , enable_json_rpc(true)
        , json_rpc_port(8545)
        , json_rpc_host("127.0.0.1")
        , json_rpc_username("")
        , json_rpc_password("")
    {}
};

/**
 * @brief Node runtime statistics
 */
struct NodeStatistics {
    uint64_t blocks_mined;               ///< Number of blocks mined
    uint64_t transactions_processed;     ///< Number of transactions processed
    uint64_t contracts_deployed;         ///< Number of contracts deployed
    uint64_t contracts_called;           ///< Number of contract calls
    uint64_t total_gas_used;             ///< Total gas consumed
    uint64_t mempool_size;               ///< Current mempool size
    uint64_t blockchain_height;          ///< Current blockchain height
    std::string best_block_hash;         ///< Best block hash
    bool is_mining;                      ///< Whether currently mining
    bool is_syncing;                     ///< Whether currently syncing
    sync::SyncStatus sync_status;        ///< Current sync status
    uint64_t sync_progress;              ///< Sync progress (0-100)
    uint64_t target_sync_height;         ///< Target height for sync
    uint64_t current_sync_height;        ///< Current sync height
    
    // Performance metrics
    double transactions_per_second;      ///< Average TPS over last period
    double avg_block_time_seconds;       ///< Average block production time
    double sync_speed_blocks_per_sec;     ///< Block synchronization speed
    uint64_t total_network_messages;     ///< Total network messages processed
    uint64_t total_storage_operations;    ///< Total storage read/write operations
};

/**
 * @brief Long-running node runtime
 * 
 * This class manages the full node runtime including:
 * - Persistent state management
 * - Transaction mempool
 * - Block production and validation
 * - VM integration for smart contracts
 * - P2P networking (placeholder)
 * - JSON-RPC API (placeholder)
 */
class NodeRuntime {
public:
    /**
     * @brief Constructor
     * @param config Node configuration
     */
    explicit NodeRuntime(const NodeConfig& config);
    
    /**
     * @brief Destructor
     */
    ~NodeRuntime();
    
    /**
     * @brief Initialize the node
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Start the node runtime
     * @return True if started successfully
     */
    bool start();
    
    /**
     * @brief Stop the node runtime
     */
    void stop();
    
    /**
     * @brief Check if node is running
     * @return True if running
     */
    bool isRunning() const;
    
    /**
     * @brief Add transaction to mempool
     * @param transaction Transaction to add
     * @return True if added successfully
     */
    bool addTransaction(std::shared_ptr<core::Transaction> transaction);
    
    /**
     * @brief Get contract information
     * @param address Contract address
     * @return Contract information as JSON string
     */
    std::string getContractInfo(const std::string& address);
    
    /**
     * @brief Get node statistics
     * @return Node statistics
     */
    NodeStatistics getStatistics() const;
    
    /**
     * @brief Get blockchain state
     * @return Blockchain state
     */
    core::BlockchainState getBlockchainState() const;
    
    /**
     * @brief Replay a block for debugging
     * @param block_hash Block hash to replay
     * @return Replay result as JSON string
     */
    std::string replayBlock(const std::string& block_hash);
    
    /**
     * @brief Get JSON-RPC server statistics
     * @return JSON-RPC statistics as JSON string
     */
    std::string getJsonRpcStatistics() const;
    
    /**
     * @brief Get a block by hash
     * @param hash Block hash
     * @return Block if found, nullptr otherwise
     */
    std::shared_ptr<core::Block> getBlock(const std::string& hash) const;
    
    /**
     * @brief Get a block by height
     * @param height Block height
     * @return Block if found, nullptr otherwise
     */
    std::shared_ptr<core::Block> getBlock(uint64_t height) const;
    
    /**
     * @brief Get a transaction by ID
     * @param tx_id Transaction ID
     * @return Transaction if found, nullptr otherwise
     */
    std::shared_ptr<core::Transaction> getTransaction(const std::string& tx_id) const;
    
    /**
     * @brief Get mempool transactions
     * @param max_count Maximum number of transactions to return (0 = all)
     * @return Vector of transactions
     */
    std::vector<std::shared_ptr<core::Transaction>> getMempoolTransactions(size_t max_count = 0) const;
    
    /**
     * @brief Get balance for an address
     * @param address Address to check
     * @return Balance in smallest unit
     */
    uint64_t getBalance(const std::string& address) const;
    
    /**
     * @brief Get mempool size
     * @return Number of transactions in mempool
     */
    size_t getMempoolSize() const;
    
    /**
     * @brief Get P2P network manager
     * @return Pointer to P2P network manager, or nullptr if not enabled
     */
    std::shared_ptr<network::P2PNetworkManager> getP2PNetworkManager() const;
    
    /**
     * @brief Broadcast transaction to network
     * @param transaction Transaction to broadcast
     */
    void broadcastTransaction(std::shared_ptr<core::Transaction> transaction);
    
    /**
     * @brief Broadcast block to network
     * @param block Block to broadcast
     */
    void broadcastBlock(std::shared_ptr<core::Block> block);
    
    /**
     * @brief Start chain synchronization
     * @return True if sync started successfully
     */
    bool startSync();
    
    /**
     * @brief Stop chain synchronization
     */
    void stopSync();
    
    /**
     * @brief Get sync status
     * @return Current sync status
     */
    sync::SyncStatus getSyncStatus() const;
    
    /**
     * @brief Get sync progress (0.0 to 1.0)
     * @return Sync progress
     */
    double getSyncProgress() const;

private:
    NodeConfig config_;                                  ///< Node configuration
    std::atomic<bool> running_;                          ///< Whether node is running
    std::atomic<bool> mining_enabled_;                   ///< Whether mining is enabled
    
    // Core components
    std::unique_ptr<core::Blockchain> blockchain_;       ///< Blockchain instance
    std::shared_ptr<vm::StateStore> state_store_;        ///< State store
    std::unique_ptr<vm::VMBlockValidator> block_validator_; ///< VM block validator
    std::shared_ptr<consensus::ConsensusEngine> consensus_engine_; ///< Consensus engine
    std::unique_ptr<api::JsonRpcServer> json_rpc_server_; ///< JSON-RPC API server
    
    // Networking
    std::shared_ptr<network::P2PNetworkManager> p2p_network_; ///< P2P network manager
    
    // Chain synchronization
    std::unique_ptr<sync::FastSyncManager> sync_manager_; ///< Chain synchronization manager
    
    // Mempool
    std::unique_ptr<TransactionMempool> mempool_;        ///< Transaction mempool
    
    // Threading
    std::thread block_production_thread_;                ///< Block production thread
    std::thread mempool_thread_;                         ///< Mempool management thread
    std::atomic<bool> stop_threads_;                     ///< Flag to stop threads
    
    // Synchronization
    mutable std::mutex runtime_mutex_;                   ///< Runtime mutex
    std::condition_variable block_production_cv_;        ///< Block production condition variable
    
    // Statistics
    mutable std::mutex stats_mutex_;                     ///< Statistics mutex
    NodeStatistics statistics_;                          ///< Node statistics
    
    // Performance tracking
    std::chrono::system_clock::time_point start_time_;   ///< Node start time
    std::chrono::system_clock::time_point last_block_time_; ///< Last block production time
    uint64_t transaction_count_window_;                   ///< Transaction count in current window
    std::chrono::system_clock::time_point tps_window_start_; ///< TPS calculation window start
    std::vector<double> recent_block_times_;              ///< Recent block production times (for average)
    static constexpr size_t MAX_RECENT_BLOCK_TIMES = 100; ///< Maximum recent block times to track
    
    /**
     * @brief Block production loop
     */
    void blockProductionLoop();
    
    /**
     * @brief Mempool management loop
     */
    void mempoolLoop();
    
    /**
     * @brief Create a new block
     * @return New block or nullptr if failed
     */
    std::shared_ptr<core::Block> createBlock();
    
    /**
     * @brief Mine a block
     * @param block Block to mine
     * @return True if mining was successful
     */
    bool mineBlock(std::shared_ptr<core::Block> block);
    
    /**
     * @brief Validate and add block to blockchain
     * @param block Block to add
     * @return True if added successfully
     */
    bool validateAndAddBlock(std::shared_ptr<core::Block> block);
    
    /**
     * @brief Update node statistics
     */
    void updateStatistics();
    
    /**
     * @brief Load blockchain from disk
     * @return True if loaded successfully
     */
    bool loadBlockchain();
    
    /**
     * @brief Save blockchain to disk
     * @return True if saved successfully
     */
    bool saveBlockchain();
    
    /**
     * @brief Initialize genesis block
     * @return True if initialized successfully
     */
    bool initializeGenesisBlock();
    
    /**
     * @brief Handle incoming transaction from network
     * @param transaction Transaction received from network
     */
    void handleIncomingTransaction(std::shared_ptr<core::Transaction> transaction);
    
    /**
     * @brief Handle incoming block from network
     * @param block Block received from network
     */
    void handleIncomingBlock(std::shared_ptr<core::Block> block);
};

} // namespace node
} // namespace deo

