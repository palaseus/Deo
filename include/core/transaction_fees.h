/**
 * @file transaction_fees.h
 * @brief Transaction fee calculation and priority management
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>

#include "transaction.h"

namespace deo {
namespace core {

/**
 * @brief Transaction fee structure
 */
struct TransactionFee {
    uint64_t base_fee;           ///< Base fee in satoshis
    uint64_t gas_fee;            ///< Gas fee in satoshis
    uint64_t priority_fee;       ///< Priority fee in satoshis
    uint64_t total_fee;          ///< Total fee in satoshis
    
    TransactionFee() : base_fee(0), gas_fee(0), priority_fee(0), total_fee(0) {}
    
    TransactionFee(uint64_t base, uint64_t gas, uint64_t priority)
        : base_fee(base), gas_fee(gas), priority_fee(priority) {
        total_fee = base_fee + gas_fee + priority_fee;
    }
};

/**
 * @brief Fee calculation parameters
 */
struct FeeParameters {
    uint64_t base_fee_per_byte;      ///< Base fee per byte in satoshis
    uint64_t gas_price;              ///< Gas price in satoshis per unit
    uint64_t priority_fee_multiplier; ///< Priority fee multiplier
    uint64_t min_fee;                ///< Minimum transaction fee
    uint64_t max_fee;                ///< Maximum transaction fee
    
    FeeParameters() 
        : base_fee_per_byte(1)
        , gas_price(20)
        , priority_fee_multiplier(2)
        , min_fee(1000)
        , max_fee(1000000000) {} // 10 BTC max fee
};

/**
 * @brief Transaction priority calculation
 */
struct TransactionPriority {
    double priority_score;           ///< Calculated priority score
    uint64_t fee_per_byte;          ///< Fee per byte ratio
    uint64_t age_in_blocks;         ///< Age in blocks
    uint64_t size_in_bytes;         ///< Transaction size in bytes
    std::chrono::system_clock::time_point timestamp; ///< Transaction timestamp
    
    TransactionPriority() 
        : priority_score(0.0)
        , fee_per_byte(0)
        , age_in_blocks(0)
        , size_in_bytes(0) {}
};

/**
 * @brief Mempool eviction policy
 */
enum class EvictionPolicy {
    OLDEST_FIRST,           ///< Remove oldest transactions first
    LOWEST_FEE_FIRST,       ///< Remove lowest fee transactions first
    LOWEST_PRIORITY_FIRST,  ///< Remove lowest priority transactions first
    SIZE_BASED,             ///< Remove largest transactions first
    HYBRID                  ///< Combination of factors
};

/**
 * @brief Mempool configuration
 */
struct MempoolConfig {
    size_t max_size_bytes;          ///< Maximum mempool size in bytes
    size_t max_transactions;        ///< Maximum number of transactions
    std::chrono::hours max_age;     ///< Maximum transaction age
    EvictionPolicy eviction_policy; ///< Eviction policy
    FeeParameters fee_params;       ///< Fee calculation parameters
    
    MempoolConfig() 
        : max_size_bytes(100 * 1024 * 1024) // 100 MB
        , max_transactions(10000)
        , max_age(std::chrono::hours(24))
        , eviction_policy(EvictionPolicy::HYBRID) {}
};

/**
 * @brief Transaction fee calculator
 */
class TransactionFeeCalculator {
public:
    TransactionFeeCalculator();
    ~TransactionFeeCalculator();
    
    // Fee calculation
    TransactionFee calculateFee(const Transaction& transaction, const FeeParameters& params) const;
    uint64_t calculateInputValueFromUTXO(const Transaction& transaction) const;
    uint64_t calculateBaseFee(const Transaction& transaction, uint64_t fee_per_byte) const;
    uint64_t calculateGasFee(const Transaction& transaction, uint64_t gas_price) const;
    uint64_t calculatePriorityFee(const Transaction& transaction, uint64_t multiplier) const;
    
    // Fee validation
    bool validateFee(const Transaction& transaction, const TransactionFee& fee, 
                    const FeeParameters& params) const;
    bool isFeeSufficient(const Transaction& transaction, const TransactionFee& fee) const;
    
    // Fee estimation
    TransactionFee estimateFee(const Transaction& transaction, 
                              const std::vector<TransactionFee>& recent_fees) const;
    uint64_t getRecommendedGasPrice(const std::vector<TransactionFee>& recent_fees) const;
    
    // Utility methods
    uint64_t calculateTransactionSize(const Transaction& transaction) const;
    uint64_t calculateGasUsage(const Transaction& transaction) const;
    
private:
};

/**
 * @brief Transaction priority calculator
 */
class TransactionPriorityCalculator {
public:
    TransactionPriorityCalculator();
    ~TransactionPriorityCalculator();
    
    // Priority calculation
    TransactionPriority calculatePriority(const Transaction& transaction, 
                                         const TransactionFee& fee,
                                         uint64_t current_block_height) const;
    double calculatePriorityScore(const Transaction& transaction,
                                 const TransactionFee& fee,
                                 uint64_t age_in_blocks) const;
    
    // Priority comparison
    bool isHigherPriority(const TransactionPriority& a, const TransactionPriority& b) const;
    std::vector<std::shared_ptr<Transaction>> sortByPriority(const std::vector<std::shared_ptr<Transaction>>& transactions,
                                           const std::map<std::string, TransactionFee>& fees,
                                           uint64_t current_block_height) const;
    
    // Utility methods
    uint64_t calculateTransactionSize(const Transaction& transaction) const;
    uint64_t calculateGasUsage(const Transaction& transaction) const;
    
private:
    double calculateFeePerByte(const Transaction& transaction, const TransactionFee& fee) const;
    uint64_t calculateAgeInBlocks(const Transaction& transaction, uint64_t current_height) const;
};

/**
 * @brief Enhanced mempool with fee and priority management
 */
class EnhancedMempool {
public:
    EnhancedMempool();
    ~EnhancedMempool();
    
    // Initialization
    bool initialize(const MempoolConfig& config);
    void shutdown();
    
    // Transaction management
    bool addTransaction(const Transaction& transaction, const TransactionFee& fee);
    bool removeTransaction(const std::string& transaction_id);
    bool hasTransaction(const std::string& transaction_id) const;
    std::vector<std::shared_ptr<Transaction>> getTransactions() const;
    
    // Priority management
    std::vector<std::shared_ptr<Transaction>> getTransactionsByPriority(size_t max_count = 0) const;
    std::vector<std::shared_ptr<Transaction>> getTransactionsForBlock(size_t max_size_bytes) const;
    TransactionPriority getTransactionPriority(const std::string& transaction_id) const;
    
    // Fee management
    std::map<std::string, TransactionFee> getAllFees() const;
    TransactionFee getTransactionFee(const std::string& transaction_id) const;
    bool updateTransactionFee(const std::string& transaction_id, const TransactionFee& new_fee);
    
    // Eviction management
    void evictTransactions();
    size_t evictOldTransactions();
    size_t evictLowFeeTransactions();
    size_t evictLargeTransactions();
    size_t evictByPriority();
    
    // Statistics
    size_t getTransactionCount() const;
    size_t getTotalSizeBytes() const;
    std::string getMempoolStatistics() const;
    std::vector<TransactionFee> getRecentFees(size_t count = 100) const;
    
    // Configuration
    void updateConfig(const MempoolConfig& config);
    MempoolConfig getConfig() const;
    
private:
    // Core components
    std::unique_ptr<TransactionFeeCalculator> fee_calculator_;
    std::unique_ptr<TransactionPriorityCalculator> priority_calculator_;
    
    // Storage
    std::map<std::string, std::shared_ptr<Transaction>> transactions_;
    std::map<std::string, TransactionFee> transaction_fees_;
    std::map<std::string, TransactionPriority> transaction_priorities_;
    std::map<std::string, std::chrono::system_clock::time_point> transaction_timestamps_;
    
    // Configuration
    MempoolConfig config_;
    uint64_t current_block_height_;
    
    // Statistics
    size_t total_size_bytes_;
    std::vector<TransactionFee> recent_fees_;
    
    // Thread safety
    mutable std::mutex peers_mutex_;
    
    // Internal methods
    void updateTransactionPriority(const std::string& transaction_id);
    void updateTotalSize();
    void addToRecentFees(const TransactionFee& fee);
    bool shouldEvictTransaction(const std::string& transaction_id) const;
    std::vector<std::string> selectTransactionsForEviction() const;
    bool removeTransactionUnlocked(const std::string& transaction_id);
    
    // Eviction strategies
    std::vector<std::string> evictOldestTransactions() const;
    std::vector<std::string> evictLowestFeeTransactions() const;
    std::vector<std::string> evictLowestPriorityTransactions() const;
    std::vector<std::string> evictLargestTransactions() const;
    std::vector<std::string> evictHybrid();
};

/**
 * @brief Coinbase reward calculator
 */
class CoinbaseRewardCalculator {
public:
    CoinbaseRewardCalculator();
    ~CoinbaseRewardCalculator();
    
    // Reward calculation
    uint64_t calculateReward(uint64_t block_height) const;
    uint64_t calculateTotalReward(uint64_t block_height, const std::vector<TransactionFee>& fees) const;
    
    // Halving management
    uint64_t getHalvingInterval() const;
    uint64_t getNextHalvingBlock(uint64_t current_height) const;
    uint64_t getHalvingCount(uint64_t block_height) const;
    
    // Configuration
    void setInitialReward(uint64_t reward);
    void setHalvingInterval(uint64_t interval);
    
private:
    uint64_t initial_reward_;       ///< Initial coinbase reward in satoshis
    uint64_t halving_interval_;     ///< Blocks between halvings
    
    uint64_t calculateHalvedReward(uint64_t halving_count) const;
};

} // namespace core
} // namespace deo
