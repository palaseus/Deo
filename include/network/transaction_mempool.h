/**
 * @file transaction_mempool.h
 * @brief Thread-safe transaction mempool with P2P propagation
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>

#include "core/transaction.h"
#include "network/tcp_network.h"
#include "network/peer_manager.h"
#include "utils/logger.h"

namespace deo {
namespace network {

/**
 * @brief Transaction mempool entry with metadata
 */
struct MempoolEntry {
    std::shared_ptr<core::Transaction> transaction;
    std::chrono::steady_clock::time_point received_time;
    std::chrono::steady_clock::time_point last_propagated;
    std::set<std::string> propagated_to_peers;
    uint32_t propagation_count;
    bool is_validated;
    
    MempoolEntry(std::shared_ptr<core::Transaction> tx) 
        : transaction(tx)
        , received_time(std::chrono::steady_clock::now())
        , last_propagated(std::chrono::steady_clock::now())
        , propagation_count(0)
        , is_validated(false) {
    }
};

/**
 * @brief Thread-safe transaction mempool with P2P propagation
 */
class TransactionMempool {
public:
    TransactionMempool(std::shared_ptr<TcpNetworkManager> network_manager,
                       std::shared_ptr<PeerManager> peer_manager);
    ~TransactionMempool();
    
    bool initialize();
    void shutdown();
    
    // Transaction management
    bool addTransaction(std::shared_ptr<core::Transaction> transaction);
    bool removeTransaction(const std::string& transaction_id);
    std::shared_ptr<core::Transaction> getTransaction(const std::string& transaction_id) const;
    std::vector<std::shared_ptr<core::Transaction>> getTransactions(size_t max_count = 1000) const;
    std::vector<std::shared_ptr<core::Transaction>> getTransactionsForBlock(size_t max_count = 100) const;
    
    // Transaction validation
    bool validateTransaction(std::shared_ptr<core::Transaction> transaction) const;
    void validateAllTransactions();
    
    // P2P propagation
    void propagateTransaction(const std::string& transaction_id);
    void handleIncomingTransaction(std::shared_ptr<core::Transaction> transaction, const std::string& peer_address);
    void broadcastNewTransaction(std::shared_ptr<core::Transaction> transaction);
    
    // Mempool queries
    size_t getTransactionCount() const;
    size_t getValidatedTransactionCount() const;
    bool hasTransaction(const std::string& transaction_id) const;
    std::vector<std::string> getTransactionIds() const;
    
    // Statistics
    struct MempoolStats {
        size_t total_transactions;
        size_t validated_transactions;
        size_t pending_validation;
        size_t transactions_propagated;
        size_t transactions_received;
        size_t duplicate_transactions_filtered;
        std::chrono::steady_clock::time_point start_time;
    };
    
    MempoolStats getMempoolStats() const;
    
    // Cleanup
    void cleanupExpiredTransactions();
    void cleanupPropagatedTransactions();

private:
    std::shared_ptr<TcpNetworkManager> network_manager_;
    std::shared_ptr<PeerManager> peer_manager_;
    
    mutable std::mutex mempool_mutex_;
    std::map<std::string, MempoolEntry> transactions_;
    
    // Validation queue
    mutable std::mutex validation_queue_mutex_;
    std::queue<std::string> validation_queue_;
    std::condition_variable validation_condition_;
    
    // Propagation tracking
    mutable std::mutex propagation_mutex_;
    std::map<std::string, std::set<std::string>> propagation_map_; // tx_id -> set of peer addresses
    
    // Statistics
    MempoolStats stats_;
    mutable std::mutex stats_mutex_;
    
    // Background threads
    std::atomic<bool> running_;
    std::thread validation_thread_;
    std::thread cleanup_thread_;
    
    // Configuration
    static constexpr size_t MAX_MEMPOOL_SIZE = 10000;
    static constexpr std::chrono::hours TRANSACTION_EXPIRY = std::chrono::hours(24);
    static constexpr std::chrono::minutes PROPAGATION_CLEANUP_INTERVAL = std::chrono::minutes(10);
    
    void validationLoop();
    void cleanupLoop();
    void propagateToPeers(const std::string& transaction_id, const std::set<std::string>& exclude_peers = {});
    bool shouldPropagateToPeer(const std::string& transaction_id, const std::string& peer_address) const;
    void recordPropagation(const std::string& transaction_id, const std::string& peer_address);
    std::string calculateTransactionPriority(std::shared_ptr<core::Transaction> transaction) const;
};

/**
 * @brief Block mempool for pending blocks
 */
class BlockMempool {
public:
    BlockMempool(std::shared_ptr<TcpNetworkManager> network_manager,
                 std::shared_ptr<PeerManager> peer_manager);
    ~BlockMempool();
    
    bool initialize();
    void shutdown();
    
    // Block management
    bool addBlock(std::shared_ptr<core::Block> block);
    bool removeBlock(const std::string& block_hash);
    std::shared_ptr<core::Block> getBlock(const std::string& block_hash) const;
    std::vector<std::shared_ptr<core::Block>> getBlocks() const;
    
    // Block validation
    bool validateBlock(std::shared_ptr<core::Block> block) const;
    
    // P2P propagation
    void propagateBlock(const std::string& block_hash);
    void handleIncomingBlock(std::shared_ptr<core::Block> block, const std::string& peer_address);
    void broadcastNewBlock(std::shared_ptr<core::Block> block);
    
    // Block queries
    size_t getBlockCount() const;
    bool hasBlock(const std::string& block_hash) const;
    std::vector<std::string> getBlockHashes() const;
    
    // Statistics
    struct BlockMempoolStats {
        size_t total_blocks;
        size_t blocks_propagated;
        size_t blocks_received;
        size_t duplicate_blocks_filtered;
        std::chrono::steady_clock::time_point start_time;
    };
    
    BlockMempoolStats getBlockMempoolStats() const;

private:
    std::shared_ptr<TcpNetworkManager> network_manager_;
    std::shared_ptr<PeerManager> peer_manager_;
    
    mutable std::mutex block_mempool_mutex_;
    std::map<std::string, std::shared_ptr<core::Block>> blocks_;
    
    // Propagation tracking
    mutable std::mutex propagation_mutex_;
    std::map<std::string, std::set<std::string>> propagation_map_; // block_hash -> set of peer addresses
    
    // Statistics
    BlockMempoolStats stats_;
    mutable std::mutex stats_mutex_;
    
    // Configuration
    static constexpr size_t MAX_BLOCK_MEMPOOL_SIZE = 100;
    static constexpr std::chrono::hours BLOCK_EXPIRY = std::chrono::hours(48);
    
    void propagateToPeers(const std::string& block_hash, const std::set<std::string>& exclude_peers = {});
    bool shouldPropagateToPeer(const std::string& block_hash, const std::string& peer_address) const;
    void recordPropagation(const std::string& block_hash, const std::string& peer_address);
};

} // namespace network
} // namespace deo
