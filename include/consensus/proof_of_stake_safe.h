/**
 * @file proof_of_stake_safe.h
 * @brief Safe Proof-of-Stake consensus mechanism implementation
 * @author Deo Blockchain Research Team
 * @version 2.0.0
 */

#pragma once

#include "consensus_engine.h"
#include "crypto/hash.h"
#include "crypto/signature.h"
#include "core/block.h"
#include "core/transaction.h"
#include "utils/logger.h"
#include "vm/uint256.h"

#include <memory>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <chrono>
#include <random>
#include <algorithm>
#include <optional>

namespace deo {
namespace consensus {

/**
 * @brief Validator information for Proof-of-Stake
 */
struct ValidatorInfo {
    std::string address;                    ///< Validator address
    std::string public_key;                 ///< Validator public key
    deo::vm::uint256_t stake_amount;        ///< Amount of stake
    deo::vm::uint256_t delegated_stake;     ///< Amount delegated to this validator
    uint64_t commission_rate;               ///< Commission rate (basis points)
    bool is_active;                         ///< Whether validator is active
    uint64_t last_block_height;             ///< Last block produced
    uint64_t slashing_count;                ///< Number of slashing events
    deo::vm::uint256_t total_rewards;        ///< Total rewards earned
    std::chrono::system_clock::time_point registration_time; ///< Registration timestamp
    std::chrono::system_clock::time_point last_activity; ///< Last activity timestamp
    uint64_t blocks_proposed;               ///< Number of blocks proposed
    
    ValidatorInfo() : stake_amount(0), delegated_stake(0), commission_rate(0), 
                      is_active(false), last_block_height(0), slashing_count(0),
                      total_rewards(0), blocks_proposed(0) {}
};

/**
 * @brief Delegation information
 */
struct DelegationInfo {
    std::string delegator_address;          ///< Address of delegator
    std::string validator_address;          ///< Address of validator
    deo::vm::uint256_t stake_amount;        ///< Delegated amount
    uint64_t start_height;                  ///< Height when delegation started
    bool is_active;                         ///< Whether delegation is active
    std::chrono::system_clock::time_point delegation_time; ///< Delegation timestamp
    
    DelegationInfo() : stake_amount(0), start_height(0), is_active(false) {}
};

/**
 * @brief Slashing event information
 */
struct SlashingEvent {
    std::string validator_address;          ///< Address of slashed validator
    std::string reason;                     ///< Reason for slashing
    std::string evidence;                   ///< Evidence of misbehavior
    deo::vm::uint256_t slashed_amount;      ///< Amount slashed
    uint64_t block_height;                  ///< Block height when slashing occurred
    std::chrono::system_clock::time_point slashing_time; ///< Timestamp of slashing
    
    SlashingEvent() : slashed_amount(0), block_height(0) {}
};

/**
 * @brief Safe Proof-of-Stake consensus engine
 */
class ProofOfStake : public ConsensusEngine {
public:
    /**
     * @brief Constructor
     * @param min_stake Minimum stake required to become validator
     * @param max_validators Maximum number of validators
     * @param epoch_length Length of each epoch in blocks
     * @param slashing_percentage Percentage of stake to slash for violations
     */
    ProofOfStake(deo::vm::uint256_t min_stake = deo::vm::uint256_t(1000000), 
                 uint32_t max_validators = 100,
                 uint64_t epoch_length = 100,
                 uint32_t slashing_percentage = 5);
    
    /**
     * @brief Destructor
     */
    ~ProofOfStake() override;
    
    // ConsensusEngine interface
    bool initialize() override;
    void shutdown() override;
    bool validateBlock(std::shared_ptr<core::Block> block) override;
    ConsensusResult startConsensus(std::shared_ptr<core::Block> block) override;
    std::string getStatistics() const override;
    
    // Proof-of-Stake specific methods
    
    /**
     * @brief Mine a new block (PoS specific)
     * @param transactions Transactions to include in the block
     * @return New block or nullptr if mining failed
     */
    std::shared_ptr<core::Block> mineBlock(const std::vector<std::shared_ptr<core::Transaction>>& transactions);
    
    /**
     * @brief Get mining difficulty (PoS specific)
     * @return Mining difficulty
     */
    uint64_t getMiningDifficulty() const;
    
    /**
     * @brief Get hash rate (PoS specific)
     * @return Hash rate
     */
    double getHashRate() const;
    
    /**
     * @brief Get total hashes (PoS specific)
     * @return Total hashes
     */
    uint64_t getTotalHashes() const;
    
    /**
     * @brief Start consensus process (PoS specific)
     */
    void startConsensus();
    
    /**
     * @brief Stop consensus process (PoS specific)
     */
    void stopConsensus();
    
    /**
     * @brief Register a validator
     * @param validator_address Address of the validator
     * @param public_key Public key of the validator
     * @param stake_amount Amount of stake to lock
     * @return True if registration successful
     */
    bool registerValidator(const std::string& validator_address,
                          const std::string& public_key,
                          deo::vm::uint256_t stake_amount);
    
    /**
     * @brief Delegate stake to a validator
     * @param delegator_address Address of the delegator
     * @param validator_address Address of the validator
     * @param amount Amount to delegate
     * @return True if delegation successful
     */
    bool delegateStake(const std::string& delegator_address,
                      const std::string& validator_address,
                      deo::vm::uint256_t amount);
    
    /**
     * @brief Undelegate stake from a validator
     * @param delegator_address Address of the delegator
     * @param validator_address Address of the validator
     * @param amount Amount to undelegate
     * @return True if undelegation successful
     */
    bool undelegateStake(const std::string& delegator_address,
                        const std::string& validator_address,
                        deo::vm::uint256_t amount);
    
    /**
     * @brief Get validator information
     * @param validator_address Address of the validator
     * @return Validator information or nullopt if not found
     */
    std::optional<ValidatorInfo> getValidatorInfo(const std::string& validator_address) const;
    
    /**
     * @brief Get all active validators
     * @return Vector of active validator addresses
     */
    std::vector<std::string> getActiveValidators() const;
    
    /**
     * @brief Get validator set for current epoch
     * @return Vector of validator addresses in selection order
     */
    std::vector<std::string> getValidatorSet() const;
    
    /**
     * @brief Select next block proposer
     * @return Address of selected proposer
     */
    std::string selectBlockProposer();
    
    /**
     * @brief Slash a validator for misbehavior
     * @param validator_address Address of validator to slash
     * @param reason Reason for slashing
     * @param evidence Evidence of misbehavior
     * @return True if slashing successful
     */
    bool slashValidator(const std::string& validator_address,
                       const std::string& reason,
                       const std::string& evidence);
    
    /**
     * @brief Get total stake in the system
     * @return Total stake amount
     */
    deo::vm::uint256_t getTotalStake() const;
    
    /**
     * @brief Get stake distribution
     * @return Map of validator addresses to their total stake
     */
    std::map<std::string, deo::vm::uint256_t> getStakeDistribution() const;
    
    /**
     * @brief Get delegation information
     * @param delegator_address Address of the delegator
     * @return Vector of delegation information
     */
    std::vector<DelegationInfo> getDelegations(const std::string& delegator_address) const;
    
    /**
     * @brief Get slashing history
     * @param validator_address Address of the validator (empty for all)
     * @return Vector of slashing events
     */
    std::vector<SlashingEvent> getSlashingHistory(const std::string& validator_address = "") const;
    
    /**
     * @brief Update epoch (called at end of each epoch)
     * @param new_height New block height
     */
    void updateEpoch(uint64_t new_height);
    
    /**
     * @brief Calculate rewards for validators
     * @param validator_address Address of validator
     * @return Reward amount for validator
     */
    deo::vm::uint256_t calculateRewards(const std::string& validator_address) const;
    
    /**
     * @brief Distribute rewards to validators and delegators
     * @param rewards Map of validator addresses to reward amounts
     * @return True if distribution successful
     */
    bool distributeRewards(const std::map<std::string, deo::vm::uint256_t>& rewards);

private:
    // Configuration parameters
    deo::vm::uint256_t min_stake_;          ///< Minimum stake to become validator
    uint32_t max_validators_;               ///< Maximum number of validators
    uint64_t epoch_length_;                 ///< Length of each epoch
    uint32_t slashing_percentage_;          ///< Percentage of stake to slash
    
    // State management
    mutable std::mutex validators_mutex_;   ///< Mutex for validators map
    std::map<std::string, ValidatorInfo> validators_; ///< Validator registry
    
    mutable std::mutex delegations_mutex_;  ///< Mutex for delegations map
    std::map<std::string, DelegationInfo> delegations_; ///< Delegation registry
    
    mutable std::mutex slashing_mutex_;     ///< Mutex for slashing events
    std::vector<SlashingEvent> slashing_history_; ///< Slashing history
    
    // Epoch management
    uint64_t current_epoch_;                ///< Current epoch number
    uint64_t epoch_start_height_;           ///< Block height when current epoch started
    std::vector<std::string> current_validator_set_; ///< Current validator set
    
    // Random number generation for proposer selection
    mutable std::mt19937_64 rng_;           ///< Random number generator
    
    // Statistics
    uint64_t total_blocks_proposed_;        ///< Total blocks proposed
    uint64_t total_slashings_;              ///< Total slashings performed
    deo::vm::uint256_t total_rewards_distributed_; ///< Total rewards distributed
    
    /**
     * @brief Update validator set for new epoch
     */
    void updateValidatorSet();
    
    /**
     * @brief Validate validator registration
     * @param validator_address Address of validator
     * @param stake_amount Amount of stake
     * @return True if registration is valid
     */
    bool validateValidatorRegistration(const std::string& validator_address, 
                                      deo::vm::uint256_t stake_amount) const;
    
    /**
     * @brief Validate delegation
     * @param delegator_address Address of delegator
     * @param validator_address Address of validator
     * @param amount Amount to delegate
     * @return True if delegation is valid
     */
    bool validateDelegation(const std::string& delegator_address,
                           const std::string& validator_address,
                           deo::vm::uint256_t amount) const;
    
    /**
     * @brief Check if validator should be slashed
     * @param validator_address Address of validator
     * @param block Block to check
     * @return True if validator should be slashed
     */
    bool shouldSlashValidator(const std::string& validator_address, 
                             const core::Block& block) const;
    
    /**
     * @brief Generate seed for proposer selection
     * @return Seed for random selection
     */
    uint64_t generateSelectionSeed() const;
    
    /**
     * @brief Update validator statistics
     * @param validator_address Address of validator
     * @param block_height Height of block produced
     */
    void updateValidatorStats(const std::string& validator_address, uint64_t block_height);
    
    /**
     * @brief Calculate commission for validator
     * @param validator_info Validator information
     * @param reward_amount Total reward amount
     * @return Commission amount
     */
    deo::vm::uint256_t calculateCommission(const ValidatorInfo& validator_info, deo::vm::uint256_t reward_amount) const;
    
    /**
     * @brief Calculate delegator reward
     * @param delegation Delegation information
     * @param validator_info Validator information
     * @param total_reward Total reward for validator
     * @return Delegator reward amount
     */
    deo::vm::uint256_t calculateDelegatorReward(const DelegationInfo& delegation,
                                               const ValidatorInfo& validator_info,
                                               deo::vm::uint256_t total_reward) const;
};

} // namespace consensus
} // namespace deo
