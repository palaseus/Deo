/**
 * @file consensus_synchronizer.h
 * @brief Consensus synchronization across multiple nodes
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <map>
#include <set>

#include "consensus_engine.h"
#include "core/blockchain.h"
#include "core/block.h"
#include "core/transaction.h"
#include "utils/logger.h"

namespace deo {
namespace consensus {

/**
 * @brief Fork choice rule implementation
 */
enum class ForkChoiceRule {
    LONGEST_CHAIN,      ///< Longest chain wins
    HEAVIEST_CHAIN,     ///< Chain with most work wins
    GHOST              ///< Greedy Heaviest Observed Subtree
};

/**
 * @brief Chain synchronization status
 */
struct SyncStatus {
    bool is_syncing;
    uint64_t current_height;
    uint64_t target_height;
    std::string best_block_hash;
    std::chrono::steady_clock::time_point last_sync_time;
    size_t peers_synced;
    
    SyncStatus() : is_syncing(false), current_height(0), target_height(0), peers_synced(0) {
        last_sync_time = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Consensus synchronizer for multi-node coordination
 */
class ConsensusSynchronizer {
public:
    ConsensusSynchronizer(std::shared_ptr<core::Blockchain> blockchain,
                         std::shared_ptr<ConsensusEngine> consensus_engine);
    ~ConsensusSynchronizer();
    
    // Initialization and lifecycle
    bool initialize();
    void shutdown();
    bool isRunning() const;
    
    // Fork choice and chain reorganization
    bool handleChainReorganization(const std::vector<std::shared_ptr<core::Block>>& new_blocks);
    std::shared_ptr<core::Block> findCommonAncestor(const std::string& block_hash1, const std::string& block_hash2);
    uint64_t calculateChainWeight(const std::string& block_hash);
    bool isOnMainChain(const std::string& block_hash);
    
    // Block validation and consensus
    bool validateBlockForConsensus(std::shared_ptr<core::Block> block);
    bool validateTransactionForConsensus(std::shared_ptr<core::Transaction> transaction);
    bool executeBlockTransactions(std::shared_ptr<core::Block> block);
    
    // Synchronization
    SyncStatus getSyncStatus() const;
    void startSynchronization();
    void stopSynchronization();
    bool isSynchronized() const;
    
    // Fork choice rule
    void setForkChoiceRule(ForkChoiceRule rule);
    ForkChoiceRule getForkChoiceRule() const;
    
    // Statistics
    struct ConsensusStats {
        uint64_t total_blocks_processed;
        uint64_t blocks_reorganized;
        uint64_t forks_detected;
        uint64_t consensus_violations;
        std::chrono::steady_clock::time_point start_time;
        double average_block_processing_time_ms;
    };
    
    ConsensusStats getConsensusStats() const;
    
    // Event handlers
    void setBlockHandler(std::function<void(std::shared_ptr<core::Block>)> handler);
    void setForkHandler(std::function<void(const std::string&, const std::string&)> handler);

private:
    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<ConsensusEngine> consensus_engine_;
    
    std::atomic<bool> running_;
    std::atomic<bool> syncing_;
    ForkChoiceRule fork_choice_rule_;
    
    mutable std::mutex sync_mutex_;
    SyncStatus sync_status_;
    
    mutable std::mutex stats_mutex_;
    ConsensusStats stats_;
    
    // Event handlers
    std::function<void(std::shared_ptr<core::Block>)> block_handler_;
    std::function<void(const std::string&, const std::string&)> fork_handler_;
    
    // Synchronization thread
    std::thread sync_thread_;
    
    // Internal methods
    void synchronizationLoop();
    bool shouldReorganizeChain(const std::string& new_block_hash);
    std::vector<std::shared_ptr<core::Block>> getBlocksToReorganize(const std::string& new_tip);
    void executeReorganization(const std::vector<std::shared_ptr<core::Block>>& blocks_to_remove,
                              const std::vector<std::shared_ptr<core::Block>>& blocks_to_add);
    void updateSyncStatus();
    void recordBlockProcessing(std::chrono::milliseconds processing_time);
    void recordForkDetection();
    void recordConsensusViolation();
};

/**
 * @brief Deterministic execution validator
 */
class DeterministicValidator {
public:
    DeterministicValidator();
    ~DeterministicValidator();
    
    // Validation methods
    bool validateBlockExecution(std::shared_ptr<core::Block> block);
    bool validateTransactionExecution(std::shared_ptr<core::Transaction> transaction);
    bool validateStateTransition(const std::string& previous_state_hash, 
                                const std::string& new_state_hash);
    
    // Determinism testing
    bool testDeterministicExecution(const std::vector<std::shared_ptr<core::Transaction>>& transactions);
    std::string calculateExecutionHash(const std::vector<std::shared_ptr<core::Transaction>>& transactions);
    
    // Statistics
    struct ValidationStats {
        uint64_t blocks_validated;
        uint64_t transactions_validated;
        uint64_t state_transitions_validated;
        uint64_t determinism_tests_passed;
        uint64_t determinism_tests_failed;
        std::chrono::steady_clock::time_point start_time;
    };
    
    ValidationStats getValidationStats() const;

private:
    mutable std::mutex stats_mutex_;
    ValidationStats stats_;
    
    void recordBlockValidation();
    void recordTransactionValidation();
    void recordStateTransitionValidation();
    void recordDeterminismTest(bool passed);
};

} // namespace consensus
} // namespace deo
