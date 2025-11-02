/**
 * @file proof_of_work.cpp
 * @brief Proof of Work consensus implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "consensus/proof_of_work.h"
#include "crypto/hash.h"
#include "utils/logger.h"
#include "vm/uint256.h"
#include "utils/error_handler.h"

#include <random>
#include <chrono>
#include <iostream>
#include <thread>
#include <sstream>
#include <iomanip>

namespace deo {
namespace consensus {

ProofOfWork::ProofOfWork(uint32_t initial_difficulty, uint32_t target_block_time)
    : ConsensusEngine(ConsensusType::PROOF_OF_WORK)
    , initial_difficulty_(initial_difficulty)
    , current_difficulty_(initial_difficulty)
    , target_block_time_(target_block_time)
    , is_mining_(false)
    , stop_mining_(false)
    , blocks_mined_(0)
    , total_hashes_(0)
    , hash_count_(0) {
    
    DEO_LOG_DEBUG(CONSENSUS, "Proof of Work consensus created with difficulty: " + 
                  std::to_string(initial_difficulty) + ", target block time: " + 
                  std::to_string(target_block_time) + "s");
}

ProofOfWork::~ProofOfWork() {
    stopMining();
}

bool ProofOfWork::initialize() {
    DEO_LOG_INFO(CONSENSUS, "Initializing Proof of Work consensus");
    
    if (!ConsensusEngine::initialize()) {
        return false;
    }
    
    mining_start_time_ = std::chrono::system_clock::now();
    last_hash_time_ = mining_start_time_;
    hash_count_ = 0;
    
    DEO_LOG_INFO(CONSENSUS, "Proof of Work consensus initialized successfully");
    return true;
}

void ProofOfWork::shutdown() {
    DEO_LOG_INFO(CONSENSUS, "Shutting down Proof of Work consensus");
    
    stopMining();
    ConsensusEngine::shutdown();
    
    DEO_LOG_INFO(CONSENSUS, "Proof of Work consensus shutdown complete");
}

ConsensusResult ProofOfWork::startConsensus(std::shared_ptr<core::Block> block) {
    DEO_LOG_DEBUG(CONSENSUS, "Starting Proof of Work consensus for block");
    
    ConsensusResult result;
    result.success = false;
    result.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (!block) {
        result.error_message = "Block is null";
        return result;
    }
    
    // Try to mine the block if it's not already mined
    if (!validateBlock(block)) {
        DEO_LOG_DEBUG(CONSENSUS, "Block not mined yet, attempting to mine");
        if (!mineBlock(block, 10000)) { // Try mining with max 10000 nonces
            result.error_message = "Failed to mine block";
            return result;
        }
    }
    
    // Validate the mined block
    if (!validateBlock(block)) {
        result.error_message = "Block validation failed after mining";
        return result;
    }
    
    // Start mining
    if (startMining()) {
        result.success = true;
        result.block_hash = block->calculateHash();
        result.votes = {"miner1", "miner2"}; // Mock votes for testing
        DEO_LOG_INFO(CONSENSUS, "Proof of Work consensus started successfully");
    } else {
        result.error_message = "Failed to start mining";
    }
    
    return result;
}

bool ProofOfWork::validateBlock(std::shared_ptr<core::Block> block) {
    DEO_LOG_DEBUG(CONSENSUS, "Validating block for Proof of Work");
    
    if (!block) {
        DEO_ERROR(CONSENSUS, "Block is null");
        return false;
    }
    
    // Validate block structure
    if (!block->verify()) {
        DEO_ERROR(CONSENSUS, "Block verification failed");
        return false;
    }
    
    // Skip difficulty validation for genesis blocks (height 0)
    // Genesis blocks are special and don't require proof of work
    if (block->getHeight() == 0 || block->isGenesis()) {
        DEO_LOG_DEBUG(CONSENSUS, "Genesis block - skipping difficulty validation");
        return true;
    }
    
    // Validate proof of work for non-genesis blocks
    // Use the same hash calculation as mining (includes nonce)
    std::string block_hash = calculateBlockHash(block, block->getNonce());
    std::string target_hash = calculateTargetHash(current_difficulty_);
    DEO_LOG_DEBUG(CONSENSUS, "Validating block hash: " + block_hash);
    DEO_LOG_DEBUG(CONSENSUS, "Target hash: " + target_hash);
    DEO_LOG_DEBUG(CONSENSUS, "Difficulty: " + std::to_string(current_difficulty_));
    
    if (!meetsDifficultyTarget(block_hash, current_difficulty_)) {
        DEO_ERROR(CONSENSUS, "Block does not meet difficulty target");
        return false;
    }
    
    DEO_LOG_DEBUG(CONSENSUS, "Block validation successful");
    return true;
}

bool ProofOfWork::startMining() {
    DEO_LOG_INFO(CONSENSUS, "Starting mining");
    
    if (is_mining_) {
        DEO_WARNING(CONSENSUS, "Mining is already active");
        return true;
    }
    
    is_mining_ = true;
    stop_mining_ = false;
    
    DEO_LOG_INFO(CONSENSUS, "Mining started successfully");
    return true;
}

void ProofOfWork::stopMining() {
    DEO_LOG_INFO(CONSENSUS, "Stopping mining");
    
    stop_mining_ = true;
    is_mining_ = false;
    
    if (mining_thread_.joinable()) {
        mining_thread_.join();
    }
    
    DEO_LOG_INFO(CONSENSUS, "Mining stopped");
}

std::string ProofOfWork::getMiningStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"type\": \"proof_of_work\",\n";
    ss << "  \"active\": " << (is_active_ ? "true" : "false") << ",\n";
    ss << "  \"mining\": " << (is_mining_ ? "true" : "false") << ",\n";
    ss << "  \"current_difficulty\": " << current_difficulty_ << ",\n";
    ss << "  \"target_block_time\": " << target_block_time_ << ",\n";
    ss << "  \"blocks_mined\": " << blocks_mined_ << ",\n";
    ss << "  \"total_hashes\": " << total_hashes_ << ",\n";
    ss << "  \"hash_rate\": " << std::fixed << std::setprecision(2) << getHashRate() << ",\n";
    ss << "  \"timestamp\": " << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "\n";
    ss << "}";
    
    return ss.str();
}

std::string ProofOfWork::getStatistics() const {
    return getMiningStatistics();
}

double ProofOfWork::getHashRate() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - mining_start_time_);
    
    if (duration.count() == 0) {
        return 0.0;
    }
    
    return static_cast<double>(total_hashes_) / duration.count();
}

void ProofOfWork::adjustDifficulty(uint32_t actual_block_time) {
    uint32_t old_difficulty = current_difficulty_;
    
    if (actual_block_time < target_block_time_ / 2) {
        // Blocks are coming too fast, increase difficulty
        current_difficulty_ = std::min(current_difficulty_ * 2, UINT32_MAX);
    } else if (actual_block_time > target_block_time_ * 2) {
        // Blocks are coming too slow, decrease difficulty
        current_difficulty_ = std::max(current_difficulty_ / 2, 1U);
    } else if (actual_block_time < target_block_time_) {
        // Blocks are slightly faster than target, increase difficulty slightly
        current_difficulty_ = std::min(current_difficulty_ + 1, UINT32_MAX);
    } else if (actual_block_time > target_block_time_) {
        // Blocks are slightly slower than target, decrease difficulty slightly
        current_difficulty_ = std::max(current_difficulty_ - 1, 1U);
    }
    
    if (current_difficulty_ != old_difficulty) {
        DEO_LOG_INFO(CONSENSUS, "Adjusted difficulty from " + std::to_string(old_difficulty) + 
                      " to: " + std::to_string(current_difficulty_));
    }
}

std::string ProofOfWork::calculateTargetHash(uint32_t difficulty) const {
    // Calculate target hash based on difficulty
    // This is a simplified implementation
    std::string target = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    
    if (difficulty > 0) {
        size_t leading_zeros = std::min(difficulty, 64U);
        for (size_t i = 0; i < leading_zeros; ++i) {
            target[i] = '0';
        }
        // For difficulty 1, we want any hash starting with '0' to be valid
        // So the target should be "0fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        if (leading_zeros < 64) {
            target[leading_zeros] = 'f';
        }
    }
    
    return target;
}

bool ProofOfWork::meetsDifficultyTarget(const std::string& hash, uint32_t difficulty) const {
    std::string target_hash = calculateTargetHash(difficulty);
    
    // Convert hex strings to uint256_t for proper numerical comparison
    try {
        vm::uint256_t hash_value(hash);
        vm::uint256_t target_value(target_hash);
        // For difficulty, lower hash values are better (meet higher difficulty)
        // Hash is valid if it's less than or equal to the target
        return hash_value <= target_value;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(CONSENSUS, "Failed to parse hash for difficulty comparison: " + std::string(e.what()));
        return false;
    }
}

bool ProofOfWork::mineBlock(std::shared_ptr<core::Block> block, uint64_t max_nonce) {
    DEO_LOG_INFO(CONSENSUS, "Starting to mine block");
    
    if (!block) {
        DEO_ERROR(CONSENSUS, "Block is null");
        return false;
    }
    
    std::string target_hash = calculateTargetHash(current_difficulty_);
    uint32_t nonce = 0;
    
    DEO_LOG_DEBUG(CONSENSUS, "Mining with target: " + target_hash);
    DEO_LOG_DEBUG(CONSENSUS, "Max nonce: " + std::to_string(max_nonce));
    DEO_LOG_DEBUG(CONSENSUS, "Current difficulty: " + std::to_string(current_difficulty_));
    
    while (nonce < max_nonce && !stop_mining_) {
        std::string block_hash = calculateBlockHash(block, nonce);
        total_hashes_++;
        hash_count_++;
        
        // Debug output for first few attempts
        if (nonce < 10) {
            DEO_LOG_DEBUG(CONSENSUS, "Nonce " + std::to_string(nonce) + " hash: " + block_hash);
        }
        
        if (meetsDifficultyTarget(block_hash, current_difficulty_)) {
            // Found a valid hash
            block->setNonce(nonce);
            blocks_mined_++;
            
            DEO_LOG_INFO(CONSENSUS, "Block mined successfully with nonce: " + std::to_string(nonce));
            DEO_LOG_INFO(CONSENSUS, "Mined hash: " + block_hash);
            DEO_LOG_INFO(CONSENSUS, "Target hash: " + target_hash);
            return true;
        }
        
        // Debug: Check if we're close to finding a valid hash
        if (nonce < 100 && block_hash[0] == '0') {
            DEO_LOG_DEBUG(CONSENSUS, "Found hash starting with 0 at nonce " + std::to_string(nonce) + ": " + block_hash);
        }
        
        nonce++;
        
        // Update hash rate statistics periodically
        if (hash_count_ % 10000 == 0) {
            updateHashRateStats();
        }
    }
    
    DEO_LOG_WARNING(CONSENSUS, "Mining stopped without finding valid hash");
    return false;
}

void ProofOfWork::miningWorker(std::shared_ptr<core::Block> block) {
    DEO_LOG_DEBUG(CONSENSUS, "Mining worker started");
    
    mineBlock(block);
    
    DEO_LOG_DEBUG(CONSENSUS, "Mining worker finished");
}

std::string ProofOfWork::calculateBlockHash(std::shared_ptr<core::Block> block, uint32_t nonce) const {
    // Get block header data
    auto header = block->getHeader();
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(8) << std::hex << header.version;
    ss << header.previous_hash;
    ss << header.merkle_root;
    ss << std::setfill('0') << std::setw(16) << std::hex << header.timestamp;
    ss << std::setfill('0') << std::setw(8) << std::hex << nonce;
    ss << std::setfill('0') << std::setw(8) << std::hex << header.difficulty;
    ss << std::setfill('0') << std::setw(16) << std::hex << header.height;
    
    return crypto::Hash::sha256(ss.str());
}

void ProofOfWork::updateHashRateStats() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_hash_time_);
    
    if (duration.count() >= 1) {
        hash_count_ = 0;
        last_hash_time_ = now;
    }
}

std::string ProofOfWork::difficultyToTarget(uint32_t difficulty) const {
    // Calculate target hash based on difficulty
    // Target = 2^256 / difficulty
    // For difficulty 1, target is maximum (all f's)
    // For higher difficulty, target gets smaller (more leading zeros)
    
    if (difficulty == 0) {
        return "0000000000000000000000000000000000000000000000000000000000000000";
    }
    
    // Use the existing calculateTargetHash method which already implements this logic
    return calculateTargetHash(difficulty);
}

uint32_t ProofOfWork::targetToDifficulty(const std::string& target) const {
    // Calculate difficulty based on target hash
    // Difficulty is inversely proportional to target value
    // Lower target = higher difficulty
    
    if (target.empty() || target == "0000000000000000000000000000000000000000000000000000000000000000") {
        return 0; // Invalid target
    }
    
    // Count leading zeros to estimate difficulty
    size_t leading_zeros = 0;
    for (char c : target) {
        if (c == '0') {
            leading_zeros++;
        } else {
            break;
        }
    }
    
    // Convert leading zeros to difficulty (exponential relationship)
    // This is a simplified calculation - in practice, you'd use the full 256-bit math
    if (leading_zeros == 0) {
        return 1; // No leading zeros = difficulty 1
    }
    
    // For targets with leading zeros, difficulty = leading_zeros
    return static_cast<uint32_t>(leading_zeros);
}

} // namespace consensus
} // namespace deo
