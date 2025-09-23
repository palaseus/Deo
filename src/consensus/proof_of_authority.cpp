/**
 * @file proof_of_authority.cpp
 * @brief Proof-of-Authority consensus implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "consensus/proof_of_authority.h"
#include "crypto/hash.h"
#include "utils/logger.h"

#include <algorithm>
#include <chrono>

namespace deo {
namespace consensus {

nlohmann::json Validator::toJson() const {
    nlohmann::json json;
    
    json["address"] = address;
    json["public_key"] = public_key;
    json["name"] = name;
    json["is_active"] = is_active;
    json["stake"] = stake;
    json["last_block_time"] = last_block_time;
    json["blocks_produced"] = blocks_produced;
    json["priority"] = priority;
    
    return json;
}

bool Validator::fromJson(const nlohmann::json& json) {
    try {
        address = json["address"];
        public_key = json["public_key"];
        name = json["name"];
        is_active = json["is_active"];
        stake = json["stake"];
        last_block_time = json["last_block_time"];
        blocks_produced = json["blocks_produced"];
        priority = json["priority"];
        
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to deserialize validator: " + std::string(e.what()));
        return false;
    }
}

ProofOfAuthority::ProofOfAuthority() 
    : ConsensusEngine(ConsensusType::PROOF_OF_AUTHORITY)
    , block_time_(15)  // 15 seconds default block time
    , epoch_length_(100)  // 100 blocks per epoch
    , genesis_validator_("0x0000000000000000000000000000000000000000") {
    DEO_LOG_DEBUG(CONSENSUS, "ProofOfAuthority created");
}

bool ProofOfAuthority::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Add default validators
        addValidator(Validator("0x1234567890123456789012345678901234567890", 
                             "default_pub_key_1", "Validator1", 1000000));
        addValidator(Validator("0x2345678901234567890123456789012345678901", 
                             "default_pub_key_2", "Validator2", 1000000));
        addValidator(Validator("0x3456789012345678901234567890123456789012", 
                             "default_pub_key_3", "Validator3", 1000000));
        
        DEO_LOG_INFO(CONSENSUS, "ProofOfAuthority initialized with " + std::to_string(validators_.size()) + " validators");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to initialize ProofOfAuthority: " + std::string(e.what()));
        return false;
    }
}

void ProofOfAuthority::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    DEO_LOG_INFO(CONSENSUS, "ProofOfAuthority shutdown");
}

ConsensusResult ProofOfAuthority::startConsensus(std::shared_ptr<core::Block> block) {
    if (!block) {
        return ConsensusResult{false, "", {}, 0, "Block is null"};
    }
    
    // For PoA, consensus is reached if the block is valid
    if (validateBlock(block)) {
        return ConsensusResult{true, block->getHash(), {}, 
                              static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                  std::chrono::system_clock::now().time_since_epoch()).count()), ""};
    }
    
    return ConsensusResult{false, "", {}, 0, "Block validation failed"};
}

bool ProofOfAuthority::validateBlock(std::shared_ptr<core::Block> block) {
    if (!block) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Check if block is from authorized validator
        std::string block_producer = block->getHeader().previous_hash; // Simplified: using previous_hash as producer
        if (!isAuthorizedValidator(block_producer)) {
            return false;
        }
        
        // Validate block structure
        if (!block->validate()) {
            return false;
        }
        
        DEO_LOG_DEBUG(CONSENSUS, "Block validation successful for height " + std::to_string(block->getHeight()));
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Block validation error: " + std::string(e.what()));
        return false;
    }
}

std::string ProofOfAuthority::getNextValidator(uint64_t current_height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (validators_.empty()) {
        return "";
    }
    
    // Get active validators
    std::vector<Validator> active_validators;
    for (const auto& validator : validators_) {
        if (validator.is_active) {
            active_validators.push_back(validator);
        }
    }
    
    if (active_validators.empty()) {
        return "";
    }
    
    // Round-robin selection based on block height
    size_t validator_index = current_height % active_validators.size();
    
    // Update validator priority
    for (auto& validator : validators_) {
        if (validator.address == active_validators[validator_index].address) {
            validator.priority = current_height;
            break;
        }
    }
    
    DEO_LOG_DEBUG(CONSENSUS, "Selected validator: " + active_validators[validator_index].address + 
                 " for height " + std::to_string(current_height));
    
    return active_validators[validator_index].address;
}

bool ProofOfAuthority::isAuthorizedValidator(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& validator : validators_) {
        if (validator.address == address && validator.is_active) {
            return true;
        }
    }
    
    return false;
}

nlohmann::json ProofOfAuthority::getConsensusParameters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json params;
    
    params["consensus_type"] = "proof_of_authority";
    params["block_time"] = block_time_;
    params["epoch_length"] = epoch_length_;
    params["genesis_validator"] = genesis_validator_;
    params["validator_count"] = validators_.size();
    params["active_validator_count"] = getActiveValidatorCount();
    
    return params;
}

bool ProofOfAuthority::addValidator(const Validator& validator) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if validator already exists
    for (const auto& existing : validators_) {
        if (existing.address == validator.address) {
            DEO_LOG_WARNING(CONSENSUS, "Validator already exists: " + validator.address);
            return false;
        }
    }
    
    validators_.push_back(validator);
    sortValidators();
    
    DEO_LOG_INFO(CONSENSUS, "Added validator: " + validator.address);
    return true;
}

bool ProofOfAuthority::removeValidator(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(validators_.begin(), validators_.end(),
                          [&address](const Validator& v) { return v.address == address; });
    
    if (it != validators_.end()) {
        validators_.erase(it);
        DEO_LOG_INFO(CONSENSUS, "Removed validator: " + address);
        return true;
    }
    
    DEO_LOG_WARNING(CONSENSUS, "Validator not found: " + address);
    return false;
}

const Validator* ProofOfAuthority::getValidator(const std::string& address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& validator : validators_) {
        if (validator.address == address) {
            return &validator;
        }
    }
    
    return nullptr;
}

std::vector<Validator> ProofOfAuthority::getAllValidators() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return validators_;
}

std::vector<Validator> ProofOfAuthority::getActiveValidators() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Validator> active;
    for (const auto& validator : validators_) {
        if (validator.is_active) {
            active.push_back(validator);
        }
    }
    
    return active;
}

bool ProofOfAuthority::updateValidatorStake(const std::string& address, uint64_t new_stake) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& validator : validators_) {
        if (validator.address == address) {
            validator.stake = new_stake;
            sortValidators();
            DEO_LOG_INFO(CONSENSUS, "Updated validator stake: " + address + " -> " + std::to_string(new_stake));
            return true;
        }
    }
    
    return false;
}

bool ProofOfAuthority::setValidatorActive(const std::string& address, bool is_active) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& validator : validators_) {
        if (validator.address == address) {
            validator.is_active = is_active;
            DEO_LOG_INFO(CONSENSUS, "Set validator active status: " + address + " -> " + (is_active ? "true" : "false"));
            return true;
        }
    }
    
    return false;
}

size_t ProofOfAuthority::getActiveValidatorCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = 0;
    for (const auto& validator : validators_) {
        if (validator.is_active) {
            count++;
        }
    }
    
    return count;
}

std::string ProofOfAuthority::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    
    stats["total_validators"] = validators_.size();
    stats["active_validators"] = getActiveValidatorCount();
    stats["block_time"] = block_time_;
    stats["epoch_length"] = epoch_length_;
    
    nlohmann::json validator_stats = nlohmann::json::array();
    for (const auto& validator : validators_) {
        nlohmann::json v_stats;
        v_stats["address"] = validator.address;
        v_stats["name"] = validator.name;
        v_stats["is_active"] = validator.is_active;
        v_stats["stake"] = validator.stake;
        v_stats["blocks_produced"] = validator.blocks_produced;
        validator_stats.push_back(v_stats);
    }
    stats["validators"] = validator_stats;
    
    return stats.dump();
}

void ProofOfAuthority::sortValidators() {
    std::sort(validators_.begin(), validators_.end(),
              [](const Validator& a, const Validator& b) {
                  return a.priority < b.priority;
              });
}

uint32_t ProofOfAuthority::calculatePriority(const Validator& validator, uint64_t /* current_height */) const {
    // Simple priority calculation based on stake and last block time
    uint32_t priority = validator.stake / 1000;  // Normalize stake
    
    // Reduce priority for validators who recently produced blocks
    uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (validator.last_block_time > 0) {
        uint64_t time_since_last = current_time - validator.last_block_time;
        if (time_since_last < block_time_ * 2) {
            priority /= 2;  // Reduce priority for recent producers
        }
    }
    
    return priority;
}

bool ProofOfAuthority::canProduceBlock(const Validator& validator, uint64_t current_time) const {
    if (!validator.is_active) {
        return false;
    }
    
    if (validator.last_block_time > 0) {
        uint64_t time_since_last = current_time - validator.last_block_time;
        if (time_since_last < block_time_) {
            return false;
        }
    }
    
    return true;
}

bool ProofOfAuthority::loadValidatorsFromConfig(const nlohmann::json& config) {
    if (!config.contains("validators") || !config["validators"].is_array()) {
        return false;
    }
    
    validators_.clear();
    
    for (const auto& validator_json : config["validators"]) {
        Validator validator;
        if (validator.fromJson(validator_json)) {
            validators_.push_back(validator);
        }
    }
    
    return !validators_.empty();
}

nlohmann::json ProofOfAuthority::saveValidatorsToConfig() const {
    nlohmann::json config;
    
    nlohmann::json validators_json = nlohmann::json::array();
    for (const auto& validator : validators_) {
        validators_json.push_back(validator.toJson());
    }
    config["validators"] = validators_json;
    
    return config;
}

} // namespace consensus
} // namespace deo
