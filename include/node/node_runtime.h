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
#include "api/json_rpc_server.h"

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
    bool enable_mining;                  ///< Whether to enable mining
    uint32_t mining_difficulty;          ///< Mining difficulty target
    uint64_t block_gas_limit;            ///< Maximum gas per block
    uint64_t block_size_limit;           ///< Maximum block size in bytes
    uint32_t max_mempool_size;           ///< Maximum mempool size
    bool enable_json_rpc;                ///< Whether to enable JSON-RPC API
    uint16_t json_rpc_port;              ///< JSON-RPC listening port
    std::vector<std::string> bootstrap_nodes; ///< Bootstrap nodes for P2P
    
    /**
     * @brief Default constructor with default values
     */
    NodeConfig() 
        : data_directory("./data")
        , state_directory("./state")
        , p2p_port(8333)
        , enable_mining(false)
        , mining_difficulty(4)
        , block_gas_limit(10000000)
        , block_size_limit(1000000)
        , max_mempool_size(10000)
        , enable_json_rpc(true)
        , json_rpc_port(8545)
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

private:
    NodeConfig config_;                                  ///< Node configuration
    std::atomic<bool> running_;                          ///< Whether node is running
    std::atomic<bool> mining_enabled_;                   ///< Whether mining is enabled
    
    // Core components
    std::unique_ptr<core::Blockchain> blockchain_;       ///< Blockchain instance
    std::shared_ptr<vm::StateStore> state_store_;        ///< State store
    std::unique_ptr<vm::VMBlockValidator> block_validator_; ///< VM block validator
    std::unique_ptr<consensus::ProofOfWork> pow_consensus_; ///< Proof of work consensus
    std::unique_ptr<api::JsonRpcServer> json_rpc_server_; ///< JSON-RPC API server
    
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
};

} // namespace node
} // namespace deo

