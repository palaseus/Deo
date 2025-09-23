/**
 * @file proof_of_work.h
 * @brief Proof of Work consensus implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdint>

#include "consensus_engine.h"
#include "core/block.h"

namespace deo {
namespace consensus {

/**
 * @brief Proof of Work consensus implementation
 * 
 * Implements the Proof of Work consensus mechanism where miners
 * compete to find a nonce that makes the block hash meet a certain
 * difficulty target.
 */
class ProofOfWork : public ConsensusEngine {
public:
    /**
     * @brief Constructor
     * @param initial_difficulty Initial mining difficulty
     * @param target_block_time Target time between blocks (seconds)
     */
    ProofOfWork(uint32_t initial_difficulty = 1, uint32_t target_block_time = 600);
    
    /**
     * @brief Destructor
     */
    ~ProofOfWork();
    
    /**
     * @brief Initialize the consensus engine
     * @return True if successful
     */
    bool initialize() override;
    
    /**
     * @brief Shutdown the consensus engine
     */
    void shutdown() override;
    
    /**
     * @brief Start consensus process for a block
     * @param block Block to mine
     * @return Consensus result
     */
    ConsensusResult startConsensus(std::shared_ptr<core::Block> block) override;
    
    /**
     * @brief Validate a block according to consensus rules
     * @param block Block to validate
     * @return True if block is valid
     */
    bool validateBlock(std::shared_ptr<core::Block> block) override;
    
    /**
     * @brief Start mining process
     * @return True if mining started successfully
     */
    bool startMining();
    
    /**
     * @brief Stop mining process
     */
    void stopMining();
    
    /**
     * @brief Check if currently mining
     * @return True if mining
     */
    bool isMining() const { return is_mining_; }
    
    /**
     * @brief Get current difficulty
     * @return Current difficulty
     */
    uint32_t getCurrentDifficulty() const { return current_difficulty_; }
    
    /**
     * @brief Set mining difficulty
     * @param difficulty New difficulty
     */
    void setDifficulty(uint32_t difficulty) { current_difficulty_ = difficulty; }
    
    /**
     * @brief Get target block time
     * @return Target block time in seconds
     */
    uint32_t getTargetBlockTime() const { return target_block_time_; }
    
    /**
     * @brief Set target block time
     * @param target_time Target time in seconds
     */
    void setTargetBlockTime(uint32_t target_time) { target_block_time_ = target_time; }
    
    /**
     * @brief Get mining statistics
     * @return JSON string with mining statistics
     */
    std::string getMiningStatistics() const;
    
    /**
     * @brief Get consensus statistics (override from base class)
     * @return JSON string with consensus statistics
     */
    std::string getStatistics() const override;
    
    /**
     * @brief Get hash rate (hashes per second)
     * @return Hash rate
     */
    double getHashRate() const;
    
    /**
     * @brief Get number of blocks mined
     * @return Number of blocks mined
     */
    uint64_t getBlocksMined() const { return blocks_mined_; }
    
    /**
     * @brief Get total hashes computed
     * @return Total hashes
     */
    uint64_t getTotalHashes() const { return total_hashes_; }
    
    /**
     * @brief Adjust difficulty based on block time
     * @param actual_block_time Actual time between blocks
     */
    void adjustDifficulty(uint32_t actual_block_time);
    
    /**
     * @brief Calculate target hash for given difficulty
     * @param difficulty Difficulty level
     * @return Target hash
     */
    std::string calculateTargetHash(uint32_t difficulty) const;
    
    /**
     * @brief Check if hash meets difficulty target
     * @param hash Hash to check
     * @param difficulty Difficulty level
     * @return True if hash meets target
     */
    bool meetsDifficultyTarget(const std::string& hash, uint32_t difficulty) const;
    
    /**
     * @brief Mine a single block
     * @param block Block to mine
     * @param max_nonce Maximum nonce to try
     * @return True if block was mined successfully
     */
    bool mineBlock(std::shared_ptr<core::Block> block, uint64_t max_nonce = UINT64_MAX);

private:
    uint32_t initial_difficulty_;      ///< Initial difficulty
    uint32_t current_difficulty_;      ///< Current difficulty
    uint32_t target_block_time_;       ///< Target block time in seconds
    
    std::atomic<bool> is_mining_;      ///< Mining status
    std::atomic<bool> stop_mining_;    ///< Stop mining flag
    
    std::thread mining_thread_;        ///< Mining thread
    std::mutex mining_mutex_;          ///< Mining mutex
    
    // Statistics
    std::atomic<uint64_t> blocks_mined_;      ///< Number of blocks mined
    std::atomic<uint64_t> total_hashes_;      ///< Total hashes computed
    std::atomic<uint64_t> hash_count_;        ///< Hash count for rate calculation
    
    std::chrono::system_clock::time_point mining_start_time_;  ///< Mining start time
    std::chrono::system_clock::time_point last_hash_time_;     ///< Last hash time
    mutable std::mutex stats_mutex_;                           ///< Statistics mutex
    
    /**
     * @brief Mining worker thread function
     * @param block Block to mine
     */
    void miningWorker(std::shared_ptr<core::Block> block);
    
    /**
     * @brief Calculate block hash with nonce
     * @param block Block to hash
     * @param nonce Nonce value
     * @return Block hash
     */
    std::string calculateBlockHash(std::shared_ptr<core::Block> block, uint32_t nonce) const;
    
    /**
     * @brief Update hash rate statistics
     */
    void updateHashRateStats();
    
    /**
     * @brief Convert difficulty to target value
     * @param difficulty Difficulty level
     * @return Target value
     */
    std::string difficultyToTarget(uint32_t difficulty) const;
    
    /**
     * @brief Convert target to difficulty
     * @param target Target value
     * @return Difficulty level
     */
    uint32_t targetToDifficulty(const std::string& target) const;
};

} // namespace consensus
} // namespace deo