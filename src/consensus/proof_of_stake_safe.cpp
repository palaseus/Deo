/**
 * @file proof_of_stake_safe.cpp
 * @brief Safe Proof-of-Stake consensus mechanism implementation
 * @author Deo Blockchain Research Team
 * @version 2.0.0
 */

#include "consensus/proof_of_stake.h"
#include "utils/logger.h"
#include "crypto/hash.h"
#include "crypto/signature.h"
#include "vm/uint256.h"

#include <algorithm>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <memory>
#include <mutex>

namespace deo {
namespace consensus {

// Safe constructor with proper initialization
ProofOfStake::ProofOfStake(deo::vm::uint256_t min_stake, uint32_t max_validators, 
                          uint64_t epoch_length, uint32_t slashing_percentage)
    : ConsensusEngine(ConsensusType::PROOF_OF_STAKE)
    , min_stake_(min_stake)
    , max_validators_(max_validators)
    , epoch_length_(epoch_length)
    , slashing_percentage_(slashing_percentage)
    , current_epoch_(0)
    , epoch_start_height_(0)
    , total_blocks_proposed_(0)
    , total_slashings_(0)
    , total_rewards_distributed_(0)
    , rng_(std::chrono::high_resolution_clock::now().time_since_epoch().count()) {
    
    // Initialize with safe defaults
    validators_.clear();
    delegations_.clear();
    slashing_history_.clear();
    current_validator_set_.clear();
    
    DEO_LOG_INFO(CONSENSUS, "ProofOfStake initialized with safe defaults");
}

ProofOfStake::~ProofOfStake() {
    // Safe cleanup - no dynamic allocations to clean up
    DEO_LOG_INFO(CONSENSUS, "ProofOfStake destroyed");
}

bool ProofOfStake::initialize() {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        DEO_LOG_INFO(CONSENSUS, "Initializing ProofOfStake consensus engine");
        
        // Initialize epoch
        current_epoch_ = 0;
        epoch_start_height_ = 0;
        
        // Clear any existing state safely
        validators_.clear();
        delegations_.clear();
        slashing_history_.clear();
        current_validator_set_.clear();
        
        // Initialize statistics
        total_blocks_proposed_ = 0;
        total_slashings_ = 0;
        total_rewards_distributed_ = 0;
        
        DEO_LOG_INFO(CONSENSUS, "ProofOfStake initialization completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "ProofOfStake initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool ProofOfStake::shutdown() {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        DEO_LOG_INFO(CONSENSUS, "Shutting down ProofOfStake consensus engine");
        
        // Safe cleanup
        validators_.clear();
        delegations_.clear();
        slashing_history_.clear();
        current_validator_set_.clear();
        
        DEO_LOG_INFO(CONSENSUS, "ProofOfStake shutdown completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "ProofOfStake shutdown failed: " + std::string(e.what()));
        return false;
    }
}

bool ProofOfStake::validateBlock(const std::shared_ptr<core::Block>& block) {
    if (!block) {
        DEO_LOG_ERROR(CONSENSUS, "Block validation failed: null block");
        return false;
    }
    
    try {
        // Basic block validation
        if (block->getHeight() < 0) {
            DEO_LOG_ERROR(CONSENSUS, "Block validation failed: invalid height");
            return false;
        }
        
        // Validate block structure
        if (block->getTransactions().empty()) {
            DEO_LOG_ERROR(CONSENSUS, "Block validation failed: no transactions");
            return false;
        }
        
        // Verify block cryptographic integrity
        if (!block->verify()) {
            DEO_LOG_ERROR(CONSENSUS, "Block validation failed: cryptographic verification failed");
            return false;
        }
        
        // Validate block height progression
        if (block->getHeight() <= epoch_start_height_) {
            DEO_LOG_ERROR(CONSENSUS, "Block validation failed: height not progressing");
            return false;
        }
        
        // Validate block timestamp (should be reasonable, not too far in future)
        uint64_t block_timestamp = block->getTimestamp();
        uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Allow up to 15 minutes in the future (clock skew tolerance)
        const uint64_t MAX_FUTURE_TIME = 900; // 15 minutes
        if (block_timestamp > current_time + MAX_FUTURE_TIME) {
            DEO_LOG_ERROR(CONSENSUS, "Block validation failed: timestamp too far in future");
            return false;
        }
        
        // Validate all transactions in the block
        for (const auto& tx : block->getTransactions()) {
            if (!tx) {
                DEO_LOG_ERROR(CONSENSUS, "Block validation failed: null transaction");
                return false;
            }
            
            // Basic transaction validation
            if (!tx->verify()) {
                DEO_LOG_ERROR(CONSENSUS, "Block validation failed: transaction verification failed");
                return false;
            }
            
            // Check transaction inputs and outputs are non-empty
            if (tx->getInputs().empty() && tx->getOutputs().empty()) {
                DEO_LOG_ERROR(CONSENSUS, "Block validation failed: empty transaction");
                return false;
            }
        }
        
        // Validate block hash matches calculated hash
        std::string calculated_hash = block->calculateHash();
        if (calculated_hash != block->getHash()) {
            DEO_LOG_ERROR(CONSENSUS, "Block validation failed: hash mismatch");
            return false;
        }
        
        DEO_LOG_INFO(CONSENSUS, "Block validation passed with comprehensive checks");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Block validation failed with exception: " + std::string(e.what()));
        return false;
    }
}

bool ProofOfStake::mineBlock(const std::shared_ptr<core::Block>& block) {
    if (!block) {
        DEO_LOG_ERROR(CONSENSUS, "Block mining failed: null block");
        return false;
    }
    
    try {
        // Set basic block properties
        block->setHeight(block->getHeight() + 1);
        block->setTimestamp(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        
        // Set previous hash (simplified)
        block->setPreviousHash("0000000000000000000000000000000000000000000000000000000000000000");
        
        // Update Merkle root
        block->updateMerkleRoot();
        
        // Increment statistics
        total_blocks_proposed_++;
        
        DEO_LOG_INFO(CONSENSUS, "Block mined successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Block mining failed with exception: " + std::string(e.what()));
        return false;
    }
}

bool ProofOfStake::startConsensus() {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        DEO_LOG_INFO(CONSENSUS, "Starting ProofOfStake consensus");
        
        // Validate current state
        if (validators_.empty()) {
            DEO_LOG_WARNING(CONSENSUS, "No validators registered, consensus may not function properly");
        }
        
        // Update validator set
        updateValidatorSet();
        
        DEO_LOG_INFO(CONSENSUS, "ProofOfStake consensus started successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to start consensus: " + std::string(e.what()));
        return false;
    }
}

std::string ProofOfStake::getStatistics() const {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        std::stringstream ss;
        ss << "ProofOfStake Statistics:\n";
        ss << "  Validators: " << validators_.size() << "\n";
        ss << "  Delegations: " << delegations_.size() << "\n";
        ss << "  Total Blocks Proposed: " << total_blocks_proposed_ << "\n";
        ss << "  Total Slashings: " << total_slashings_ << "\n";
        ss << "  Total Rewards Distributed: " << total_rewards_distributed_.toString() << "\n";
        ss << "  Current Epoch: " << current_epoch_ << "\n";
        ss << "  Epoch Start Height: " << epoch_start_height_ << "\n";
        
        return ss.str();
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to get statistics: " + std::string(e.what()));
        return "Error retrieving statistics";
    }
}

// Safe validator registration
bool ProofOfStake::registerValidator(const std::string& validator_address, 
                                   const std::string& public_key,
                                   const deo::vm::uint256_t& stake_amount) {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        // Validate inputs
        if (validator_address.empty() || public_key.empty()) {
            DEO_LOG_ERROR(CONSENSUS, "Validator registration failed: invalid address or public key");
            return false;
        }
        
        if (stake_amount < min_stake_) {
            DEO_LOG_ERROR(CONSENSUS, "Validator registration failed: insufficient stake");
            return false;
        }
        
        // Check if validator already exists
        if (validators_.find(validator_address) != validators_.end()) {
            DEO_LOG_WARNING(CONSENSUS, "Validator already registered: " + validator_address);
            return false;
        }
        
        // Create validator info
        ValidatorInfo validator_info;
        validator_info.address = validator_address;
        validator_info.public_key = public_key;
        validator_info.stake_amount = stake_amount;
        validator_info.is_active = true;
        validator_info.registration_time = std::chrono::system_clock::now();
        validator_info.blocks_proposed = 0;
        validator_info.slashing_count = 0;
        validator_info.total_rewards = deo::vm::uint256_t(0);
        
        // Add to validators map
        validators_[validator_address] = validator_info;
        
        DEO_LOG_INFO(CONSENSUS, "Validator registered successfully: " + validator_address);
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Validator registration failed with exception: " + std::string(e.what()));
        return false;
    }
}

// Safe validator info retrieval
std::optional<ValidatorInfo> ProofOfStake::getValidatorInfo(const std::string& validator_address) const {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        auto it = validators_.find(validator_address);
        if (it != validators_.end()) {
            return it->second;
        }
        return std::nullopt;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to get validator info: " + std::string(e.what()));
        return std::nullopt;
    }
}

// Safe stake delegation
bool ProofOfStake::delegateStake(const std::string& delegator_address,
                                const std::string& validator_address,
                                const deo::vm::uint256_t& stake_amount) {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        // Validate inputs
        if (delegator_address.empty() || validator_address.empty()) {
            DEO_LOG_ERROR(CONSENSUS, "Stake delegation failed: invalid addresses");
            return false;
        }
        
        if (stake_amount <= deo::vm::uint256_t(0)) {
            DEO_LOG_ERROR(CONSENSUS, "Stake delegation failed: invalid stake amount");
            return false;
        }
        
        // Check if validator exists
        auto validator_it = validators_.find(validator_address);
        if (validator_it == validators_.end()) {
            DEO_LOG_ERROR(CONSENSUS, "Stake delegation failed: validator not found");
            return false;
        }
        
        // Create delegation info
        DelegationInfo delegation_info;
        delegation_info.delegator_address = delegator_address;
        delegation_info.validator_address = validator_address;
        delegation_info.stake_amount = stake_amount;
        delegation_info.delegation_time = std::chrono::system_clock::now();
        delegation_info.is_active = true;
        
        // Add to delegations map
        std::string delegation_key = delegator_address + "_" + validator_address;
        delegations_[delegation_key] = delegation_info;
        
        // Update validator stake
        validator_it->second.stake_amount = validator_it->second.stake_amount + stake_amount;
        
        DEO_LOG_INFO(CONSENSUS, "Stake delegated successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Stake delegation failed with exception: " + std::string(e.what()));
        return false;
    }
}

// Safe stake undelegation
bool ProofOfStake::undelegateStake(const std::string& delegator_address,
                                  const std::string& validator_address,
                                  const deo::vm::uint256_t& stake_amount) {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        // Validate inputs
        if (delegator_address.empty() || validator_address.empty()) {
            DEO_LOG_ERROR(CONSENSUS, "Stake undelegation failed: invalid addresses");
            return false;
        }
        
        if (stake_amount <= deo::vm::uint256_t(0)) {
            DEO_LOG_ERROR(CONSENSUS, "Stake undelegation failed: invalid stake amount");
            return false;
        }
        
        // Find delegation
        std::string delegation_key = delegator_address + "_" + validator_address;
        auto delegation_it = delegations_.find(delegation_key);
        if (delegation_it == delegations_.end()) {
            DEO_LOG_ERROR(CONSENSUS, "Stake undelegation failed: delegation not found");
            return false;
        }
        
        // Check if delegation has sufficient stake
        if (delegation_it->second.stake_amount < stake_amount) {
            DEO_LOG_ERROR(CONSENSUS, "Stake undelegation failed: insufficient stake");
            return false;
        }
        
        // Update delegation
        delegation_it->second.stake_amount = delegation_it->second.stake_amount - stake_amount;
        
        // Update validator stake
        auto validator_it = validators_.find(validator_address);
        if (validator_it != validators_.end()) {
            validator_it->second.stake_amount = validator_it->second.stake_amount - stake_amount;
        }
        
        // Remove delegation if stake is zero
        if (delegation_it->second.stake_amount == deo::vm::uint256_t(0)) {
            delegations_.erase(delegation_it);
        }
        
        DEO_LOG_INFO(CONSENSUS, "Stake undelegated successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Stake undelegation failed with exception: " + std::string(e.what()));
        return false;
    }
}

// Safe validator set retrieval
std::vector<std::string> ProofOfStake::getValidatorSet() const {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        std::vector<std::string> validator_addresses;
        for (const auto& pair : validators_) {
            if (pair.second.is_active) {
                validator_addresses.push_back(pair.first);
            }
        }
        return validator_addresses;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to get validator set: " + std::string(e.what()));
        return {};
    }
}

// Safe total stake calculation
deo::vm::uint256_t ProofOfStake::getTotalStake() const {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        deo::vm::uint256_t total_stake(0);
        for (const auto& pair : validators_) {
            if (pair.second.is_active) {
                total_stake = total_stake + pair.second.stake_amount;
            }
        }
        return total_stake;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to calculate total stake: " + std::string(e.what()));
        return deo::vm::uint256_t(0);
    }
}

// Safe block proposer selection
std::string ProofOfStake::selectBlockProposer() {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        if (current_validator_set_.empty()) {
            DEO_LOG_ERROR(CONSENSUS, "No validators in current set");
            return "";
        }
        
        // Simple round-robin selection for now
        static size_t proposer_index = 0;
        proposer_index = (proposer_index + 1) % current_validator_set_.size();
        
        return current_validator_set_[proposer_index];
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to select block proposer: " + std::string(e.what()));
        return "";
    }
}

// Safe validator set update
void ProofOfStake::updateValidatorSet() {
    try {
        current_validator_set_.clear();
        
        for (const auto& pair : validators_) {
            if (pair.second.is_active) {
                current_validator_set_.push_back(pair.first);
            }
        }
        
        DEO_LOG_INFO(CONSENSUS, "Validator set updated with " + std::to_string(current_validator_set_.size()) + " validators");
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to update validator set: " + std::string(e.what()));
    }
}

// Safe reward calculation
deo::vm::uint256_t ProofOfStake::calculateRewards(const std::string& validator_address) const {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        auto validator_it = validators_.find(validator_address);
        if (validator_it == validators_.end()) {
            return deo::vm::uint256_t(0);
        }
        
        // Simple reward calculation based on stake
        deo::vm::uint256_t total_system_stake = getTotalStake();
        if (total_system_stake == deo::vm::uint256_t(0)) {
            return deo::vm::uint256_t(0);
        }
        
        // Calculate reward as percentage of total stake
        deo::vm::uint256_t reward = (validator_it->second.stake_amount * deo::vm::uint256_t(100)) / total_system_stake;
        
        return reward;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to calculate rewards: " + std::string(e.what()));
        return deo::vm::uint256_t(0);
    }
}

// Safe slashing
bool ProofOfStake::slashValidator(const std::string& validator_address, const std::string& reason) {
    std::lock_guard<std::mutex> lock(validators_mutex_);
    
    try {
        auto validator_it = validators_.find(validator_address);
        if (validator_it == validators_.end()) {
            DEO_LOG_ERROR(CONSENSUS, "Slashing failed: validator not found");
            return false;
        }
        
        // Calculate slashing amount
        deo::vm::uint256_t slashing_amount = (validator_it->second.stake_amount * deo::vm::uint256_t(slashing_percentage_)) / deo::vm::uint256_t(100);
        
        // Update validator stake
        validator_it->second.stake_amount = validator_it->second.stake_amount - slashing_amount;
        validator_it->second.slashing_count++;
        
        // Record slashing event
        SlashingEvent slashing_event;
        slashing_event.validator_address = validator_address;
        slashing_event.slashing_amount = slashing_amount;
        slashing_event.reason = reason;
        slashing_event.slashing_time = std::chrono::system_clock::now();
        
        slashing_history_.push_back(slashing_event);
        
        // Update statistics
        total_slashings_++;
        
        DEO_LOG_INFO(CONSENSUS, "Validator slashed: " + validator_address);
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Slashing failed with exception: " + std::string(e.what()));
        return false;
    }
}

} // namespace consensus
} // namespace deo
