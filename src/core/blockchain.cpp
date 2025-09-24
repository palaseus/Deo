/**
 * @file blockchain.cpp
 * @brief Main blockchain implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "core/blockchain.h"
#include "utils/logger.h"
#include "utils/error_handler.h"
#include "consensus/proof_of_work.h"
#include "storage/block_storage.h"
#include "storage/state_storage.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <algorithm>

namespace deo {
namespace core {

Blockchain::Blockchain(const BlockchainConfig& config)
    : config_(config)
    , is_initialized_(false)
    , is_mining_(false)
    , current_height_(0)
    , total_difficulty_(0)
    , stop_mining_(false) {
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Blockchain created with config: " + config.network_id);
}

Blockchain::~Blockchain() {
    shutdown();
}

bool Blockchain::initialize() {
    DEO_LOG_INFO(BLOCKCHAIN, "Initializing blockchain");
    
    if (is_initialized_) {
        DEO_WARNING(BLOCKCHAIN, "Blockchain already initialized");
        return true;
    }
    
    // Create data directory if it doesn't exist
    if (!std::filesystem::exists(config_.data_directory)) {
        try {
            std::filesystem::create_directories(config_.data_directory);
            DEO_LOG_INFO(BLOCKCHAIN, "Created data directory: " + config_.data_directory);
        } catch (const std::exception& e) {
            DEO_ERROR(BLOCKCHAIN, "Failed to create data directory: " + std::string(e.what()));
            return false;
        }
    }
    
    // Initialize storage systems
    block_storage_ = std::make_unique<storage::BlockStorage>(config_.data_directory + "/blocks");
    state_storage_ = std::make_unique<storage::StateStorage>(config_.data_directory + "/state");
    
    if (!block_storage_->initialize()) {
        DEO_ERROR(BLOCKCHAIN, "Failed to initialize block storage");
        return false;
    }
    
    if (!state_storage_->initialize()) {
        DEO_ERROR(BLOCKCHAIN, "Failed to initialize state storage");
        return false;
    }
    
    // Initialize consensus engine
    consensus_engine_ = std::make_unique<consensus::ProofOfWork>(config_.initial_difficulty);
    if (!consensus_engine_->initialize()) {
        DEO_ERROR(BLOCKCHAIN, "Failed to initialize consensus engine");
        return false;
    }
    
    // Load existing state
    if (!loadState()) {
        DEO_WARNING(BLOCKCHAIN, "Failed to load existing state, starting fresh");
    }
    
    // Create genesis block if needed
    if (blocks_.empty()) {
        auto genesis_block = createGenesisBlock();
        if (genesis_block) {
            blocks_.push_back(genesis_block);
            block_index_[genesis_block->calculateHash()] = genesis_block;
            best_block_hash_ = genesis_block->calculateHash();
            current_height_ = 0;
            
            DEO_LOG_INFO(BLOCKCHAIN, "Genesis block created");
        }
    }
    
    is_initialized_ = true;
    
    DEO_LOG_INFO(BLOCKCHAIN, "Blockchain initialized successfully");
    return true;
}

bool Blockchain::isInitialized() const {
    return is_initialized_;
}

void Blockchain::shutdown() {
    DEO_LOG_INFO(BLOCKCHAIN, "Shutting down blockchain");
    
    stopMining();
    
    if (consensus_engine_) {
        consensus_engine_->shutdown();
    }
    
    if (block_storage_) {
        block_storage_->shutdown();
    }
    
    if (state_storage_) {
        state_storage_->shutdown();
    }
    
    is_initialized_ = false;
    
    DEO_LOG_INFO(BLOCKCHAIN, "Blockchain shutdown complete");
}

BlockchainState Blockchain::getState() const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    BlockchainState state;
    state.height = current_height_;
    state.best_block_hash = best_block_hash_;
    state.genesis_hash = blocks_.empty() ? "" : blocks_[0]->calculateHash();
    state.total_transactions = 0; // Placeholder
    state.total_blocks = blocks_.size();
    state.total_difficulty = total_difficulty_;
    state.last_block_time = std::chrono::system_clock::now();
    state.is_syncing = false;
    state.is_mining = is_mining_;
    
    return state;
}

bool Blockchain::addBlock(std::shared_ptr<Block> block) {
    if (!block) {
        DEO_ERROR(BLOCKCHAIN, "Cannot add null block");
        return false;
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Adding block: " + block->calculateHash());
    
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    // Validate block
    if (!validateBlock(block)) {
        DEO_ERROR(BLOCKCHAIN, "Block validation failed");
        return false;
    }
    
    // Check if block already exists
    if (block_index_.find(block->calculateHash()) != block_index_.end()) {
        DEO_LOG_DEBUG(BLOCKCHAIN, "Block already exists: " + block->calculateHash());
        return true;
    }
    
    // Handle chain reorganization if needed
    if (!handleChainReorganization(block)) {
        DEO_LOG_DEBUG(BLOCKCHAIN, "Chain reorganization failed or not needed");
        // If it's not a reorganization, check if it extends the current chain
        auto current_best = getBestBlock();
        if (current_best && block->getPreviousHash() == current_best->getHash()) {
            // Block extends current chain
            blocks_.push_back(block);
            block_index_[block->calculateHash()] = block;
            
            // Update state
            current_height_ = block->getHeader().height;
            best_block_hash_ = block->calculateHash();
            total_difficulty_ += block->getHeader().difficulty;
            
            // Update UTXO set
            updateUTXOSet(block);
            
            // Store block
            if (block_storage_) {
                block_storage_->storeBlock(block);
            }
            
            DEO_LOG_INFO(BLOCKCHAIN, "Block added successfully: " + block->calculateHash());
            return true;
        } else {
            DEO_LOG_DEBUG(BLOCKCHAIN, "Block does not extend current chain and is not heavier");
            return false;
        }
    }
    
    // Block was added through reorganization
    DEO_LOG_INFO(BLOCKCHAIN, "Block added through reorganization: " + block->calculateHash());
    return true;
}

std::shared_ptr<Block> Blockchain::getBlock(const std::string& hash) const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    auto it = block_index_.find(hash);
    if (it != block_index_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::shared_ptr<Block> Blockchain::getBlock(uint64_t height) const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    if (height < blocks_.size()) {
        return blocks_[height];
    }
    
    return nullptr;
}

std::shared_ptr<Block> Blockchain::getBestBlock() const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    if (blocks_.empty()) {
        return nullptr;
    }
    
    return blocks_.back();
}

std::shared_ptr<Block> Blockchain::getGenesisBlock() const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    if (blocks_.empty()) {
        return nullptr;
    }
    
    return blocks_[0];
}

bool Blockchain::addTransaction(std::shared_ptr<Transaction> transaction) {
    if (!transaction) {
        DEO_ERROR(BLOCKCHAIN, "Cannot add null transaction");
        return false;
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Adding transaction to mempool: " + transaction->getId());
    
    // Validate transaction
    if (!transaction->verify()) {
        DEO_ERROR(BLOCKCHAIN, "Transaction validation failed");
        return false;
    }
    
    // Validate inputs against UTXO set
    if (!validateTransactionInputs(transaction)) {
        DEO_ERROR(BLOCKCHAIN, "Transaction input validation failed");
        return false;
    }
    
    // Add to mempool
    mempool_[transaction->getId()] = transaction;
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction added to mempool successfully");
    return true;
}

std::shared_ptr<Transaction> Blockchain::getTransaction(const std::string& tx_id) const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    auto it = mempool_.find(tx_id);
    if (it != mempool_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<Transaction>> Blockchain::getMempoolTransactions(size_t max_count) const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    std::vector<std::shared_ptr<Transaction>> transactions;
    
    size_t count = 0;
    for (const auto& pair : mempool_) {
        if (max_count > 0 && count >= max_count) {
            break;
        }
        transactions.push_back(pair.second);
        count++;
    }
    
    return transactions;
}

bool Blockchain::removeTransaction(const std::string& tx_id) {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    auto it = mempool_.find(tx_id);
    if (it != mempool_.end()) {
        mempool_.erase(it);
        DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction removed from mempool: " + tx_id);
        return true;
    }
    
    return false;
}

uint64_t Blockchain::getHeight() const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    return current_height_;
}

uint64_t Blockchain::getTotalDifficulty() const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    return total_difficulty_;
}

bool Blockchain::verifyBlockchain() const {
    DEO_LOG_INFO(BLOCKCHAIN, "Verifying entire blockchain");
    
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    for (size_t i = 0; i < blocks_.size(); ++i) {
        const auto& block = blocks_[i];
        
        if (!block->verify()) {
            DEO_ERROR(BLOCKCHAIN, "Block verification failed at height: " + std::to_string(i));
            return false;
        }
        
        // Verify previous hash (except for genesis block)
        if (i > 0) {
            const auto& prev_block = blocks_[i - 1];
            if (block->getHeader().previous_hash != prev_block->calculateHash()) {
                DEO_ERROR(BLOCKCHAIN, "Previous hash mismatch at height: " + std::to_string(i));
                return false;
            }
        }
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Blockchain verification completed successfully");
    return true;
}

uint64_t Blockchain::getBalance(const std::string& address) const {
    if (state_storage_) {
        return state_storage_->getBalance(address);
    }
    return 0;
}

std::vector<TransactionOutput> Blockchain::getUnspentOutputs(const std::string& address) const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    auto it = utxo_set_.find(address);
    if (it != utxo_set_.end()) {
        return it->second;
    }
    
    return {};
}

bool Blockchain::startMining() {
    DEO_LOG_INFO(BLOCKCHAIN, "Starting mining");
    
    if (is_mining_) {
        DEO_WARNING(BLOCKCHAIN, "Mining is already active");
        return true;
    }
    
    if (!consensus_engine_) {
        DEO_ERROR(BLOCKCHAIN, "Consensus engine not initialized");
        return false;
    }
    
    is_mining_ = true;
    stop_mining_ = false;
    
    // Start mining thread
    mining_thread_ = std::thread([this]() { miningWorker(); });
    
    DEO_LOG_INFO(BLOCKCHAIN, "Mining started successfully");
    return true;
}

void Blockchain::stopMining() {
    DEO_LOG_INFO(BLOCKCHAIN, "Stopping mining");
    
    if (!is_mining_) {
        return;
    }
    
    stop_mining_ = true;
    is_mining_ = false;
    
    if (mining_thread_.joinable()) {
        mining_thread_.join();
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Mining stopped");
}

bool Blockchain::isMining() const {
    return is_mining_;
}

Blockchain::MiningStats Blockchain::getMiningStats() const {
    MiningStats stats;
    stats.blocks_mined = 0; // Placeholder
    stats.hashes_per_second = 0; // Placeholder
    stats.total_hashes = 0; // Placeholder
    stats.start_time = std::chrono::system_clock::now();
    
    return stats;
}

bool Blockchain::saveState() {
    DEO_LOG_INFO(BLOCKCHAIN, "Saving blockchain state");
    
    // Note: In a real implementation, we would save the blockchain state
    // This is a placeholder implementation
    DEO_LOG_WARNING(BLOCKCHAIN, "State saving not fully implemented yet");
    
    return true;
}

bool Blockchain::loadState() {
    DEO_LOG_INFO(BLOCKCHAIN, "Loading blockchain state");
    
    // Note: In a real implementation, we would load the blockchain state
    // This is a placeholder implementation
    DEO_LOG_WARNING(BLOCKCHAIN, "State loading not fully implemented yet");
    
    return true;
}

bool Blockchain::exportBlockchain(const std::string& filename) const {
    DEO_LOG_INFO(BLOCKCHAIN, "Exporting blockchain to: " + filename);
    
    // Note: In a real implementation, we would export the blockchain
    // This is a placeholder implementation
    DEO_LOG_WARNING(BLOCKCHAIN, "Blockchain export not fully implemented yet");
    
    return true;
}

bool Blockchain::importBlockchain(const std::string& filename) {
    DEO_LOG_INFO(BLOCKCHAIN, "Importing blockchain from: " + filename);
    
    // Note: In a real implementation, we would import the blockchain
    // This is a placeholder implementation
    DEO_LOG_WARNING(BLOCKCHAIN, "Blockchain import not fully implemented yet");
    
    return true;
}

std::shared_ptr<Block> Blockchain::createGenesisBlock() {
    DEO_LOG_INFO(BLOCKCHAIN, "Creating genesis block");
    
    BlockHeader header;
    header.version = 1;
    header.previous_hash = "0000000000000000000000000000000000000000000000000000000000000000"; // Genesis block previous hash (all zeros)
    header.merkle_root = config_.genesis_merkle_root;
    header.timestamp = config_.genesis_timestamp;
    header.nonce = 0;
    header.difficulty = config_.initial_difficulty;
    header.height = 0;
    
    // Create empty transaction list for genesis block
    std::vector<std::shared_ptr<Transaction>> transactions;
    
    auto genesis_block = std::make_shared<Block>(header, transactions);
    
    DEO_LOG_INFO(BLOCKCHAIN, "Genesis block created: " + genesis_block->calculateHash());
    return genesis_block;
}

bool Blockchain::validateBlock(std::shared_ptr<Block> block) const {
    if (!block) {
        return false;
    }
    
    // Validate block structure
    if (!block->verify()) {
        return false;
    }
    
    // Validate consensus
    if (consensus_engine_ && !consensus_engine_->validateBlock(block)) {
        return false;
    }
    
    return true;
}

void Blockchain::updateUTXOSet(std::shared_ptr<Block> block) {
    if (!block) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    // Process each transaction in the block
    for (const auto& tx : block->getTransactions()) {
        if (!tx) continue;
        
        // Remove spent outputs (inputs)
        for (const auto& input : tx->getInputs()) {
            if (input.previous_tx_hash != "0000000000000000000000000000000000000000000000000000000000000000") {
                // Remove from UTXO set
                std::string utxo_key = input.previous_tx_hash + "_" + std::to_string(input.output_index);
                auto it = utxo_set_.find(utxo_key);
                if (it != utxo_set_.end()) {
                    utxo_set_.erase(it);
                }
            }
        }
        
        // Add new outputs to UTXO set
        for (size_t i = 0; i < tx->getOutputs().size(); ++i) {
            const auto& output = tx->getOutputs()[i];
            std::string utxo_key = tx->getId() + "_" + std::to_string(i);
            utxo_set_[utxo_key] = {output}; // Wrap in vector
        }
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "UTXO set updated for block: " + block->calculateHash());
}

void Blockchain::miningWorker() {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Mining worker started");
    
    while (!stop_mining_) {
        try {
            // Get transactions from mempool
            auto transactions = getMempoolTransactions(100); // Limit to 100 transactions per block
            
            if (!transactions.empty()) {
                // Create new block
                auto block = createNewBlock(transactions);
                if (block) {
                    // Attempt to reach consensus on the block
                    if (consensus_engine_) {
                        auto consensus_result = consensus_engine_->startConsensus(block);
                        if (consensus_result.success) {
                            // Block consensus reached, add it to the blockchain
                            if (addBlock(block)) {
                                DEO_LOG_INFO(BLOCKCHAIN, "Successfully reached consensus and added block: " + block->calculateHash());
                                
                                // Remove mined transactions from mempool
                                for (const auto& tx : transactions) {
                                    removeTransaction(tx->getId());
                                }
                            }
                        }
                    }
                }
            }
            
            // Sleep for a short time before next mining attempt
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Mining worker error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Mining worker finished");
}

uint32_t Blockchain::calculateNextDifficulty() const {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    // Simple difficulty adjustment based on block time
    if (blocks_.size() < 2) {
        return config_.initial_difficulty;
    }
    
    // Get the last two blocks for time comparison
    auto last_block = blocks_.back();
    auto prev_block = blocks_[blocks_.size() - 2];
    
    uint64_t time_diff = last_block->getHeader().timestamp - prev_block->getHeader().timestamp;
    uint64_t target_time = 600; // 10 minutes in seconds
    
    uint32_t new_difficulty = config_.initial_difficulty;
    
    if (time_diff < target_time / 2) {
        // Blocks are coming too fast, increase difficulty
        new_difficulty = last_block->getHeader().difficulty * 2;
    } else if (time_diff > target_time * 2) {
        // Blocks are coming too slow, decrease difficulty
        new_difficulty = last_block->getHeader().difficulty / 2;
        if (new_difficulty < 1) new_difficulty = 1;
    } else {
        // Keep current difficulty
        new_difficulty = last_block->getHeader().difficulty;
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Calculated next difficulty: " + std::to_string(new_difficulty));
    return new_difficulty;
}

std::shared_ptr<Block> Blockchain::createNewBlock(const std::vector<std::shared_ptr<Transaction>>& transactions) {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    // Create block header
    BlockHeader header;
    header.version = 1;
    header.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header.difficulty = calculateNextDifficulty();
    header.height = current_height_ + 1;
    header.transaction_count = static_cast<uint32_t>(transactions.size());
    
    // Set previous hash
    if (!blocks_.empty()) {
        header.previous_hash = best_block_hash_;
    } else {
        header.previous_hash = "0000000000000000000000000000000000000000000000000000000000000000";
    }
    
    // Create block
    auto block = std::make_shared<Block>(header, transactions);
    
    // Update Merkle root
    block->updateMerkleRoot();
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Created new block with " + std::to_string(transactions.size()) + " transactions");
    return block;
}

bool Blockchain::handleChainReorganization(std::shared_ptr<Block> new_block) {
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    if (!new_block) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Cannot reorganize chain: new block is null");
        return false;
    }
    
    // Get current best block
    auto current_best = getBestBlock();
    if (!current_best) {
        DEO_LOG_INFO(BLOCKCHAIN, "No current best block, accepting new block");
        return true;
    }
    
    // Check if new block is on the same chain
    if (new_block->getPreviousHash() == current_best->getHash()) {
        DEO_LOG_DEBUG(BLOCKCHAIN, "New block extends current chain");
        return true;
    }
    
    // Find common ancestor
    auto common_ancestor = findCommonAncestor(new_block, current_best);
    if (!common_ancestor) {
        DEO_LOG_ERROR(BLOCKCHAIN, "No common ancestor found between blocks");
        return false;
    }
    
    // Calculate chain weights
    uint64_t current_weight = getChainWeight(current_best);
    uint64_t new_weight = getChainWeight(new_block);
    
    DEO_LOG_INFO(BLOCKCHAIN, "Chain weights - Current: " + std::to_string(current_weight) + 
                 ", New: " + std::to_string(new_weight));
    
    // Apply longest-chain-wins rule
    if (new_weight > current_weight) {
        DEO_LOG_INFO(BLOCKCHAIN, "New chain is heavier, performing reorganization");
        return reorganizeChain(new_block);
    } else {
        DEO_LOG_DEBUG(BLOCKCHAIN, "Current chain is heavier, rejecting new block");
        return false;
    }
}

std::shared_ptr<Block> Blockchain::findCommonAncestor(std::shared_ptr<Block> block1, 
                                                     std::shared_ptr<Block> block2) const {
    if (!block1 || !block2) {
        return nullptr;
    }
    
    // Build chain from block1 to genesis
    std::unordered_set<std::string> block1_chain;
    auto current = block1;
    while (current) {
        block1_chain.insert(current->getHash());
        if (current->getPreviousHash().empty()) {
            break; // Reached genesis
        }
        current = getBlock(current->getPreviousHash());
    }
    
    // Walk up from block2 until we find a common block
    current = block2;
    while (current) {
        if (block1_chain.find(current->getHash()) != block1_chain.end()) {
            return current;
        }
        if (current->getPreviousHash().empty()) {
            break; // Reached genesis
        }
        current = getBlock(current->getPreviousHash());
    }
    
    return nullptr; // No common ancestor found
}

uint64_t Blockchain::getChainWeight(std::shared_ptr<Block> block) const {
    if (!block) {
        return 0;
    }
    
    uint64_t weight = 0;
    auto current = block;
    
    // Walk up the chain and sum difficulties
    while (current) {
        weight += current->getHeader().difficulty;
        if (current->getPreviousHash().empty()) {
            break; // Reached genesis
        }
        current = getBlock(current->getPreviousHash());
    }
    
    return weight;
}

bool Blockchain::isOnMainChain(std::shared_ptr<Block> block) const {
    if (!block) {
        return false;
    }
    
    auto best_block = getBestBlock();
    if (!best_block) {
        return false;
    }
    
    // Check if block is an ancestor of the best block
    auto current = best_block;
    while (current) {
        if (current->getHash() == block->getHash()) {
            return true;
        }
        if (current->getPreviousHash().empty()) {
            break; // Reached genesis
        }
        current = getBlock(current->getPreviousHash());
    }
    
    return false;
}

bool Blockchain::reorganizeChain(std::shared_ptr<Block> new_best_block) {
    DEO_LOG_INFO(BLOCKCHAIN, "Starting chain reorganization to block: " + new_best_block->getHash());
    
    // Find the fork point
    auto current_best = getBestBlock();
    auto fork_point = findCommonAncestor(new_best_block, current_best);
    
    if (!fork_point) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Cannot reorganize: no fork point found");
        return false;
    }
    
    // Build the new chain from fork point to new best block
    std::vector<std::shared_ptr<Block>> new_chain;
    auto current = new_best_block;
    while (current && current->getHash() != fork_point->getHash()) {
        new_chain.push_back(current);
        if (current->getPreviousHash().empty()) {
            break;
        }
        current = getBlock(current->getPreviousHash());
    }
    
    // Reverse to get correct order (from fork point to new best)
    std::reverse(new_chain.begin(), new_chain.end());
    
    // Validate the new chain
    for (const auto& block : new_chain) {
        if (!block->validate()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Invalid block in new chain: " + block->getHash());
            return false;
        }
    }
    
    // Update the best block
    best_block_hash_ = new_best_block->getHash();
    current_height_ = new_best_block->getHeight();
    
    DEO_LOG_INFO(BLOCKCHAIN, "Chain reorganization completed. New height: " + std::to_string(current_height_));
    
    return true;
}

bool Blockchain::validateTransactionInputs(std::shared_ptr<Transaction> transaction) const {
    if (!transaction) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    // Validate each input
    for (const auto& input : transaction->getInputs()) {
        // Skip coinbase transactions
        if (input.previous_tx_hash == "0000000000000000000000000000000000000000000000000000000000000000") {
            continue;
        }
        
        // Check if the referenced output exists in UTXO set
        std::string utxo_key = input.previous_tx_hash + "_" + std::to_string(input.output_index);
        auto it = utxo_set_.find(utxo_key);
        if (it == utxo_set_.end()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Transaction input references non-existent UTXO: " + utxo_key);
            return false;
        }
        
        // Verify the output is not already spent
        // (This is a simplified check - in reality we'd need more sophisticated tracking)
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction input validation passed");
    return true;
}

bool Blockchain::processBlockTransactions(std::shared_ptr<Block> block) {
    if (!block) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(blockchain_mutex_);
    
    // Process each transaction in the block
    for (const auto& tx : block->getTransactions()) {
        if (!tx) continue;
        
        // Validate transaction
        if (!tx->verify()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Invalid transaction in block: " + tx->getId());
            return false;
        }
        
        // Validate transaction inputs
        if (!validateTransactionInputs(tx)) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Transaction input validation failed: " + tx->getId());
            return false;
        }
        
        // Update UTXO set
        // Remove spent outputs
        for (const auto& input : tx->getInputs()) {
            if (input.previous_tx_hash != "0000000000000000000000000000000000000000000000000000000000000000") {
                std::string utxo_key = input.previous_tx_hash + "_" + std::to_string(input.output_index);
                utxo_set_.erase(utxo_key);
            }
        }
        
        // Add new outputs
        for (size_t i = 0; i < tx->getOutputs().size(); ++i) {
            const auto& output = tx->getOutputs()[i];
            std::string utxo_key = tx->getId() + "_" + std::to_string(i);
            utxo_set_[utxo_key] = {output}; // Wrap in vector
        }
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Processed " + std::to_string(block->getTransactions().size()) + " transactions in block");
    return true;
}

} // namespace core
} // namespace deo
