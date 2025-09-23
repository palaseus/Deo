/**
 * @file transaction_mempool.cpp
 * @brief Thread-safe transaction mempool with P2P propagation
 */

#include "network/transaction_mempool.h"
#include <algorithm>

namespace deo {
namespace network {

TransactionMempool::TransactionMempool(std::shared_ptr<TcpNetworkManager> network_manager,
                                       std::shared_ptr<PeerManager> peer_manager)
    : network_manager_(network_manager)
    , peer_manager_(peer_manager)
    , running_(false) {
    stats_.total_transactions = 0;
    stats_.validated_transactions = 0;
    stats_.pending_validation = 0;
    stats_.transactions_propagated = 0;
    stats_.transactions_received = 0;
    stats_.duplicate_transactions_filtered = 0;
    stats_.start_time = std::chrono::steady_clock::now();
}

TransactionMempool::~TransactionMempool() {
    shutdown();
}

bool TransactionMempool::initialize() {
    DEO_LOG_INFO(NETWORKING, "Initializing transaction mempool");
    
    running_ = true;
    validation_thread_ = std::thread(&TransactionMempool::validationLoop, this);
    cleanup_thread_ = std::thread(&TransactionMempool::cleanupLoop, this);
    
    DEO_LOG_INFO(NETWORKING, "Transaction mempool initialized successfully");
    return true;
}

void TransactionMempool::shutdown() {
    if (!running_) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Shutting down transaction mempool");
    running_ = false;
    
    validation_condition_.notify_all();
    
    if (validation_thread_.joinable()) {
        validation_thread_.join();
    }
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    
    DEO_LOG_INFO(NETWORKING, "Transaction mempool shutdown complete");
}

bool TransactionMempool::addTransaction(std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        DEO_LOG_ERROR(NETWORKING, "Cannot add null transaction to mempool");
        return false;
    }
    
    std::string tx_id = transaction->getId();
    
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    // Check if transaction already exists
    if (transactions_.find(tx_id) != transactions_.end()) {
        DEO_LOG_DEBUG(NETWORKING, "Transaction already in mempool: " + tx_id);
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.duplicate_transactions_filtered++;
        return false;
    }
    
    // Check mempool size limit
    if (transactions_.size() >= MAX_MEMPOOL_SIZE) {
        DEO_LOG_WARNING(NETWORKING, "Mempool size limit reached, removing oldest transaction");
        // Remove oldest transaction
        auto oldest = std::min_element(transactions_.begin(), transactions_.end(),
            [](const auto& a, const auto& b) {
                return a.second.received_time < b.second.received_time;
            });
        if (oldest != transactions_.end()) {
            transactions_.erase(oldest);
        }
    }
    
    // Add transaction to mempool
    transactions_.emplace(tx_id, MempoolEntry(transaction));
    
    // Add to validation queue
    {
        std::lock_guard<std::mutex> queue_lock(validation_queue_mutex_);
        validation_queue_.push(tx_id);
        validation_condition_.notify_one();
    }
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.total_transactions++;
    stats_.transactions_received++;
    stats_.pending_validation++;
    
    DEO_LOG_DEBUG(NETWORKING, "Added transaction to mempool: " + tx_id);
    return true;
}

bool TransactionMempool::removeTransaction(const std::string& transaction_id) {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    auto it = transactions_.find(transaction_id);
    if (it != transactions_.end()) {
        transactions_.erase(it);
        DEO_LOG_DEBUG(NETWORKING, "Removed transaction from mempool: " + transaction_id);
        return true;
    }
    
    return false;
}

std::shared_ptr<core::Transaction> TransactionMempool::getTransaction(const std::string& transaction_id) const {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    auto it = transactions_.find(transaction_id);
    if (it != transactions_.end()) {
        return it->second.transaction;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<core::Transaction>> TransactionMempool::getTransactions(size_t max_count) const {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    std::vector<std::shared_ptr<core::Transaction>> result;
    result.reserve(std::min(max_count, transactions_.size()));
    
    for (const auto& [tx_id, entry] : transactions_) {
        if (result.size() >= max_count) {
            break;
        }
        result.push_back(entry.transaction);
    }
    
    return result;
}

std::vector<std::shared_ptr<core::Transaction>> TransactionMempool::getTransactionsForBlock(size_t max_count) const {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    std::vector<std::pair<std::string, MempoolEntry>> sorted_entries;
    
    // Collect validated transactions
    for (const auto& [tx_id, entry] : transactions_) {
        if (entry.is_validated) {
            sorted_entries.push_back({tx_id, entry});
        }
    }
    
    // Sort by priority (fee per byte, then by timestamp)
    std::sort(sorted_entries.begin(), sorted_entries.end(),
        [](const auto& a, const auto& b) {
            // TODO: Implement proper fee calculation
            // For now, sort by timestamp (newest first)
            return a.second.received_time > b.second.received_time;
        });
    
    std::vector<std::shared_ptr<core::Transaction>> result;
    result.reserve(std::min(max_count, sorted_entries.size()));
    
    for (size_t i = 0; i < std::min(max_count, sorted_entries.size()); ++i) {
        result.push_back(sorted_entries[i].second.transaction);
    }
    
    return result;
}

bool TransactionMempool::validateTransaction(std::shared_ptr<core::Transaction> transaction) const {
    if (!transaction) {
        return false;
    }
    
    // Basic validation
    if (!transaction->verify()) {
        DEO_LOG_WARNING(NETWORKING, "Transaction signature verification failed");
        return false;
    }
    
    // TODO: Add more validation checks:
    // - Check if inputs exist and are unspent
    // - Check if sender has sufficient balance
    // - Check transaction size limits
    // - Check gas limits for smart contracts
    
    return true;
}

void TransactionMempool::validateAllTransactions() {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    for (auto& [tx_id, entry] : transactions_) {
        if (!entry.is_validated) {
            if (validateTransaction(entry.transaction)) {
                entry.is_validated = true;
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.validated_transactions++;
                stats_.pending_validation--;
            }
        }
    }
}

void TransactionMempool::propagateTransaction(const std::string& transaction_id) {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    auto it = transactions_.find(transaction_id);
    if (it != transactions_.end() && it->second.is_validated) {
        propagateToPeers(transaction_id);
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.transactions_propagated++;
    }
}

void TransactionMempool::handleIncomingTransaction(std::shared_ptr<core::Transaction> transaction, const std::string& peer_address) {
    if (!transaction) {
        peer_manager_->reportMisbehavior(peer_address, 0, 5);
        return;
    }
    
    std::string tx_id = transaction->getId();
    
    // Check if we already have this transaction
    {
        std::lock_guard<std::mutex> lock(mempool_mutex_);
        if (transactions_.find(tx_id) != transactions_.end()) {
            DEO_LOG_DEBUG(NETWORKING, "Received duplicate transaction: " + tx_id);
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.duplicate_transactions_filtered++;
            return;
        }
    }
    
    // Add transaction to mempool
    if (addTransaction(transaction)) {
        // Record that we received this from this peer
        recordPropagation(tx_id, peer_address);
        
        // Propagate to other peers
        propagateTransaction(tx_id);
        
        DEO_LOG_DEBUG(NETWORKING, "Handled incoming transaction from " + peer_address + ": " + tx_id);
    }
}

void TransactionMempool::broadcastNewTransaction(std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        return;
    }
    
    std::string tx_id = transaction->getId();
    
    // Add to our mempool first
    if (addTransaction(transaction)) {
        // Broadcast to all peers
        propagateTransaction(tx_id);
        
        DEO_LOG_INFO(NETWORKING, "Broadcasted new transaction: " + tx_id);
    }
}

size_t TransactionMempool::getTransactionCount() const {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    return transactions_.size();
}

size_t TransactionMempool::getValidatedTransactionCount() const {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    size_t count = 0;
    for (const auto& [tx_id, entry] : transactions_) {
        if (entry.is_validated) {
            count++;
        }
    }
    
    return count;
}

bool TransactionMempool::hasTransaction(const std::string& transaction_id) const {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    return transactions_.find(transaction_id) != transactions_.end();
}

std::vector<std::string> TransactionMempool::getTransactionIds() const {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    std::vector<std::string> ids;
    ids.reserve(transactions_.size());
    
    for (const auto& [tx_id, entry] : transactions_) {
        ids.push_back(tx_id);
    }
    
    return ids;
}

TransactionMempool::MempoolStats TransactionMempool::getMempoolStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void TransactionMempool::cleanupExpiredTransactions() {
    std::lock_guard<std::mutex> lock(mempool_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    auto it = transactions_.begin();
    while (it != transactions_.end()) {
        if (now - it->second.received_time > TRANSACTION_EXPIRY) {
            DEO_LOG_DEBUG(NETWORKING, "Removing expired transaction: " + it->first);
            it = transactions_.erase(it);
        } else {
            ++it;
        }
    }
}

void TransactionMempool::cleanupPropagatedTransactions() {
    std::lock_guard<std::mutex> lock(propagation_mutex_);
    
    [[maybe_unused]] auto now = std::chrono::steady_clock::now();
    auto cleanup_threshold = std::chrono::hours(1);
    
    auto it = propagation_map_.begin();
    while (it != propagation_map_.end()) {
        // Clean up old propagation records
        it = propagation_map_.erase(it);
    }
}

void TransactionMempool::validationLoop() {
    DEO_LOG_INFO(NETWORKING, "Transaction validation thread started");
    
    while (running_) {
        std::unique_lock<std::mutex> lock(validation_queue_mutex_);
        validation_condition_.wait(lock, [this] { return !validation_queue_.empty() || !running_; });
        
        if (!running_) {
            break;
        }
        
        while (!validation_queue_.empty()) {
            std::string tx_id = validation_queue_.front();
            validation_queue_.pop();
            lock.unlock();
            
            // Validate transaction
            auto transaction = getTransaction(tx_id);
            if (transaction) {
                if (validateTransaction(transaction)) {
                    // Mark as validated
                    {
                        std::lock_guard<std::mutex> mempool_lock(mempool_mutex_);
                        auto it = transactions_.find(tx_id);
                        if (it != transactions_.end()) {
                            it->second.is_validated = true;
                        }
                    }
                    
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    stats_.validated_transactions++;
                    stats_.pending_validation--;
                }
            }
            
            lock.lock();
        }
    }
    
    DEO_LOG_INFO(NETWORKING, "Transaction validation thread stopped");
}

void TransactionMempool::cleanupLoop() {
    DEO_LOG_INFO(NETWORKING, "Transaction mempool cleanup thread started");
    
    while (running_) {
        cleanupExpiredTransactions();
        cleanupPropagatedTransactions();
        
        std::this_thread::sleep_for(PROPAGATION_CLEANUP_INTERVAL);
    }
    
    DEO_LOG_INFO(NETWORKING, "Transaction mempool cleanup thread stopped");
}

void TransactionMempool::propagateToPeers(const std::string& transaction_id, const std::set<std::string>& exclude_peers) {
    auto transaction = getTransaction(transaction_id);
    if (!transaction) {
        return;
    }
    
    auto connected_peers = peer_manager_->getConnectedPeers();
    
    for (const auto& peer_key : connected_peers) {
        if (exclude_peers.find(peer_key) == exclude_peers.end()) {
            if (shouldPropagateToPeer(transaction_id, peer_key)) {
                // Create network message
                // Create transaction message using the correct message type
                auto tx_message = std::make_unique<TxMessage>(transaction);
                // Note: We would need to send the message through the network manager
                // For now, we'll just log the propagation attempt
                
                // Extract address from peer_key
                size_t colon_pos = peer_key.find(':');
                if (colon_pos != std::string::npos) {
                    std::string address = peer_key.substr(0, colon_pos);
                    // network_manager_->sendToPeer(address, *tx_message);
                    DEO_LOG_DEBUG(NETWORKING, "Transaction propagation to " + address + " (implementation pending)");
                    recordPropagation(transaction_id, peer_key);
                }
            }
        }
    }
}

bool TransactionMempool::shouldPropagateToPeer(const std::string& transaction_id, const std::string& peer_address) const {
    std::lock_guard<std::mutex> lock(propagation_mutex_);
    
    auto it = propagation_map_.find(transaction_id);
    if (it != propagation_map_.end()) {
        return it->second.find(peer_address) == it->second.end();
    }
    
    return true;
}

void TransactionMempool::recordPropagation(const std::string& transaction_id, const std::string& peer_address) {
    std::lock_guard<std::mutex> lock(propagation_mutex_);
    propagation_map_[transaction_id].insert(peer_address);
}

std::string TransactionMempool::calculateTransactionPriority(std::shared_ptr<core::Transaction> transaction) const {
    // TODO: Implement proper priority calculation based on fee per byte
    // For now, return a simple priority based on timestamp
    return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        transaction->getTimestamp().time_since_epoch()).count());
}

// BlockMempool implementation

BlockMempool::BlockMempool(std::shared_ptr<TcpNetworkManager> network_manager,
                           std::shared_ptr<PeerManager> peer_manager)
    : network_manager_(network_manager)
    , peer_manager_(peer_manager) {
    stats_.total_blocks = 0;
    stats_.blocks_propagated = 0;
    stats_.blocks_received = 0;
    stats_.duplicate_blocks_filtered = 0;
    stats_.start_time = std::chrono::steady_clock::now();
}

BlockMempool::~BlockMempool() {
    shutdown();
}

bool BlockMempool::initialize() {
    DEO_LOG_INFO(NETWORKING, "Initializing block mempool");
    DEO_LOG_INFO(NETWORKING, "Block mempool initialized successfully");
    return true;
}

void BlockMempool::shutdown() {
    DEO_LOG_INFO(NETWORKING, "Block mempool shutdown complete");
}

bool BlockMempool::addBlock(std::shared_ptr<core::Block> block) {
    if (!block) {
        DEO_LOG_ERROR(NETWORKING, "Cannot add null block to mempool");
        return false;
    }
    
    std::string block_hash = block->getHash();
    
    std::lock_guard<std::mutex> lock(block_mempool_mutex_);
    
    if (blocks_.find(block_hash) != blocks_.end()) {
        DEO_LOG_DEBUG(NETWORKING, "Block already in mempool: " + block_hash);
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.duplicate_blocks_filtered++;
        return false;
    }
    
    if (blocks_.size() >= MAX_BLOCK_MEMPOOL_SIZE) {
        DEO_LOG_WARNING(NETWORKING, "Block mempool size limit reached");
        return false;
    }
    
    blocks_[block_hash] = block;
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.total_blocks++;
    stats_.blocks_received++;
    
    DEO_LOG_DEBUG(NETWORKING, "Added block to mempool: " + block_hash);
    return true;
}

bool BlockMempool::removeBlock(const std::string& block_hash) {
    std::lock_guard<std::mutex> lock(block_mempool_mutex_);
    
    auto it = blocks_.find(block_hash);
    if (it != blocks_.end()) {
        blocks_.erase(it);
        DEO_LOG_DEBUG(NETWORKING, "Removed block from mempool: " + block_hash);
        return true;
    }
    
    return false;
}

std::shared_ptr<core::Block> BlockMempool::getBlock(const std::string& block_hash) const {
    std::lock_guard<std::mutex> lock(block_mempool_mutex_);
    
    auto it = blocks_.find(block_hash);
    if (it != blocks_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<core::Block>> BlockMempool::getBlocks() const {
    std::lock_guard<std::mutex> lock(block_mempool_mutex_);
    
    std::vector<std::shared_ptr<core::Block>> result;
    result.reserve(blocks_.size());
    
    for (const auto& [block_hash, block] : blocks_) {
        result.push_back(block);
    }
    
    return result;
}

bool BlockMempool::validateBlock(std::shared_ptr<core::Block> block) const {
    if (!block) {
        return false;
    }
    
    return block->validate();
}

void BlockMempool::propagateBlock(const std::string& block_hash) {
    auto block = getBlock(block_hash);
    if (!block) {
        return;
    }
    
    propagateToPeers(block_hash);
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.blocks_propagated++;
}

void BlockMempool::handleIncomingBlock(std::shared_ptr<core::Block> block, const std::string& peer_address) {
    if (!block) {
        peer_manager_->reportMisbehavior(peer_address, 0, 10);
        return;
    }
    
    std::string block_hash = block->getHash();
    
    if (addBlock(block)) {
        recordPropagation(block_hash, peer_address);
        propagateBlock(block_hash);
        
        DEO_LOG_DEBUG(NETWORKING, "Handled incoming block from " + peer_address + ": " + block_hash);
    }
}

void BlockMempool::broadcastNewBlock(std::shared_ptr<core::Block> block) {
    if (!block) {
        return;
    }
    
    std::string block_hash = block->getHash();
    
    if (addBlock(block)) {
        propagateBlock(block_hash);
        
        DEO_LOG_INFO(NETWORKING, "Broadcasted new block: " + block_hash);
    }
}

size_t BlockMempool::getBlockCount() const {
    std::lock_guard<std::mutex> lock(block_mempool_mutex_);
    return blocks_.size();
}

bool BlockMempool::hasBlock(const std::string& block_hash) const {
    std::lock_guard<std::mutex> lock(block_mempool_mutex_);
    return blocks_.find(block_hash) != blocks_.end();
}

std::vector<std::string> BlockMempool::getBlockHashes() const {
    std::lock_guard<std::mutex> lock(block_mempool_mutex_);
    
    std::vector<std::string> hashes;
    hashes.reserve(blocks_.size());
    
    for (const auto& [block_hash, block] : blocks_) {
        hashes.push_back(block_hash);
    }
    
    return hashes;
}

BlockMempool::BlockMempoolStats BlockMempool::getBlockMempoolStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void BlockMempool::propagateToPeers(const std::string& block_hash, const std::set<std::string>& exclude_peers) {
    auto block = getBlock(block_hash);
    if (!block) {
        return;
    }
    
    auto connected_peers = peer_manager_->getConnectedPeers();
    
    for (const auto& peer_key : connected_peers) {
        if (exclude_peers.find(peer_key) == exclude_peers.end()) {
            if (shouldPropagateToPeer(block_hash, peer_key)) {
                // Create block message using the correct message type
                auto block_message = std::make_unique<BlockMessage>(block);
                // Note: We would need to send the message through the network manager
                // For now, we'll just log the propagation attempt
                
                size_t colon_pos = peer_key.find(':');
                if (colon_pos != std::string::npos) {
                    std::string address = peer_key.substr(0, colon_pos);
                    // network_manager_->sendToPeer(address, *tx_message);
                    DEO_LOG_DEBUG(NETWORKING, "Transaction propagation to " + address + " (implementation pending)");
                    recordPropagation(block_hash, peer_key);
                }
            }
        }
    }
}

bool BlockMempool::shouldPropagateToPeer(const std::string& block_hash, const std::string& peer_address) const {
    std::lock_guard<std::mutex> lock(propagation_mutex_);
    
    auto it = propagation_map_.find(block_hash);
    if (it != propagation_map_.end()) {
        return it->second.find(peer_address) == it->second.end();
    }
    
    return true;
}

void BlockMempool::recordPropagation(const std::string& block_hash, const std::string& peer_address) {
    std::lock_guard<std::mutex> lock(propagation_mutex_);
    propagation_map_[block_hash].insert(peer_address);
}

} // namespace network
} // namespace deo
