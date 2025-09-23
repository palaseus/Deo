/**
 * @file consensus_engine.h
 * @brief Consensus mechanism engine for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

#include "core/block.h"

namespace deo {
namespace consensus {

/**
 * @brief Consensus algorithm types
 */
enum class ConsensusType {
    PROOF_OF_WORK,      ///< Proof of Work (PoW)
    PROOF_OF_STAKE,     ///< Proof of Stake (PoS)
    DELEGATED_PROOF_OF_STAKE, ///< Delegated Proof of Stake (DPoS)
    PROOF_OF_AUTHORITY, ///< Proof of Authority (PoA)
    BYZANTINE_FAULT_TOLERANCE ///< Byzantine Fault Tolerance (BFT)
};

/**
 * @brief Consensus result
 */
struct ConsensusResult {
    bool success;                    ///< Whether consensus was reached
    std::string block_hash;          ///< Hash of the agreed block
    std::vector<std::string> votes;  ///< List of validator votes
    uint64_t timestamp;             ///< Consensus timestamp
    std::string error_message;      ///< Error message if consensus failed
};

/**
 * @brief Consensus engine interface
 * 
 * This class defines the interface for consensus mechanisms in the blockchain.
 * Different consensus algorithms can be implemented by inheriting from this class.
 */
class ConsensusEngine {
public:
    /**
     * @brief Constructor
     * @param type Consensus algorithm type
     */
    explicit ConsensusEngine(ConsensusType type);
    
    /**
     * @brief Destructor
     */
    virtual ~ConsensusEngine() = default;
    
    // Disable copy and move semantics
    ConsensusEngine(const ConsensusEngine&) = delete;
    ConsensusEngine& operator=(const ConsensusEngine&) = delete;
    ConsensusEngine(ConsensusEngine&&) = delete;
    ConsensusEngine& operator=(ConsensusEngine&&) = delete;

    /**
     * @brief Initialize the consensus engine
     * @return True if initialization was successful
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Shutdown the consensus engine
     */
    virtual void shutdown() = 0;
    
    /**
     * @brief Start consensus process
     * @param block Block to reach consensus on
     * @return Consensus result
     */
    virtual ConsensusResult startConsensus(std::shared_ptr<core::Block> block) = 0;
    
    /**
     * @brief Validate a block according to consensus rules
     * @param block Block to validate
     * @return True if block is valid
     */
    virtual bool validateBlock(std::shared_ptr<core::Block> block) = 0;
    
    /**
     * @brief Get the consensus type
     * @return Consensus type
     */
    ConsensusType getType() const { return type_; }
    
    /**
     * @brief Check if consensus is active
     * @return True if consensus is active
     */
    bool isActive() const { return is_active_; }
    
    /**
     * @brief Get consensus statistics
     * @return Consensus statistics as JSON string
     */
    virtual std::string getStatistics() const = 0;

protected:
    ConsensusType type_;             ///< Consensus algorithm type
    std::atomic<bool> is_active_;    ///< Whether consensus is active
    mutable std::mutex mutex_;       ///< Mutex for thread safety
};

} // namespace consensus
} // namespace deo
