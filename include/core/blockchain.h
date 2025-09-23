/**
 * @file blockchain.h
 * @brief Main blockchain class that manages the entire blockchain state
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>

#include "block.h"
#include "transaction.h"
#include "merkle_tree.h"
#include "consensus/consensus_engine.h"
#include "consensus/proof_of_work.h"
// #include "network/peer_manager.h" // Temporarily disabled
#include "storage/block_storage.h"
#include "storage/state_storage.h"
#include "vm/virtual_machine.h"
#include "utils/config.h"

namespace deo {
namespace core {

/**
 * @brief Blockchain state information
 */
struct BlockchainState {
    uint64_t height;                    ///< Current blockchain height
    std::string best_block_hash;        ///< Hash of the best block
    std::string genesis_hash;           ///< Hash of the genesis block
    uint64_t total_transactions;        ///< Total number of transactions
    uint64_t total_blocks;             ///< Total number of blocks
    uint64_t total_difficulty;         ///< Total accumulated difficulty
    std::chrono::system_clock::time_point last_block_time; ///< Time of last block
    bool is_syncing;                   ///< Whether the blockchain is syncing
    bool is_mining;                    ///< Whether mining is active
};

/**
 * @brief Blockchain configuration parameters
 */
struct BlockchainConfig {
    std::string network_id;            ///< Network identifier
    std::string data_directory;        ///< Directory for blockchain data
    uint32_t block_time;               ///< Target block time in seconds
    uint32_t max_block_size;           ///< Maximum block size in bytes
    uint32_t max_transactions_per_block; ///< Maximum transactions per block
    uint32_t difficulty_adjustment_interval; ///< Blocks between difficulty adjustments
    uint64_t genesis_timestamp;        ///< Genesis block timestamp
    std::string genesis_merkle_root;   ///< Genesis block merkle root
    uint32_t initial_difficulty;       ///< Initial mining difficulty
    bool enable_mining;                ///< Whether to enable mining
    bool enable_networking;            ///< Whether to enable networking
};

/**
 * @brief Main blockchain class that manages the entire blockchain state
 * 
 * The Blockchain class is the central component that manages all blockchain
 * operations including block validation, transaction processing, state management,
 * and consensus coordination.
 */
class Blockchain {
public:
    /**
     * @brief Constructor
     * @param config Blockchain configuration
     */
    explicit Blockchain(const BlockchainConfig& config);
    
    /**
     * @brief Destructor
     */
    ~Blockchain();
    
    // Disable copy and move semantics
    Blockchain(const Blockchain&) = delete;
    Blockchain& operator=(const Blockchain&) = delete;
    Blockchain(Blockchain&&) = delete;
    Blockchain& operator=(Blockchain&&) = delete;

    /**
     * @brief Initialize the blockchain
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Check if blockchain is initialized
     * @return True if blockchain is initialized
     */
    bool isInitialized() const;
    
    /**
     * @brief Shutdown the blockchain
     */
    void shutdown();
    
    /**
     * @brief Get the current blockchain state
     * @return Current blockchain state
     */
    BlockchainState getState() const;
    
    /**
     * @brief Get the blockchain configuration
     * @return Blockchain configuration
     */
    const BlockchainConfig& getConfig() const { return config_; }
    
    /**
     * @brief Add a new block to the blockchain
     * @param block Block to add
     * @return True if the block was added successfully
     */
    bool addBlock(std::shared_ptr<Block> block);
    
    /**
     * @brief Get a block by hash
     * @param hash Block hash
     * @return Block if found, nullptr otherwise
     */
    std::shared_ptr<Block> getBlock(const std::string& hash) const;
    
    /**
     * @brief Get a block by height
     * @param height Block height
     * @return Block if found, nullptr otherwise
     */
    std::shared_ptr<Block> getBlock(uint64_t height) const;
    
    /**
     * @brief Get the best (latest) block
     * @return Best block
     */
    std::shared_ptr<Block> getBestBlock() const;
    
    /**
     * @brief Get the genesis block
     * @return Genesis block
     */
    std::shared_ptr<Block> getGenesisBlock() const;
    
    /**
     * @brief Handle chain reorganization (fork choice rule)
     * @param new_block New block that might cause a reorg
     * @return True if reorg was successful
     */
    bool handleChainReorganization(std::shared_ptr<Block> new_block);
    
    /**
     * @brief Find the common ancestor between two blocks
     * @param block1 First block
     * @param block2 Second block
     * @return Common ancestor block
     */
    std::shared_ptr<Block> findCommonAncestor(std::shared_ptr<Block> block1, 
                                            std::shared_ptr<Block> block2) const;
    
    /**
     * @brief Get the chain weight (total difficulty) for a block
     * @param block Block to calculate weight for
     * @return Chain weight
     */
    uint64_t getChainWeight(std::shared_ptr<Block> block) const;
    
    /**
     * @brief Check if a block is on the main chain
     * @param block Block to check
     * @return True if block is on main chain
     */
    bool isOnMainChain(std::shared_ptr<Block> block) const;
    
    /**
     * @brief Add a transaction to the mempool
     * @param transaction Transaction to add
     * @return True if the transaction was added successfully
     */
    bool addTransaction(std::shared_ptr<Transaction> transaction);
    
    /**
     * @brief Get a transaction by ID
     * @param tx_id Transaction ID
     * @return Transaction if found, nullptr otherwise
     */
    std::shared_ptr<Transaction> getTransaction(const std::string& tx_id) const;
    
    /**
     * @brief Get transactions from the mempool
     * @param max_count Maximum number of transactions to return
     * @return List of transactions
     */
    std::vector<std::shared_ptr<Transaction>> getMempoolTransactions(size_t max_count = 0) const;
    
    /**
     * @brief Remove a transaction from the mempool
     * @param tx_id Transaction ID to remove
     * @return True if the transaction was removed
     */
    bool removeTransaction(const std::string& tx_id);
    
    /**
     * @brief Get the current blockchain height
     * @return Blockchain height
     */
    uint64_t getHeight() const;
    
    /**
     * @brief Get the total difficulty
     * @return Total accumulated difficulty
     */
    uint64_t getTotalDifficulty() const;
    
    /**
     * @brief Verify the entire blockchain
     * @return True if the blockchain is valid
     */
    bool verifyBlockchain() const;
    
    /**
     * @brief Get the balance of an address
     * @param address Address to check
     * @return Balance in smallest unit
     */
    uint64_t getBalance(const std::string& address) const;
    
    /**
     * @brief Get unspent transaction outputs for an address
     * @param address Address to check
     * @return List of unspent outputs
     */
    std::vector<TransactionOutput> getUnspentOutputs(const std::string& address) const;
    
    /**
     * @brief Start mining
     * @return True if mining started successfully
     */
    bool startMining();
    
    /**
     * @brief Stop mining
     */
    void stopMining();
    
    /**
     * @brief Check if mining is active
     * @return True if mining is active
     */
    bool isMining() const;
    
    /**
     * @brief Get mining statistics
     * @return Mining statistics
     */
    struct MiningStats {
        uint64_t blocks_mined;
        uint64_t hashes_per_second;
        uint64_t total_hashes;
        std::chrono::system_clock::time_point start_time;
    };
    MiningStats getMiningStats() const;
    
    /**
     * @brief Save the blockchain state to disk
     * @return True if save was successful
     */
    bool saveState();
    
    /**
     * @brief Load the blockchain state from disk
     * @return True if load was successful
     */
    bool loadState();
    
    /**
     * @brief Export the blockchain to a file
     * @param filename Output filename
     * @return True if export was successful
     */
    bool exportBlockchain(const std::string& filename) const;
    
    /**
     * @brief Import the blockchain from a file
     * @param filename Input filename
     * @return True if import was successful
     */
    bool importBlockchain(const std::string& filename);

private:
    BlockchainConfig config_;                                    ///< Blockchain configuration
    mutable std::mutex blockchain_mutex_;                        ///< Mutex for thread safety
    std::atomic<bool> is_initialized_;                          ///< Initialization flag
    std::atomic<bool> is_mining_;                               ///< Mining flag
    
    // Blockchain data
    std::vector<std::shared_ptr<Block>> blocks_;                ///< List of all blocks
    std::unordered_map<std::string, std::shared_ptr<Block>> block_index_; ///< Block hash to block mapping
    std::unordered_map<std::string, std::shared_ptr<Transaction>> mempool_; ///< Transaction mempool
    std::unordered_map<std::string, std::vector<TransactionOutput>> utxo_set_; ///< Unspent transaction outputs
    
    // State tracking
    std::atomic<uint64_t> current_height_;                      ///< Current blockchain height
    std::atomic<uint64_t> total_difficulty_;                    ///< Total accumulated difficulty
    std::string best_block_hash_;                               ///< Hash of the best block
    
    // Mining
    std::thread mining_thread_;                                 ///< Mining thread
    std::atomic<bool> stop_mining_;                             ///< Stop mining flag
    MiningStats mining_stats_;                                  ///< Mining statistics
    
    /**
     * @brief Create the genesis block
     * @return Genesis block
     */
    std::shared_ptr<Block> createGenesisBlock();
    
    /**
     * @brief Validate a block before adding it to the chain
     * @param block Block to validate
     * @return True if the block is valid
     */
    bool validateBlock(std::shared_ptr<Block> block) const;
    
    /**
     * @brief Update the UTXO set when a block is added
     * @param block Block that was added
     */
    void updateUTXOSet(std::shared_ptr<Block> block);
    
    /**
     * @brief Mining worker function
     */
    void miningWorker();
    
    /**
     * @brief Calculate the next difficulty target
     * @return New difficulty target
     */
    uint32_t calculateNextDifficulty() const;
    
    /**
     * @brief Reorganize the blockchain (handle forks)
     * @param new_best_block New best block
     * @return True if reorganization was successful
     */
    bool reorganizeChain(std::shared_ptr<Block> new_best_block);
    
    /**
     * @brief Validate transaction inputs against UTXO set
     * @param transaction Transaction to validate
     * @return True if inputs are valid
     */
    bool validateTransactionInputs(std::shared_ptr<Transaction> transaction) const;
    
    /**
     * @brief Process a block's transactions
     * @param block Block containing transactions
     * @return True if all transactions were processed successfully
     */
    bool processBlockTransactions(std::shared_ptr<Block> block);
    
private:
    // Core components
    std::unique_ptr<consensus::ConsensusEngine> consensus_engine_;
    // std::unique_ptr<network::PeerManager> peer_manager_; // Temporarily disabled
    std::unique_ptr<storage::BlockStorage> block_storage_;
    std::unique_ptr<storage::StateStorage> state_storage_;
    std::unique_ptr<vm::VirtualMachine> virtual_machine_;
    
};

} // namespace core
} // namespace deo
