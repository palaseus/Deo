/**
 * @file proof_of_authority.h
 * @brief Proof-of-Authority consensus mechanism implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "consensus_engine.h"
#include "core/block.h"

namespace deo {
namespace consensus {

/**
 * @brief Validator information for Proof-of-Authority
 */
struct Validator {
    std::string address;              ///< Validator address
    std::string public_key;           ///< Validator public key
    std::string name;                 ///< Validator name
    bool is_active;                   ///< Whether validator is active
    uint64_t stake;                   ///< Validator stake amount
    uint64_t last_block_time;         ///< Timestamp of last block produced
    uint32_t blocks_produced;         ///< Number of blocks produced
    uint32_t priority;                ///< Priority for round-robin selection
    
    /**
     * @brief Default constructor
     */
    Validator() = default;
    
    /**
     * @brief Constructor with parameters
     * @param addr Validator address
     * @param pub_key Public key
     * @param n Validator name
     * @param stake_amount Stake amount
     */
    Validator(const std::string& addr, const std::string& pub_key, const std::string& n, uint64_t stake_amount)
        : address(addr)
        , public_key(pub_key)
        , name(n)
        , is_active(true)
        , stake(stake_amount)
        , last_block_time(0)
        , blocks_produced(0)
        , priority(0) {}
    
    /**
     * @brief Serialize to JSON
     * @return JSON representation
     */
    nlohmann::json toJson() const;
    
    /**
     * @brief Deserialize from JSON
     * @param json JSON representation
     * @return True if successful
     */
    bool fromJson(const nlohmann::json& json);
};

/**
 * @brief Proof-of-Authority consensus engine
 * 
 * Implements a round-robin validator selection mechanism where
 * validators take turns producing blocks in a predetermined order.
 */
class ProofOfAuthority : public ConsensusEngine {
public:
    /**
     * @brief Default constructor
     */
    ProofOfAuthority();
    
    /**
     * @brief Destructor
     */
    ~ProofOfAuthority() override = default;
    
    // Disable copy and move semantics
    ProofOfAuthority(const ProofOfAuthority&) = delete;
    ProofOfAuthority& operator=(const ProofOfAuthority&) = delete;
    ProofOfAuthority(ProofOfAuthority&&) = delete;
    ProofOfAuthority& operator=(ProofOfAuthority&&) = delete;
    
    /**
     * @brief Initialize consensus engine
     * @param config Configuration parameters
     * @return True if successful
     */
    bool initialize() override;
    
    /**
     * @brief Shutdown consensus engine
     */
    void shutdown() override;
    
    /**
     * @brief Start consensus process
     * @param block Block to reach consensus on
     * @return Consensus result
     */
    ConsensusResult startConsensus(std::shared_ptr<core::Block> block) override;
    
    /**
     * @brief Validate block according to consensus rules
     * @param block Block to validate
     * @param previous_block Previous block in chain
     * @return Validation result
     */
    bool validateBlock(std::shared_ptr<core::Block> block) override;
    
    /**
     * @brief Get next validator for block production
     * @param current_height Current blockchain height
     * @return Validator address or empty string if none available
     */
    std::string getNextValidator(uint64_t current_height);
    
    /**
     * @brief Check if address is authorized to produce blocks
     * @param address Address to check
     * @return True if authorized
     */
    bool isAuthorizedValidator(const std::string& address);
    
    /**
     * @brief Get consensus parameters
     * @return JSON with consensus parameters
     */
    nlohmann::json getConsensusParameters() const;
    
    /**
     * @brief Add validator to the set
     * @param validator Validator to add
     * @return True if successful
     */
    bool addValidator(const Validator& validator);
    
    /**
     * @brief Remove validator from the set
     * @param address Validator address to remove
     * @return True if successful
     */
    bool removeValidator(const std::string& address);
    
    /**
     * @brief Get validator by address
     * @param address Validator address
     * @return Validator pointer or nullptr if not found
     */
    const Validator* getValidator(const std::string& address) const;
    
    /**
     * @brief Get all validators
     * @return Vector of validators
     */
    std::vector<Validator> getAllValidators() const;
    
    /**
     * @brief Get active validators
     * @return Vector of active validators
     */
    std::vector<Validator> getActiveValidators() const;
    
    /**
     * @brief Update validator stake
     * @param address Validator address
     * @param new_stake New stake amount
     * @return True if successful
     */
    bool updateValidatorStake(const std::string& address, uint64_t new_stake);
    
    /**
     * @brief Set validator active status
     * @param address Validator address
     * @param is_active Active status
     * @return True if successful
     */
    bool setValidatorActive(const std::string& address, bool is_active);
    
    /**
     * @brief Get block production time
     * @return Block time in seconds
     */
    uint64_t getBlockTime() const { return block_time_; }
    
    /**
     * @brief Set block production time
     * @param block_time Block time in seconds
     */
    void setBlockTime(uint64_t block_time) { block_time_ = block_time; }
    
    /**
     * @brief Get validator count
     * @return Number of validators
     */
    size_t getValidatorCount() const { return validators_.size(); }
    
    /**
     * @brief Get active validator count
     * @return Number of active validators
     */
    size_t getActiveValidatorCount() const;
    
    /**
     * @brief Get consensus statistics
     * @return JSON with statistics
     */
    std::string getStatistics() const override;

private:
    std::vector<Validator> validators_;           ///< List of validators
    mutable std::mutex mutex_;                   ///< Mutex for thread safety
    uint64_t block_time_;                        ///< Block production time in seconds
    uint64_t epoch_length_;                      ///< Epoch length for validator rotation
    std::string genesis_validator_;              ///< Genesis validator address
    
    /**
     * @brief Sort validators by priority
     */
    void sortValidators();
    
    /**
     * @brief Calculate validator priority
     * @param validator Validator to calculate priority for
     * @param current_height Current blockchain height
     * @return Priority value
     */
    uint32_t calculatePriority(const Validator& validator, uint64_t current_height) const;
    
    /**
     * @brief Check if validator can produce block
     * @param validator Validator to check
     * @param current_time Current timestamp
     * @return True if validator can produce block
     */
    bool canProduceBlock(const Validator& validator, uint64_t current_time) const;
    
    /**
     * @brief Load validators from configuration
     * @param config Configuration JSON
     * @return True if successful
     */
    bool loadValidatorsFromConfig(const nlohmann::json& config);
    
    /**
     * @brief Save validators to configuration
     * @return Configuration JSON
     */
    nlohmann::json saveValidatorsToConfig() const;
};

} // namespace consensus
} // namespace deo
