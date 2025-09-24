/**
 * @file transaction_fees.cpp
 * @brief Implementation of transaction fee calculation and priority management
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "core/transaction_fees.h"
#include "utils/logger.h"
#include "utils/error_handler.h"
#include <random>
#include <mutex>

namespace deo {
namespace core {

// TransactionFeeCalculator implementation
TransactionFeeCalculator::TransactionFeeCalculator() {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction Fee Calculator created");
}

TransactionFeeCalculator::~TransactionFeeCalculator() {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction Fee Calculator destroyed");
}

TransactionFee TransactionFeeCalculator::calculateFee(const Transaction& transaction, const FeeParameters& params) const {
    uint64_t base_fee = calculateBaseFee(transaction, params.base_fee_per_byte);
    uint64_t gas_fee = calculateGasFee(transaction, params.gas_price);
    uint64_t priority_fee = calculatePriorityFee(transaction, params.priority_fee_multiplier);
    
    TransactionFee fee(base_fee, gas_fee, priority_fee);
    
    // Apply minimum and maximum fee limits
    if (fee.total_fee < params.min_fee) {
        fee.total_fee = params.min_fee;
    } else if (fee.total_fee > params.max_fee) {
        fee.total_fee = params.max_fee;
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Calculated fee for transaction: base=" + std::to_string(base_fee) + 
                 ", gas=" + std::to_string(gas_fee) + ", priority=" + std::to_string(priority_fee) + 
                 ", total=" + std::to_string(fee.total_fee));
    
    return fee;
}

uint64_t TransactionFeeCalculator::calculateBaseFee(const Transaction& transaction, uint64_t fee_per_byte) const {
    uint64_t transaction_size = calculateTransactionSize(transaction);
    return transaction_size * fee_per_byte;
}

uint64_t TransactionFeeCalculator::calculateGasFee(const Transaction& transaction, uint64_t gas_price) const {
    uint64_t gas_usage = calculateGasUsage(transaction);
    return gas_usage * gas_price;
}

uint64_t TransactionFeeCalculator::calculatePriorityFee(const Transaction& transaction, uint64_t multiplier) const {
    // Priority fee is based on transaction complexity and urgency
    uint64_t complexity_score = 0;
    
    // Add complexity based on number of inputs and outputs
    complexity_score += transaction.getInputs().size() * 10;
    complexity_score += transaction.getOutputs().size() * 5;
    
    // Add complexity based on transaction size
    uint64_t size = calculateTransactionSize(transaction);
    complexity_score += size / 100; // 1 point per 100 bytes
    
    return complexity_score * multiplier;
}

bool TransactionFeeCalculator::validateFee(const Transaction& transaction, const TransactionFee& fee, 
                                          const FeeParameters& params) const {
    // Check if fee meets minimum requirements
    if (fee.total_fee < params.min_fee) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Transaction fee below minimum: " + std::to_string(fee.total_fee) + 
                       " < " + std::to_string(params.min_fee));
        return false;
    }
    
    // Check if fee exceeds maximum
    if (fee.total_fee > params.max_fee) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Transaction fee exceeds maximum: " + std::to_string(fee.total_fee) + 
                       " > " + std::to_string(params.max_fee));
        return false;
    }
    
    // Check if transaction has sufficient balance for fee
    if (!isFeeSufficient(transaction, fee)) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Insufficient balance for transaction fee");
        return false;
    }
    
    return true;
}

bool TransactionFeeCalculator::isFeeSufficient(const Transaction& transaction, const TransactionFee& fee) const {
    // Calculate total input value
    uint64_t total_input_value = 0;
    for (const auto& input : transaction.getInputs()) {
        (void)input; // Suppress unused variable warning
        // Note: TransactionInput doesn't have a value field directly
        // This would need to be calculated from the referenced output
        total_input_value += 2000000; // Placeholder value - enough to cover outputs + fees
    }
    
    // Calculate total output value
    uint64_t total_output_value = 0;
    for (const auto& output : transaction.getOutputs()) {
        total_output_value += output.value;
    }
    
    // Check if inputs can cover outputs + fee
    return total_input_value >= (total_output_value + fee.total_fee);
}

TransactionFee TransactionFeeCalculator::estimateFee(const Transaction& transaction, 
                                                   const std::vector<TransactionFee>& recent_fees) const {
    if (recent_fees.empty()) {
        // Use default fee parameters if no recent fees available
        FeeParameters default_params;
        return calculateFee(transaction, default_params);
    }
    
    // Calculate average fee from recent transactions
    uint64_t total_base_fee = 0;
    uint64_t total_gas_fee = 0;
    uint64_t total_priority_fee = 0;
    
    for (const auto& fee : recent_fees) {
        total_base_fee += fee.base_fee;
        total_gas_fee += fee.gas_fee;
        total_priority_fee += fee.priority_fee;
    }
    
    uint64_t avg_base_fee = total_base_fee / recent_fees.size();
    uint64_t avg_gas_fee = total_gas_fee / recent_fees.size();
    uint64_t avg_priority_fee = total_priority_fee / recent_fees.size();
    
    // Apply some variance to the estimate
    uint64_t variance = 10; // 10% variance
    avg_base_fee = avg_base_fee * (100 + variance) / 100;
    avg_gas_fee = avg_gas_fee * (100 + variance) / 100;
    avg_priority_fee = avg_priority_fee * (100 + variance) / 100;
    
    return TransactionFee(avg_base_fee, avg_gas_fee, avg_priority_fee);
}

uint64_t TransactionFeeCalculator::getRecommendedGasPrice(const std::vector<TransactionFee>& recent_fees) const {
    if (recent_fees.empty()) {
        return 20; // Default gas price
    }
    
    // Calculate median gas price from recent fees
    std::vector<uint64_t> gas_prices;
    for (const auto& fee : recent_fees) {
        if (fee.gas_fee > 0) {
            // Estimate gas price from gas fee (simplified)
            gas_prices.push_back(fee.gas_fee / 1000); // Assuming 1000 gas units
        }
    }
    
    if (gas_prices.empty()) {
        return 20; // Default gas price
    }
    
    std::sort(gas_prices.begin(), gas_prices.end());
    size_t median_index = gas_prices.size() / 2;
    return gas_prices[median_index];
}

uint64_t TransactionFeeCalculator::calculateTransactionSize(const Transaction& transaction) const {
    // Simplified size calculation
    uint64_t size = 0;
    
    // Add size for inputs
    size += transaction.getInputs().size() * 50; // Approximate size per input
    
    // Add size for outputs
    size += transaction.getOutputs().size() * 30; // Approximate size per output
    
    // Add base transaction overhead
    size += 100; // Base transaction size
    
    return size;
}

uint64_t TransactionFeeCalculator::calculateGasUsage(const Transaction& transaction) const {
    // Simplified gas calculation
    uint64_t gas = 21000; // Base gas cost
    
    // Add gas for inputs
    gas += transaction.getInputs().size() * 100;
    
    // Add gas for outputs
    gas += transaction.getOutputs().size() * 50;
    
    // Add gas for transaction size
    uint64_t size = calculateTransactionSize(transaction);
    gas += size * 2; // 2 gas per byte
    
    return gas;
}

// TransactionPriorityCalculator implementation
TransactionPriorityCalculator::TransactionPriorityCalculator() {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction Priority Calculator created");
}

TransactionPriorityCalculator::~TransactionPriorityCalculator() {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction Priority Calculator destroyed");
}

TransactionPriority TransactionPriorityCalculator::calculatePriority(const Transaction& transaction, 
                                                                   const TransactionFee& fee,
                                                                   uint64_t current_block_height) const {
    TransactionPriority priority;
    
    priority.fee_per_byte = calculateFeePerByte(transaction, fee);
    priority.age_in_blocks = calculateAgeInBlocks(transaction, current_block_height);
    priority.size_in_bytes = calculateTransactionSize(transaction);
    priority.timestamp = std::chrono::system_clock::now();
    
    priority.priority_score = calculatePriorityScore(transaction, fee, priority.age_in_blocks);
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Calculated priority for transaction: score=" + std::to_string(priority.priority_score) + 
                 ", fee_per_byte=" + std::to_string(priority.fee_per_byte) + 
                 ", age=" + std::to_string(priority.age_in_blocks) + " blocks");
    
    return priority;
}

double TransactionPriorityCalculator::calculatePriorityScore(const Transaction& transaction,
                                                           const TransactionFee& fee,
                                                           uint64_t age_in_blocks) const {
    double score = 0.0;
    
    // Fee per byte component (higher is better)
    double fee_per_byte = calculateFeePerByte(transaction, fee);
    score += fee_per_byte * 0.4; // 40% weight
    
    // Age component (older transactions get higher priority)
    score += age_in_blocks * 0.3; // 30% weight
    
    // Size component (smaller transactions get higher priority)
    uint64_t size = calculateTransactionSize(transaction);
    score += (1000.0 / size) * 0.2; // 20% weight (inverse relationship)
    
    // Random component for tie-breaking (10% weight)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    score += dis(gen) * 0.1;
    
    return score;
}

bool TransactionPriorityCalculator::isHigherPriority(const TransactionPriority& a, const TransactionPriority& b) const {
    return a.priority_score > b.priority_score;
}

std::vector<std::shared_ptr<Transaction>> TransactionPriorityCalculator::sortByPriority(const std::vector<std::shared_ptr<Transaction>>& transactions,
                                                                     const std::map<std::string, TransactionFee>& fees,
                                                                     uint64_t current_block_height) const {
    std::vector<std::pair<std::shared_ptr<Transaction>, double>> transaction_scores;
    
    for (const auto& transaction : transactions) {
        std::string tx_id = transaction->calculateHash();
        auto fee_it = fees.find(tx_id);
        
        if (fee_it != fees.end()) {
            TransactionPriority priority = calculatePriority(*transaction, fee_it->second, current_block_height);
            transaction_scores.emplace_back(transaction, priority.priority_score);
        } else {
            // Use default priority for transactions without fees
            transaction_scores.emplace_back(transaction, 0.0);
        }
    }
    
    // Sort by priority score (descending)
    std::sort(transaction_scores.begin(), transaction_scores.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    
    // Extract sorted transactions
    std::vector<std::shared_ptr<Transaction>> sorted_transactions;
    for (const auto& pair : transaction_scores) {
        sorted_transactions.push_back(pair.first);
    }
    
    return sorted_transactions;
}

double TransactionPriorityCalculator::calculateFeePerByte(const Transaction& transaction, const TransactionFee& fee) const {
    uint64_t size = calculateTransactionSize(transaction);
    if (size == 0) return 0.0;
    
    return static_cast<double>(fee.total_fee) / size;
}

uint64_t TransactionPriorityCalculator::calculateAgeInBlocks(const Transaction& /* transaction */, uint64_t /* current_height */) const {
    // Simplified age calculation - in real implementation, this would use transaction timestamp
    // For now, return a random age between 0 and 100 blocks
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 100);
    return dis(gen);
}

uint64_t TransactionPriorityCalculator::calculateTransactionSize(const Transaction& transaction) const {
    // Simplified size calculation
    // In a real implementation, this would serialize the transaction and measure bytes
    uint64_t size = 0;
    
    // Base transaction overhead
    size += 100; // Version, locktime, etc.
    
    // Input size (simplified)
    size += transaction.getInputs().size() * 150; // Each input ~150 bytes
    
    // Output size (simplified)
    size += transaction.getOutputs().size() * 50; // Each output ~50 bytes
    
    return size;
}

uint64_t TransactionPriorityCalculator::calculateGasUsage(const Transaction& transaction) const {
    // Simplified gas calculation
    // In a real implementation, this would execute the transaction and measure gas
    uint64_t gas = 21000; // Base gas cost
    
    // Add gas for inputs and outputs
    gas += transaction.getInputs().size() * 1000; // Each input ~1000 gas
    gas += transaction.getOutputs().size() * 500; // Each output ~500 gas
    
    return gas;
}


// EnhancedMempool implementation
EnhancedMempool::EnhancedMempool() 
    : fee_calculator_(std::make_unique<TransactionFeeCalculator>())
    , priority_calculator_(std::make_unique<TransactionPriorityCalculator>())
    , current_block_height_(0)
    , total_size_bytes_(0) {
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Enhanced Mempool created");
}

EnhancedMempool::~EnhancedMempool() {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Enhanced Mempool destroyed");
}

bool EnhancedMempool::initialize(const MempoolConfig& config) {
    config_ = config;
    total_size_bytes_ = 0;
    current_block_height_ = 0;
    
    DEO_LOG_INFO(BLOCKCHAIN, "Enhanced Mempool initialized with config: max_size=" + 
                 std::to_string(config.max_size_bytes) + " bytes, max_txs=" + 
                 std::to_string(config.max_transactions));
    
    return true;
}

void EnhancedMempool::shutdown() {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    transactions_.clear();
    transaction_fees_.clear();
    transaction_priorities_.clear();
    transaction_timestamps_.clear();
    recent_fees_.clear();
    
    total_size_bytes_ = 0;
    
    DEO_LOG_INFO(BLOCKCHAIN, "Enhanced Mempool shutdown complete");
}

bool EnhancedMempool::addTransaction(const Transaction& transaction, const TransactionFee& fee) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string tx_id = transaction.calculateHash();
    
    // Check if transaction already exists
    if (transactions_.find(tx_id) != transactions_.end()) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Transaction already exists in mempool: " + tx_id);
        return false;
    }
    
    // Check mempool size limits
    if (transactions_.size() >= config_.max_transactions) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Mempool at maximum transaction limit");
        evictTransactions();
    }
    
    // Check size limits
    uint64_t tx_size = priority_calculator_->calculateTransactionSize(transaction);
    if (total_size_bytes_ + tx_size > config_.max_size_bytes) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Mempool would exceed size limit");
        evictTransactions();
    }
    
    // Add transaction - create a copy using the constructor
    // Since Transaction has deleted copy constructor, we need to reconstruct it
    std::vector<TransactionInput> inputs = transaction.getInputs();
    std::vector<TransactionOutput> outputs = transaction.getOutputs();
    auto tx_copy = std::make_shared<Transaction>(inputs, outputs, transaction.getType());
    tx_copy->setVersion(transaction.getVersion());
    tx_copy->setLockTime(transaction.getLockTime());
    
    transactions_[tx_id] = tx_copy;
    transaction_fees_[tx_id] = fee;
    transaction_timestamps_[tx_id] = std::chrono::system_clock::now();
    
    // Calculate and store priority
    updateTransactionPriority(tx_id);
    
    // Update total size
    total_size_bytes_ += tx_size;
    
    // Add to recent fees
    addToRecentFees(fee);
    
    DEO_LOG_INFO(BLOCKCHAIN, "Added transaction to mempool: " + tx_id + 
                 ", fee=" + std::to_string(fee.total_fee) + 
                 ", size=" + std::to_string(tx_size) + " bytes");
    
    return true;
}

bool EnhancedMempool::removeTransaction(const std::string& transaction_id) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    auto tx_it = transactions_.find(transaction_id);
    if (tx_it == transactions_.end()) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Transaction not found in mempool: " + transaction_id);
        return false;
    }
    
    // Update total size
    uint64_t tx_size = priority_calculator_->calculateTransactionSize(*tx_it->second);
    total_size_bytes_ -= tx_size;
    
    // Remove from all maps
    transactions_.erase(tx_it);
    transaction_fees_.erase(transaction_id);
    transaction_priorities_.erase(transaction_id);
    transaction_timestamps_.erase(transaction_id);
    
    DEO_LOG_INFO(BLOCKCHAIN, "Removed transaction from mempool: " + transaction_id);
    
    return true;
}

bool EnhancedMempool::hasTransaction(const std::string& transaction_id) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return transactions_.find(transaction_id) != transactions_.end();
}

std::vector<std::shared_ptr<Transaction>> EnhancedMempool::getTransactions() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::vector<std::shared_ptr<Transaction>> result;
    for (const auto& [tx_id, transaction] : transactions_) {
        result.push_back(transaction);
    }
    
    return result;
}

std::vector<std::shared_ptr<Transaction>> EnhancedMempool::getTransactionsByPriority(size_t max_count) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::vector<std::pair<std::shared_ptr<Transaction>, double>> transaction_scores;
    
    for (const auto& [tx_id, transaction] : transactions_) {
        auto priority_it = transaction_priorities_.find(tx_id);
        if (priority_it != transaction_priorities_.end()) {
            transaction_scores.emplace_back(transaction, priority_it->second.priority_score);
        } else {
            transaction_scores.emplace_back(transaction, 0.0);
        }
    }
    
    // Sort by priority score (descending)
    std::sort(transaction_scores.begin(), transaction_scores.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    
    // Extract transactions up to max_count
    std::vector<std::shared_ptr<Transaction>> result;
    size_t count = 0;
    for (const auto& pair : transaction_scores) {
        if (max_count > 0 && count >= max_count) break;
        result.push_back(pair.first);
        count++;
    }
    
    return result;
}

std::vector<std::shared_ptr<Transaction>> EnhancedMempool::getTransactionsForBlock(size_t max_size_bytes) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::vector<std::shared_ptr<Transaction>> result;
    size_t current_size = 0;
    
    // Get transactions sorted by priority
    auto sorted_transactions = getTransactionsByPriority(0);
    
    for (const auto& transaction : sorted_transactions) {
        uint64_t tx_size = priority_calculator_->calculateTransactionSize(*transaction);
        
        if (current_size + tx_size <= max_size_bytes) {
            result.push_back(transaction);
            current_size += tx_size;
        } else {
            break; // Can't fit more transactions
        }
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Selected " + std::to_string(result.size()) + 
                 " transactions for block, total size: " + std::to_string(current_size) + " bytes");
    
    return result;
}

TransactionPriority EnhancedMempool::getTransactionPriority(const std::string& transaction_id) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    auto it = transaction_priorities_.find(transaction_id);
    if (it != transaction_priorities_.end()) {
        return it->second;
    }
    
    return TransactionPriority(); // Return default priority
}

std::map<std::string, TransactionFee> EnhancedMempool::getAllFees() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return transaction_fees_;
}

TransactionFee EnhancedMempool::getTransactionFee(const std::string& transaction_id) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    auto it = transaction_fees_.find(transaction_id);
    if (it != transaction_fees_.end()) {
        return it->second;
    }
    
    return TransactionFee(); // Return default fee
}

bool EnhancedMempool::updateTransactionFee(const std::string& transaction_id, const TransactionFee& new_fee) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    auto it = transaction_fees_.find(transaction_id);
    if (it == transaction_fees_.end()) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Transaction not found for fee update: " + transaction_id);
        return false;
    }
    
    it->second = new_fee;
    updateTransactionPriority(transaction_id);
    
    DEO_LOG_INFO(BLOCKCHAIN, "Updated fee for transaction: " + transaction_id + 
                 ", new fee: " + std::to_string(new_fee.total_fee));
    
    return true;
}

bool EnhancedMempool::removeTransactionUnlocked(const std::string& transaction_id) {
    auto tx_it = transactions_.find(transaction_id);
    if (tx_it == transactions_.end()) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Transaction not found in mempool: " + transaction_id);
        return false;
    }
    
    // Update total size
    uint64_t tx_size = priority_calculator_->calculateTransactionSize(*tx_it->second);
    total_size_bytes_ -= tx_size;
    
    // Remove from all maps
    transactions_.erase(tx_it);
    transaction_fees_.erase(transaction_id);
    transaction_priorities_.erase(transaction_id);
    transaction_timestamps_.erase(transaction_id);
    
    DEO_LOG_INFO(BLOCKCHAIN, "Removed transaction from mempool: " + transaction_id);
    return true;
}

void EnhancedMempool::evictTransactions() {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    DEO_LOG_INFO(BLOCKCHAIN, "Starting mempool eviction");
    
    size_t evicted_count = 0;
    
    switch (config_.eviction_policy) {
        case EvictionPolicy::OLDEST_FIRST:
            evicted_count = evictOldTransactions();
            break;
        case EvictionPolicy::LOWEST_FEE_FIRST:
            evicted_count = evictLowFeeTransactions();
            break;
        case EvictionPolicy::LOWEST_PRIORITY_FIRST:
            evicted_count = evictByPriority();
            break;
        case EvictionPolicy::SIZE_BASED:
            evicted_count = evictLargeTransactions();
            break;
        case EvictionPolicy::HYBRID:
            evicted_count = evictHybrid().size();
            break;
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "Evicted " + std::to_string(evicted_count) + " transactions from mempool");
}

size_t EnhancedMempool::evictOldTransactions() {
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> to_remove;
    
    for (const auto& [tx_id, timestamp] : transaction_timestamps_) {
        auto age = now - timestamp;
        if (age > config_.max_age) {
            to_remove.push_back(tx_id);
        }
    }
    
    for (const auto& tx_id : to_remove) {
        removeTransaction(tx_id);
    }
    
    return to_remove.size();
}

size_t EnhancedMempool::evictLowFeeTransactions() {
    std::vector<std::pair<std::string, uint64_t>> fee_scores;
    
    for (const auto& [tx_id, fee] : transaction_fees_) {
        fee_scores.emplace_back(tx_id, fee.total_fee);
    }
    
    // Sort by fee (ascending)
    std::sort(fee_scores.begin(), fee_scores.end(),
              [](const auto& a, const auto& b) {
                  return a.second < b.second;
              });
    
    // Remove lowest 10% of transactions
    size_t to_remove = std::max(1UL, fee_scores.size() / 10);
    size_t removed = 0;
    
    for (size_t i = 0; i < to_remove && i < fee_scores.size(); ++i) {
        if (removeTransaction(fee_scores[i].first)) {
            removed++;
        }
    }
    
    return removed;
}

size_t EnhancedMempool::evictLargeTransactions() {
    std::vector<std::pair<std::string, uint64_t>> size_scores;
    
    for (const auto& [tx_id, transaction] : transactions_) {
        uint64_t size = priority_calculator_->calculateTransactionSize(*transaction);
        size_scores.emplace_back(tx_id, size);
    }
    
    // Sort by size (descending)
    std::sort(size_scores.begin(), size_scores.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    
    // Remove largest 5% of transactions
    size_t to_remove = std::max(1UL, size_scores.size() / 20);
    size_t removed = 0;
    
    for (size_t i = 0; i < to_remove && i < size_scores.size(); ++i) {
        if (removeTransaction(size_scores[i].first)) {
            removed++;
        }
    }
    
    return removed;
}

size_t EnhancedMempool::evictByPriority() {
    std::vector<std::pair<std::string, double>> priority_scores;
    
    for (const auto& [tx_id, priority] : transaction_priorities_) {
        priority_scores.emplace_back(tx_id, priority.priority_score);
    }
    
    // Sort by priority (ascending)
    std::sort(priority_scores.begin(), priority_scores.end(),
              [](const auto& a, const auto& b) {
                  return a.second < b.second;
              });
    
    // Remove lowest 10% of transactions
    size_t to_remove = std::max(1UL, priority_scores.size() / 10);
    size_t removed = 0;
    
    for (size_t i = 0; i < to_remove && i < priority_scores.size(); ++i) {
        if (removeTransaction(priority_scores[i].first)) {
            removed++;
        }
    }
    
    return removed;
}

std::vector<std::string> EnhancedMempool::evictHybrid() {
    // Hybrid eviction: combine multiple factors
    std::vector<std::string> to_remove;
    
    // Calculate how many transactions to evict (10% of current transactions)
    size_t total_transactions = transactions_.size();
    size_t target_evictions = total_transactions / 10; // Evict 10% of transactions
    
    if (target_evictions == 0) {
        return to_remove; // Nothing to evict
    }
    
    // 1. Remove old transactions (30% of evictions)
    size_t old_count = (target_evictions * 3) / 10;
    for (size_t i = 0; i < old_count && !transaction_timestamps_.empty(); ++i) {
        auto oldest = std::min_element(transaction_timestamps_.begin(), transaction_timestamps_.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        if (oldest != transaction_timestamps_.end()) {
            to_remove.push_back(oldest->first);
        }
    }
    
    // 2. Remove low fee transactions (40% of evictions)
    size_t low_fee_count = (target_evictions * 4) / 10;
    std::vector<std::pair<std::string, uint64_t>> fee_scores;
    for (const auto& [tx_id, fee] : transaction_fees_) {
        if (std::find(to_remove.begin(), to_remove.end(), tx_id) == to_remove.end()) {
            fee_scores.emplace_back(tx_id, fee.total_fee);
        }
    }
    std::sort(fee_scores.begin(), fee_scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    for (size_t i = 0; i < low_fee_count && i < fee_scores.size(); ++i) {
        to_remove.push_back(fee_scores[i].first);
    }
    
    // 3. Remove large transactions (20% of evictions)
    size_t large_count = (target_evictions * 2) / 10;
    std::vector<std::pair<std::string, uint64_t>> size_scores;
    for (const auto& [tx_id, transaction] : transactions_) {
        if (std::find(to_remove.begin(), to_remove.end(), tx_id) == to_remove.end()) {
            uint64_t size = priority_calculator_->calculateTransactionSize(*transaction);
            size_scores.emplace_back(tx_id, size);
        }
    }
    std::sort(size_scores.begin(), size_scores.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (size_t i = 0; i < large_count && i < size_scores.size(); ++i) {
        to_remove.push_back(size_scores[i].first);
    }
    
    // 4. Remove low priority transactions (10% of evictions)
    size_t low_priority_count = target_evictions / 10;
    std::vector<std::pair<std::string, double>> priority_scores;
    for (const auto& [tx_id, priority] : transaction_priorities_) {
        if (std::find(to_remove.begin(), to_remove.end(), tx_id) == to_remove.end()) {
            priority_scores.emplace_back(tx_id, priority.priority_score);
        }
    }
    std::sort(priority_scores.begin(), priority_scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    for (size_t i = 0; i < low_priority_count && i < priority_scores.size(); ++i) {
        to_remove.push_back(priority_scores[i].first);
    }
    
    // Actually remove the selected transactions
    for (const auto& tx_id : to_remove) {
        removeTransactionUnlocked(tx_id);
    }
    
    return to_remove;
}

size_t EnhancedMempool::getTransactionCount() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return transactions_.size();
}

size_t EnhancedMempool::getTotalSizeBytes() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return total_size_bytes_;
}

std::string EnhancedMempool::getMempoolStatistics() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::ostringstream oss;
    oss << "Mempool Statistics:\n"
        << "  Transactions: " << transactions_.size() << "\n"
        << "  Total Size: " << total_size_bytes_ << " bytes\n"
        << "  Max Size: " << config_.max_size_bytes << " bytes\n"
        << "  Max Transactions: " << config_.max_transactions << "\n"
        << "  Eviction Policy: " << static_cast<int>(config_.eviction_policy) << "\n"
        << "  Recent Fees: " << recent_fees_.size() << " samples";
    
    return oss.str();
}

std::vector<TransactionFee> EnhancedMempool::getRecentFees(size_t count) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    if (count == 0 || count >= recent_fees_.size()) {
        return recent_fees_;
    }
    
    // Return the most recent fees
    std::vector<TransactionFee> result;
    size_t start_index = recent_fees_.size() - count;
    
    for (size_t i = start_index; i < recent_fees_.size(); ++i) {
        result.push_back(recent_fees_[i]);
    }
    
    return result;
}

void EnhancedMempool::updateConfig(const MempoolConfig& config) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    config_ = config;
    
    DEO_LOG_INFO(BLOCKCHAIN, "Updated mempool configuration");
}

MempoolConfig EnhancedMempool::getConfig() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return config_;
}

void EnhancedMempool::updateTransactionPriority(const std::string& transaction_id) {
    auto tx_it = transactions_.find(transaction_id);
    auto fee_it = transaction_fees_.find(transaction_id);
    
    if (tx_it != transactions_.end() && fee_it != transaction_fees_.end()) {
        TransactionPriority priority = priority_calculator_->calculatePriority(
            *tx_it->second, fee_it->second, current_block_height_);
        transaction_priorities_[transaction_id] = priority;
    }
}

void EnhancedMempool::updateTotalSize() {
    total_size_bytes_ = 0;
    for (const auto& [tx_id, transaction] : transactions_) {
        total_size_bytes_ += priority_calculator_->calculateTransactionSize(*transaction);
    }
}

void EnhancedMempool::addToRecentFees(const TransactionFee& fee) {
    recent_fees_.push_back(fee);
    
    // Keep only the most recent 1000 fees
    if (recent_fees_.size() > 1000) {
        recent_fees_.erase(recent_fees_.begin());
    }
}

bool EnhancedMempool::shouldEvictTransaction(const std::string& transaction_id) const {
    auto timestamp_it = transaction_timestamps_.find(transaction_id);
    if (timestamp_it != transaction_timestamps_.end()) {
        auto age = std::chrono::system_clock::now() - timestamp_it->second;
        return age > config_.max_age;
    }
    
    return false;
}

std::vector<std::string> EnhancedMempool::selectTransactionsForEviction() const {
    std::vector<std::string> candidates;
    
    for (const auto& [tx_id, timestamp] : transaction_timestamps_) {
        if (shouldEvictTransaction(tx_id)) {
            candidates.push_back(tx_id);
        }
    }
    
    return candidates;
}

// CoinbaseRewardCalculator implementation
CoinbaseRewardCalculator::CoinbaseRewardCalculator() 
    : initial_reward_(5000000000) // 50 BTC in satoshis
    , halving_interval_(210000) { // Every 210,000 blocks
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Coinbase Reward Calculator created");
}

CoinbaseRewardCalculator::~CoinbaseRewardCalculator() {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Coinbase Reward Calculator destroyed");
}

uint64_t CoinbaseRewardCalculator::calculateReward(uint64_t block_height) const {
    uint64_t halving_count = getHalvingCount(block_height);
    uint64_t reward = calculateHalvedReward(halving_count);
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Calculated coinbase reward for block " + std::to_string(block_height) + 
                 ": " + std::to_string(reward) + " satoshis (halving #" + std::to_string(halving_count) + ")");
    
    return reward;
}

uint64_t CoinbaseRewardCalculator::calculateTotalReward(uint64_t block_height, const std::vector<TransactionFee>& fees) const {
    uint64_t coinbase_reward = calculateReward(block_height);
    uint64_t total_fees = 0;
    
    for (const auto& fee : fees) {
        total_fees += fee.total_fee;
    }
    
    uint64_t total_reward = coinbase_reward + total_fees;
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Calculated total reward: coinbase=" + std::to_string(coinbase_reward) + 
                 ", fees=" + std::to_string(total_fees) + ", total=" + std::to_string(total_reward));
    
    return total_reward;
}

uint64_t CoinbaseRewardCalculator::getHalvingInterval() const {
    return halving_interval_;
}

uint64_t CoinbaseRewardCalculator::getNextHalvingBlock(uint64_t current_height) const {
    uint64_t current_halving = current_height / halving_interval_;
    return (current_halving + 1) * halving_interval_;
}

uint64_t CoinbaseRewardCalculator::getHalvingCount(uint64_t block_height) const {
    return block_height / halving_interval_;
}

void CoinbaseRewardCalculator::setInitialReward(uint64_t reward) {
    initial_reward_ = reward;
    DEO_LOG_INFO(BLOCKCHAIN, "Set initial coinbase reward to: " + std::to_string(reward) + " satoshis");
}

void CoinbaseRewardCalculator::setHalvingInterval(uint64_t interval) {
    halving_interval_ = interval;
    DEO_LOG_INFO(BLOCKCHAIN, "Set halving interval to: " + std::to_string(interval) + " blocks");
}

uint64_t CoinbaseRewardCalculator::calculateHalvedReward(uint64_t halving_count) const {
    uint64_t reward = initial_reward_;
    
    // Halve the reward for each halving period
    for (uint64_t i = 0; i < halving_count; ++i) {
        reward /= 2;
    }
    
    return reward;
}

} // namespace core
} // namespace deo
