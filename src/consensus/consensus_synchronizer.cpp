/**
 * @file consensus_synchronizer.cpp
 * @brief Consensus synchronization across multiple nodes
 */

#include "consensus/consensus_synchronizer.h"
#include <algorithm>
#include <chrono>

namespace deo {
namespace consensus {

ConsensusSynchronizer::ConsensusSynchronizer(std::shared_ptr<core::Blockchain> blockchain,
                                             std::shared_ptr<ConsensusEngine> consensus_engine)
    : blockchain_(blockchain)
    , consensus_engine_(consensus_engine)
    , running_(false)
    , syncing_(false)
    , fork_choice_rule_(ForkChoiceRule::LONGEST_CHAIN) {
    
    stats_.total_blocks_processed = 0;
    stats_.blocks_reorganized = 0;
    stats_.forks_detected = 0;
    stats_.consensus_violations = 0;
    stats_.start_time = std::chrono::steady_clock::now();
    stats_.average_block_processing_time_ms = 0.0;
}

ConsensusSynchronizer::~ConsensusSynchronizer() {
    shutdown();
}

bool ConsensusSynchronizer::initialize() {
    DEO_LOG_INFO(CONSENSUS, "Initializing consensus synchronizer");
    
    if (!blockchain_) {
        DEO_LOG_ERROR(CONSENSUS, "Blockchain instance is null");
        return false;
    }
    
    if (!consensus_engine_) {
        DEO_LOG_ERROR(CONSENSUS, "Consensus engine instance is null");
        return false;
    }
    
    running_ = true;
    sync_thread_ = std::thread(&ConsensusSynchronizer::synchronizationLoop, this);
    
    DEO_LOG_INFO(CONSENSUS, "Consensus synchronizer initialized successfully");
    return true;
}

void ConsensusSynchronizer::shutdown() {
    if (!running_) {
        return;
    }
    
    DEO_LOG_INFO(CONSENSUS, "Shutting down consensus synchronizer");
    running_ = false;
    syncing_ = false;
    
    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }
    
    DEO_LOG_INFO(CONSENSUS, "Consensus synchronizer shutdown complete");
}

bool ConsensusSynchronizer::isRunning() const {
    return running_;
}

bool ConsensusSynchronizer::handleChainReorganization(const std::vector<std::shared_ptr<core::Block>>& new_blocks) {
    if (new_blocks.empty()) {
        return false;
    }
    
    DEO_LOG_INFO(CONSENSUS, "Handling chain reorganization with " + std::to_string(new_blocks.size()) + " blocks");
    
    std::lock_guard<std::mutex> lock(sync_mutex_);
    
    // Validate all blocks in the new chain
    for (const auto& block : new_blocks) {
        if (!validateBlockForConsensus(block)) {
            DEO_LOG_ERROR(CONSENSUS, "Block validation failed during reorganization: " + block->getHash());
            return false;
        }
    }
    
    // Check if reorganization is needed
    std::string new_tip_hash = new_blocks.back()->getHash();
    if (!shouldReorganizeChain(new_tip_hash)) {
        DEO_LOG_DEBUG(CONSENSUS, "No reorganization needed for block: " + new_tip_hash);
        return true;
    }
    
    // Get blocks to remove and add
    std::vector<std::shared_ptr<core::Block>> blocks_to_remove = getBlocksToReorganize(new_tip_hash);
    std::vector<std::shared_ptr<core::Block>> blocks_to_add = new_blocks;
    
    // Execute reorganization
    executeReorganization(blocks_to_remove, blocks_to_add);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.blocks_reorganized += blocks_to_remove.size();
        stats_.forks_detected++;
    }
    
    DEO_LOG_INFO(CONSENSUS, "Chain reorganization completed successfully");
    return true;
}

std::shared_ptr<core::Block> ConsensusSynchronizer::findCommonAncestor(const std::string& block_hash1, const std::string& block_hash2) {
    if (!blockchain_) {
        return nullptr;
    }
    
    // Get blocks from both chains
    auto block1 = blockchain_->getBlock(block_hash1);
    auto block2 = blockchain_->getBlock(block_hash2);
    
    if (!block1 || !block2) {
        return nullptr;
    }
    
    // Build ancestor chains
    std::set<std::string> ancestors1;
    std::string current_hash = block_hash1;
    
    while (!current_hash.empty()) {
        ancestors1.insert(current_hash);
        auto current_block = blockchain_->getBlock(current_hash);
        if (!current_block) {
            break;
        }
        current_hash = current_block->getPreviousHash();
    }
    
    // Find common ancestor
    current_hash = block_hash2;
    while (!current_hash.empty()) {
        if (ancestors1.find(current_hash) != ancestors1.end()) {
            return blockchain_->getBlock(current_hash);
        }
        
        auto current_block = blockchain_->getBlock(current_hash);
        if (!current_block) {
            break;
        }
        current_hash = current_block->getPreviousHash();
    }
    
    return nullptr;
}

uint64_t ConsensusSynchronizer::calculateChainWeight(const std::string& block_hash) {
    if (!blockchain_) {
        return 0;
    }
    
    uint64_t weight = 0;
    std::string current_hash = block_hash;
    
    while (!current_hash.empty()) {
        auto block = blockchain_->getBlock(current_hash);
        if (!block) {
            break;
        }
        
        // Add block weight (simplified: just count blocks)
        weight++;
        
        current_hash = block->getPreviousHash();
    }
    
    return weight;
}

bool ConsensusSynchronizer::isOnMainChain(const std::string& block_hash) {
    if (!blockchain_) {
        return false;
    }
    
    // Get the current tip
    auto tip = blockchain_->getBestBlock();
    if (!tip) {
        return false;
    }
    
    // Check if block is in the path to the tip
    std::string current_hash = tip->getHash();
    while (!current_hash.empty()) {
        if (current_hash == block_hash) {
            return true;
        }
        
        auto block = blockchain_->getBlock(current_hash);
        if (!block) {
            break;
        }
        current_hash = block->getPreviousHash();
    }
    
    return false;
}

bool ConsensusSynchronizer::validateBlockForConsensus(std::shared_ptr<core::Block> block) {
    if (!block || !consensus_engine_) {
        return false;
    }
    
    // Use consensus engine to validate block
    bool is_valid = consensus_engine_->validateBlock(block);
    
    if (!is_valid) {
        DEO_LOG_WARNING(CONSENSUS, "Block failed consensus validation: " + block->getHash());
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.consensus_violations++;
    }
    
    return is_valid;
}

bool ConsensusSynchronizer::validateTransactionForConsensus(std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        return false;
    }
    
    // Basic transaction validation
    bool is_valid = transaction->verify();
    
    if (!is_valid) {
        DEO_LOG_WARNING(CONSENSUS, "Transaction failed consensus validation: " + transaction->getId());
    }
    
    return is_valid;
}

bool ConsensusSynchronizer::executeBlockTransactions(std::shared_ptr<core::Block> block) {
    if (!block || !blockchain_) {
        return false;
    }
    
    DEO_LOG_DEBUG(CONSENSUS, "Executing transactions in block: " + block->getHash());
    
    // Execute each transaction in the block
    for (const auto& transaction : block->getTransactions()) {
        if (!validateTransactionForConsensus(transaction)) {
            DEO_LOG_ERROR(CONSENSUS, "Transaction validation failed: " + transaction->getId());
            return false;
        }
        
        // TODO: Execute transaction in VM and update state
        // This would involve calling the VM and updating the state trie
    }
    
    return true;
}

SyncStatus ConsensusSynchronizer::getSyncStatus() const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    return sync_status_;
}

void ConsensusSynchronizer::startSynchronization() {
    DEO_LOG_INFO(CONSENSUS, "Starting consensus synchronization");
    syncing_ = true;
}

void ConsensusSynchronizer::stopSynchronization() {
    DEO_LOG_INFO(CONSENSUS, "Stopping consensus synchronization");
    syncing_ = false;
}

bool ConsensusSynchronizer::isSynchronized() const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    return !sync_status_.is_syncing && sync_status_.current_height == sync_status_.target_height;
}

void ConsensusSynchronizer::setForkChoiceRule(ForkChoiceRule rule) {
    fork_choice_rule_ = rule;
    DEO_LOG_INFO(CONSENSUS, "Fork choice rule set to: " + std::to_string(static_cast<int>(rule)));
}

ForkChoiceRule ConsensusSynchronizer::getForkChoiceRule() const {
    return fork_choice_rule_;
}

ConsensusSynchronizer::ConsensusStats ConsensusSynchronizer::getConsensusStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void ConsensusSynchronizer::setBlockHandler(std::function<void(std::shared_ptr<core::Block>)> handler) {
    block_handler_ = handler;
}

void ConsensusSynchronizer::setForkHandler(std::function<void(const std::string&, const std::string&)> handler) {
    fork_handler_ = handler;
}

void ConsensusSynchronizer::synchronizationLoop() {
    DEO_LOG_INFO(CONSENSUS, "Consensus synchronization thread started");
    
    while (running_) {
        if (syncing_) {
            updateSyncStatus();
            
            // TODO: Implement actual synchronization logic
            // This would involve:
            // 1. Requesting blocks from peers
            // 2. Validating received blocks
            // 3. Handling chain reorganizations
            // 4. Updating consensus state
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    DEO_LOG_INFO(CONSENSUS, "Consensus synchronization thread stopped");
}

bool ConsensusSynchronizer::shouldReorganizeChain(const std::string& new_block_hash) {
    if (!blockchain_) {
        return false;
    }
    
    auto current_tip = blockchain_->getBestBlock();
    if (!current_tip) {
        return true; // No current chain, accept new block
    }
    
    // Check if new block is already on main chain
    if (isOnMainChain(new_block_hash)) {
        return false;
    }
    
    // Apply fork choice rule
    switch (fork_choice_rule_) {
        case ForkChoiceRule::LONGEST_CHAIN: {
            uint64_t current_weight = calculateChainWeight(current_tip->getHash());
            uint64_t new_weight = calculateChainWeight(new_block_hash);
            return new_weight > current_weight;
        }
        
        case ForkChoiceRule::HEAVIEST_CHAIN: {
            // TODO: Implement heaviest chain logic
            // This would calculate total work/difficulty
            return false;
        }
        
        case ForkChoiceRule::GHOST: {
            // TODO: Implement GHOST protocol
            return false;
        }
        
        default:
            return false;
    }
}

std::vector<std::shared_ptr<core::Block>> ConsensusSynchronizer::getBlocksToReorganize(const std::string& new_tip) {
    std::vector<std::shared_ptr<core::Block>> blocks_to_remove;
    
    if (!blockchain_) {
        return blocks_to_remove;
    }
    
    auto current_tip = blockchain_->getBestBlock();
    if (!current_tip) {
        return blocks_to_remove;
    }
    
    // Find common ancestor
    auto common_ancestor = findCommonAncestor(current_tip->getHash(), new_tip);
    if (!common_ancestor) {
        return blocks_to_remove;
    }
    
    // Get blocks to remove (from current tip to common ancestor)
    std::string current_hash = current_tip->getHash();
    while (current_hash != common_ancestor->getHash()) {
        auto block = blockchain_->getBlock(current_hash);
        if (!block) {
            break;
        }
        
        blocks_to_remove.push_back(block);
        current_hash = block->getPreviousHash();
    }
    
    return blocks_to_remove;
}

void ConsensusSynchronizer::executeReorganization(const std::vector<std::shared_ptr<core::Block>>& blocks_to_remove,
                                                 const std::vector<std::shared_ptr<core::Block>>& blocks_to_add) {
    DEO_LOG_INFO(CONSENSUS, "Executing chain reorganization");
    
    // TODO: Implement actual reorganization logic
    // This would involve:
    // 1. Reverting state changes from blocks to remove
    // 2. Applying state changes from blocks to add
    // 3. Updating blockchain state
    
    // For now, just log the reorganization
    DEO_LOG_INFO(CONSENSUS, "Removing " + std::to_string(blocks_to_remove.size()) + " blocks");
    DEO_LOG_INFO(CONSENSUS, "Adding " + std::to_string(blocks_to_add.size()) + " blocks");
    
    // Call fork handler if set
    if (fork_handler_ && !blocks_to_remove.empty() && !blocks_to_add.empty()) {
        fork_handler_(blocks_to_remove.back()->getHash(), blocks_to_add.back()->getHash());
    }
}

void ConsensusSynchronizer::updateSyncStatus() {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    
    if (blockchain_) {
        auto latest_block = blockchain_->getBestBlock();
        if (latest_block) {
            sync_status_.current_height = latest_block->getHeight();
            sync_status_.best_block_hash = latest_block->getHash();
        }
    }
    
    sync_status_.is_syncing = syncing_;
    sync_status_.last_sync_time = std::chrono::steady_clock::now();
}

void ConsensusSynchronizer::recordBlockProcessing(std::chrono::milliseconds processing_time) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_blocks_processed++;
    
    // Update average processing time
    double total_time = stats_.average_block_processing_time_ms * (stats_.total_blocks_processed - 1);
    total_time += processing_time.count();
    stats_.average_block_processing_time_ms = total_time / stats_.total_blocks_processed;
}

void ConsensusSynchronizer::recordForkDetection() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.forks_detected++;
}

void ConsensusSynchronizer::recordConsensusViolation() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.consensus_violations++;
}

// DeterministicValidator implementation

DeterministicValidator::DeterministicValidator() {
    stats_.blocks_validated = 0;
    stats_.transactions_validated = 0;
    stats_.state_transitions_validated = 0;
    stats_.determinism_tests_passed = 0;
    stats_.determinism_tests_failed = 0;
    stats_.start_time = std::chrono::steady_clock::now();
}

DeterministicValidator::~DeterministicValidator() {
}

bool DeterministicValidator::validateBlockExecution(std::shared_ptr<core::Block> block) {
    if (!block) {
        return false;
    }
    
    DEO_LOG_DEBUG(CONSENSUS, "Validating block execution: " + block->getHash());
    
    // TODO: Implement block execution validation
    // This would involve:
    // 1. Executing all transactions in the block
    // 2. Verifying state transitions
    // 3. Checking gas consumption
    // 4. Validating receipts
    
    recordBlockValidation();
    return true;
}

bool DeterministicValidator::validateTransactionExecution(std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        return false;
    }
    
    DEO_LOG_DEBUG(CONSENSUS, "Validating transaction execution: " + transaction->getId());
    
    // TODO: Implement transaction execution validation
    // This would involve:
    // 1. Executing the transaction in the VM
    // 2. Verifying gas consumption
    // 3. Checking state changes
    // 4. Validating signatures
    
    recordTransactionValidation();
    return true;
}

bool DeterministicValidator::validateStateTransition(const std::string& /* previous_state_hash */, 
                                                    const std::string& /* new_state_hash */) {
    DEO_LOG_DEBUG(CONSENSUS, "Validating state transition");
    
    // TODO: Implement state transition validation
    // This would involve:
    // 1. Comparing state hashes
    // 2. Verifying state changes are valid
    // 3. Checking for state inconsistencies
    
    recordStateTransitionValidation();
    return true;
}

bool DeterministicValidator::testDeterministicExecution(const std::vector<std::shared_ptr<core::Transaction>>& /* transactions */) {
    DEO_LOG_DEBUG(CONSENSUS, "Testing deterministic execution");
    
    // TODO: Implement deterministic execution testing
    // This would involve:
    // 1. Executing transactions multiple times
    // 2. Comparing execution results
    // 3. Verifying identical state transitions
    
    bool passed = true; // Placeholder
    
    if (passed) {
        recordDeterminismTest(true);
    } else {
        recordDeterminismTest(false);
    }
    
    return passed;
}

std::string DeterministicValidator::calculateExecutionHash(const std::vector<std::shared_ptr<core::Transaction>>& /* transactions */) {
    // TODO: Implement execution hash calculation
    // This would involve:
    // 1. Executing all transactions
    // 2. Collecting execution results
    // 3. Calculating a hash of the results
    
    return "execution_hash_placeholder";
}

DeterministicValidator::ValidationStats DeterministicValidator::getValidationStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void DeterministicValidator::recordBlockValidation() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.blocks_validated++;
}

void DeterministicValidator::recordTransactionValidation() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.transactions_validated++;
}

void DeterministicValidator::recordStateTransitionValidation() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.state_transitions_validated++;
}

void DeterministicValidator::recordDeterminismTest(bool passed) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (passed) {
        stats_.determinism_tests_passed++;
    } else {
        stats_.determinism_tests_failed++;
    }
}

} // namespace consensus
} // namespace deo
