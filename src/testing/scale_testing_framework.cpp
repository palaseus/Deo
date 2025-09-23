/**
 * @file scale_testing_framework.cpp
 * @brief Scale testing framework implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "testing/scale_testing.h"
#include "utils/logger.h"
#include "crypto/hash.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <thread>
#include <chrono>
#include <sys/resource.h>
#include <sys/statvfs.h>

namespace deo {
namespace testing {

ScaleTestingFramework::ScaleTestingFramework() 
    : rng_(std::chrono::system_clock::now().time_since_epoch().count()),
      node_distribution_(0, 0),
      amount_distribution_(1000, 1000000) {
}

ScaleTestingFramework::~ScaleTestingFramework() {
    shutdown();
}

bool ScaleTestingFramework::initialize(const ScaleTestConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_ = config;
    
    // Validate configuration
    if (config_.num_nodes == 0 || config_.num_nodes > 20) {
        return false;
    }
    
    if (config_.base_port < 1024 || config_.base_port > 65535) {
        return false;
    }
    
    // Initialize random distributions
    node_distribution_ = std::uniform_int_distribution<uint32_t>(0, config_.num_nodes - 1);
    
    // Create test data directory
    try {
        std::filesystem::create_directories(config_.data_directory);
    } catch (const std::exception& e) {
        logError("Failed to create test data directory: " + std::string(e.what()));
        return false;
    }
    
    logMessage("Scale testing framework initialized with " + std::to_string(config_.num_nodes) + " nodes");
    return true;
}

void ScaleTestingFramework::shutdown() {
    stopTest();
    
    // Wait for all worker threads to finish
    workers_active_.store(false);
    worker_cv_.notify_all();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
    
    // Clean up test nodes
    cleanupTestNodes();
    
    // Clean up test data
    cleanupTestData();
    
    logMessage("Scale testing framework shutdown completed");
}

bool ScaleTestingFramework::runTest() {
    if (test_running_.load()) {
        return false; // Already running
    }
    
    test_running_.store(true);
    test_active_.store(true);
    test_start_time_ = std::chrono::system_clock::now();
    
    // Initialize statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = ScaleTestStatistics{};
        stats_.test_start_time = test_start_time_;
        stats_.total_nodes = config_.num_nodes;
    }
    
    logMessage("Starting scale test: " + std::to_string(config_.num_nodes) + " nodes, " + 
              std::to_string(config_.test_duration.count()) + " seconds");
    
    try {
        // Phase 1: Create and initialize test nodes
        updateProgress(10, "Creating test nodes...");
        if (!createTestNodes()) {
            logError("Failed to create test nodes");
            return false;
        }
        
        // Phase 2: Initialize test nodes
        updateProgress(20, "Initializing test nodes...");
        if (!initializeTestNodes()) {
            logError("Failed to initialize test nodes");
            return false;
        }
        
        // Phase 3: Start test nodes
        updateProgress(30, "Starting test nodes...");
        if (!startTestNodes()) {
            logError("Failed to start test nodes");
            return false;
        }
        
        // Phase 4: Warmup period
        updateProgress(40, "Warmup period...");
        std::this_thread::sleep_for(config_.warmup_duration);
        
        // Phase 5: Run test scenario
        updateProgress(50, "Running test scenario...");
        workers_active_.store(true);
        
        // Start worker threads based on test scenario
        switch (config_.scenario) {
            case TestScenario::TRANSACTION_STORM:
                transaction_storm_thread_ = std::thread(&ScaleTestingFramework::runTransactionStormTest, this);
                break;
            case TestScenario::BLOCK_PRODUCTION:
                block_production_thread_ = std::thread(&ScaleTestingFramework::runBlockProductionTest, this);
                break;
            case TestScenario::MIXED_LOAD:
                transaction_storm_thread_ = std::thread(&ScaleTestingFramework::runTransactionStormTest, this);
                block_production_thread_ = std::thread(&ScaleTestingFramework::runBlockProductionTest, this);
                break;
            default:
                logError("Unsupported test scenario");
                return false;
        }
        
        // Start monitoring threads
        network_monitoring_thread_ = std::thread(&ScaleTestingFramework::runNetworkMonitoring, this);
        statistics_collection_thread_ = std::thread(&ScaleTestingFramework::collectStatistics, this);
        
        // Phase 6: Run test for specified duration
        updateProgress(60, "Running test...");
        auto test_end_time = test_start_time_ + config_.test_duration;
        
        while (test_active_.load() && std::chrono::system_clock::now() < test_end_time) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Update progress
            auto elapsed = std::chrono::system_clock::now() - test_start_time_;
            auto total = config_.test_duration;
            uint32_t progress = 60 + (40 * elapsed.count() / total.count());
            updateProgress(progress, "Test in progress...");
        }
        
        // Phase 7: Stop test
        updateProgress(90, "Stopping test...");
        test_active_.store(false);
        
        // Wait for worker threads to finish
        if (transaction_storm_thread_.joinable()) {
            transaction_storm_thread_.join();
        }
        if (block_production_thread_.joinable()) {
            block_production_thread_.join();
        }
        if (network_monitoring_thread_.joinable()) {
            network_monitoring_thread_.join();
        }
        if (statistics_collection_thread_.joinable()) {
            statistics_collection_thread_.join();
        }
        
        // Phase 8: Stop test nodes
        updateProgress(95, "Stopping test nodes...");
        stopTestNodes();
        
        // Phase 9: Finalize test
        updateProgress(100, "Test completed");
        test_running_.store(false);
        
        // Update final statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.test_end_time = std::chrono::system_clock::now();
            stats_.total_duration = std::chrono::duration_cast<std::chrono::seconds>(
                stats_.test_end_time - stats_.test_start_time);
        }
        
        // Validate test results
        if (!validateTestResults()) {
            logError("Test validation failed");
            return false;
        }
        
        logMessage("Scale test completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError("Test execution failed: " + std::string(e.what()));
        test_running_.store(false);
        test_active_.store(false);
        return false;
    }
}

void ScaleTestingFramework::stopTest() {
    if (!test_running_.load()) {
        return;
    }
    
    test_active_.store(false);
    test_running_.store(false);
    
    // Stop worker threads
    if (transaction_storm_thread_.joinable()) {
        transaction_storm_thread_.join();
    }
    if (block_production_thread_.joinable()) {
        block_production_thread_.join();
    }
    if (network_monitoring_thread_.joinable()) {
        network_monitoring_thread_.join();
    }
    if (statistics_collection_thread_.joinable()) {
        statistics_collection_thread_.join();
    }
    
    // Stop test nodes
    stopTestNodes();
    
    logMessage("Test stopped");
}

bool ScaleTestingFramework::isTestRunning() const {
    return test_running_.load();
}

ScaleTestStatistics ScaleTestingFramework::getTestStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

std::string ScaleTestingFramework::getDetailedTestResults() const {
    return generateTestReport();
}

bool ScaleTestingFramework::saveTestResults(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        std::string report = generateTestReport();
        file << report;
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        const_cast<ScaleTestingFramework*>(this)->logError("Failed to save test results: " + std::string(e.what()));
        return false;
    }
}

bool ScaleTestingFramework::updateConfig(const ScaleTestConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Validate configuration
    if (config.num_nodes == 0 || config.num_nodes > 20) {
        return false;
    }
    
    if (config.base_port < 1024 || config.base_port > 65535) {
        return false;
    }
    
    config_ = config;
    
    // Update random distributions
    node_distribution_ = std::uniform_int_distribution<uint32_t>(0, config_.num_nodes - 1);
    
    return true;
}

const ScaleTestConfig& ScaleTestingFramework::getConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

bool ScaleTestingFramework::createTestNodes() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    test_nodes_.clear();
    test_nodes_.reserve(config_.num_nodes);
    
    for (uint32_t i = 0; i < config_.num_nodes; ++i) {
        auto node = std::make_unique<TestNode>();
        node->node_id = i;
        node->address = "127.0.0.1";
        node->port = config_.base_port + i;
        node->data_dir = createTestDataDirectory(i);
        
        // Create node components
        node->peer_manager = std::make_unique<network::PeerManager>();
        node->p2p_manager = std::make_unique<network::P2PNetworkManager>();
        node->block_storage = std::make_unique<storage::LevelDBBlockStorage>(node->data_dir);
        node->state_storage = std::make_unique<storage::LevelDBStateStorage>(node->data_dir);
        node->key_pair = std::make_unique<crypto::KeyPair>();
        
        // Create sync manager
        node->sync_manager = std::make_unique<sync::FastSyncManager>(
            std::shared_ptr<network::PeerManager>(node->peer_manager.get(), [](network::PeerManager*){}),
            std::shared_ptr<storage::LevelDBBlockStorage>(node->block_storage.get(), [](storage::LevelDBBlockStorage*){}),
            std::shared_ptr<storage::LevelDBStateStorage>(node->state_storage.get(), [](storage::LevelDBStateStorage*){})
        );
        
        test_nodes_.push_back(std::move(node));
    }
    
    logMessage("Created " + std::to_string(test_nodes_.size()) + " test nodes");
    return true;
}

bool ScaleTestingFramework::initializeTestNodes() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    for (auto& node : test_nodes_) {
        // Initialize peer manager
        if (!node->peer_manager->initialize()) {
            logError("Failed to initialize peer manager for node " + std::to_string(node->node_id));
            return false;
        }
        
        // Initialize storage
        if (!node->block_storage->initialize()) {
            logError("Failed to initialize block storage for node " + std::to_string(node->node_id));
            return false;
        }
        
        if (!node->state_storage->initialize()) {
            logError("Failed to initialize state storage for node " + std::to_string(node->node_id));
            return false;
        }
        
        // Initialize sync manager
        sync::SyncConfig sync_config;
        sync_config.mode = sync::SyncMode::FAST_SYNC;
        sync_config.max_peers = config_.max_peers_per_node;
        sync_config.min_peers = 2;
        sync_config.connection_timeout_ms = config_.connection_timeout_ms;
        sync_config.batch_size = 10;
        sync_config.max_concurrent_downloads = 2;
        
        if (!node->sync_manager->initialize(sync_config)) {
            logError("Failed to initialize sync manager for node " + std::to_string(node->node_id));
            return false;
        }
    }
    
    logMessage("Initialized " + std::to_string(test_nodes_.size()) + " test nodes");
    return true;
}

bool ScaleTestingFramework::startTestNodes() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    for (auto& node : test_nodes_) {
        // Start peer manager
        // Note: In a real implementation, this would start the P2P network
        
        // Mark node as running
        node->is_running.store(true);
        node->start_time = std::chrono::system_clock::now();
        node->last_activity = node->start_time;
        
        // Update statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.active_nodes++;
        }
    }
    
    logMessage("Started " + std::to_string(test_nodes_.size()) + " test nodes");
    return true;
}

void ScaleTestingFramework::stopTestNodes() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    for (auto& node : test_nodes_) {
        if (node->is_running.load()) {
            // Stop sync manager
            node->sync_manager->shutdown();
            
            // Stop storage
            node->block_storage->shutdown();
            node->state_storage->shutdown();
            
            // Stop peer manager
            node->peer_manager->shutdown();
            
            // Mark node as stopped
            node->is_running.store(false);
            
            // Update statistics
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.active_nodes--;
            }
        }
    }
    
    logMessage("Stopped all test nodes");
}

void ScaleTestingFramework::cleanupTestNodes() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    for (auto& node : test_nodes_) {
        if (node->is_running.load()) {
            node->sync_manager->shutdown();
            node->block_storage->shutdown();
            node->state_storage->shutdown();
            node->peer_manager->shutdown();
        }
    }
    
    test_nodes_.clear();
    logMessage("Cleaned up test nodes");
}

void ScaleTestingFramework::runTransactionStormTest() {
    logMessage("Starting transaction storm test");
    
    uint64_t transactions_generated = 0;
    auto start_time = std::chrono::system_clock::now();
    auto end_time = start_time + config_.test_duration;
    
    while (test_active_.load() && std::chrono::system_clock::now() < end_time) {
        if (transactions_generated >= config_.max_transactions) {
            break;
        }
        
        // Generate random transaction
        uint32_t from_node = node_distribution_(rng_);
        uint32_t to_node = node_distribution_(rng_);
        
        if (from_node != to_node) {
            auto transaction = generateRandomTransaction(from_node, to_node);
            if (transaction) {
                transactions_generated++;
                
                // Update statistics
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.total_transactions_generated++;
                }
            }
        }
        
        // Control transaction rate
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / config_.transactions_per_second));
    }
    
    logMessage("Transaction storm test completed: " + std::to_string(transactions_generated) + " transactions generated");
}

void ScaleTestingFramework::runBlockProductionTest() {
    logMessage("Starting block production test");
    
    uint64_t blocks_produced = 0;
    auto start_time = std::chrono::system_clock::now();
    auto end_time = start_time + config_.test_duration;
    
    while (test_active_.load() && std::chrono::system_clock::now() < end_time) {
        if (blocks_produced >= config_.max_blocks) {
            break;
        }
        
        // Select random node to produce block
        uint32_t node_id = node_distribution_(rng_);
        
        auto block = generateRandomBlock(node_id);
        if (block) {
            blocks_produced++;
            
            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_blocks_produced++;
            }
            
            // Update node statistics
            std::lock_guard<std::mutex> lock(nodes_mutex_);
            if (node_id < test_nodes_.size()) {
                test_nodes_[node_id]->blocks_produced++;
            }
        }
        
        // Control block production rate
        std::this_thread::sleep_for(std::chrono::seconds(config_.block_time_seconds));
    }
    
    logMessage("Block production test completed: " + std::to_string(blocks_produced) + " blocks produced");
}

void ScaleTestingFramework::runNetworkMonitoring() {
    logMessage("Starting network monitoring");
    
    while (test_active_.load()) {
        // Monitor network statistics
        std::lock_guard<std::mutex> lock(nodes_mutex_);
        
        uint32_t total_peers = 0;
        for (const auto& node : test_nodes_) {
            if (node->is_running.load()) {
                total_peers += node->peers_connected.load();
            }
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.total_peer_connections = total_peers;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.monitoring_interval_ms));
    }
    
    logMessage("Network monitoring completed");
}

void ScaleTestingFramework::collectStatistics() {
    logMessage("Starting statistics collection");
    
    while (test_active_.load()) {
        updateTestStatistics();
        calculatePerformanceMetrics();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.monitoring_interval_ms));
    }
    
    logMessage("Statistics collection completed");
}

std::shared_ptr<core::Transaction> ScaleTestingFramework::generateRandomTransaction(uint32_t from_node, uint32_t to_node) {
    if (from_node >= test_nodes_.size() || to_node >= test_nodes_.size()) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto& from_node_ptr = test_nodes_[from_node];
    auto& to_node_ptr = test_nodes_[to_node];
    
    // Create transaction inputs
    std::vector<core::TransactionInput> inputs;
    core::TransactionInput input(
        "0000000000000000000000000000000000000000000000000000000000000000",
        0,
        "test_signature",
        from_node_ptr->key_pair->getPublicKey(),
        0xFFFFFFFF
    );
    inputs.push_back(input);
    
    // Create transaction outputs
    std::vector<core::TransactionOutput> outputs;
    uint64_t amount = amount_distribution_(rng_);
    core::TransactionOutput output(amount, to_node_ptr->key_pair->getPublicKey());
    outputs.push_back(output);
    
    // Create transaction
    auto transaction = std::make_shared<core::Transaction>(inputs, outputs, core::Transaction::Type::REGULAR);
    transaction->setVersion(1);
    
    return transaction;
}

std::shared_ptr<core::Block> ScaleTestingFramework::generateRandomBlock(uint32_t node_id) {
    if (node_id >= test_nodes_.size()) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto& node = test_nodes_[node_id];
    
    // Create block header
    core::BlockHeader header;
    header.version = 1;
    header.previous_hash = "0000000000000000000000000000000000000000000000000000000000000000";
    header.merkle_root = "test_merkle_root";
    header.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header.nonce = 12345;
    header.difficulty = 1000;
    header.height = node->blocks_produced.load();
    
    // Create transactions
    std::vector<std::shared_ptr<core::Transaction>> transactions;
    
    // Create coinbase transaction
    std::vector<core::TransactionInput> coinbase_inputs;
    core::TransactionInput coinbase_input(
        "0000000000000000000000000000000000000000000000000000000000000000",
        0xFFFFFFFF,
        "",
        "",
        0xFFFFFFFF
    );
    coinbase_inputs.push_back(coinbase_input);
    
    std::vector<core::TransactionOutput> coinbase_outputs;
    core::TransactionOutput coinbase_output(5000000000, node->key_pair->getPublicKey());
    coinbase_outputs.push_back(coinbase_output);
    
    auto coinbase_tx = std::make_shared<core::Transaction>(coinbase_inputs, coinbase_outputs, core::Transaction::Type::COINBASE);
    coinbase_tx->setVersion(1);
    transactions.push_back(coinbase_tx);
    
    return std::make_shared<core::Block>(header, transactions);
}

void ScaleTestingFramework::updateTestStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // Update node statistics
    stats_.active_nodes = 0;
    stats_.failed_nodes = 0;
    
    std::lock_guard<std::mutex> nodes_lock(nodes_mutex_);
    for (const auto& node : test_nodes_) {
        if (node->is_running.load()) {
            stats_.active_nodes++;
        } else {
            stats_.failed_nodes++;
        }
    }
    
    // Update transaction statistics
    stats_.total_transactions_processed = 0;
    for (const auto& node : test_nodes_) {
        stats_.total_transactions_processed += node->transactions_processed.load();
    }
    
    // Update block statistics
    stats_.total_blocks_produced = 0;
    for (const auto& node : test_nodes_) {
        stats_.total_blocks_produced += node->blocks_produced.load();
    }
}

void ScaleTestingFramework::calculatePerformanceMetrics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (stats_.total_duration.count() > 0) {
        // Calculate transaction rate
        double transaction_rate = static_cast<double>(stats_.total_transactions_processed) / 
                                 stats_.total_duration.count();
        stats_.peak_transaction_rate_tps = std::max(stats_.peak_transaction_rate_tps, transaction_rate);
        
        // Calculate block rate
        double block_rate = static_cast<double>(stats_.total_blocks_produced) / 
                           stats_.total_duration.count();
        stats_.peak_block_production_rate_bps = std::max(stats_.peak_block_production_rate_bps, block_rate);
    }
}

void ScaleTestingFramework::logMessage(const std::string& message) {
    if (config_.log_callback) {
        config_.log_callback(message);
    }
}

void ScaleTestingFramework::logError(const std::string& error) {
    if (config_.error_callback) {
        config_.error_callback(error);
    }
}

void ScaleTestingFramework::updateProgress(uint32_t progress, const std::string& message) {
    if (config_.progress_callback) {
        config_.progress_callback(progress, message);
    }
}

std::string ScaleTestingFramework::createTestDataDirectory(uint32_t node_id) {
    return config_.data_directory + "/node_" + std::to_string(node_id);
}

void ScaleTestingFramework::cleanupTestData() {
    try {
        std::filesystem::remove_all(config_.data_directory);
    } catch (const std::exception& e) {
        logError("Failed to cleanup test data: " + std::string(e.what()));
    }
}

bool ScaleTestingFramework::validateTestResults() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // Basic validation checks
    if (stats_.total_nodes == 0) {
        return false;
    }
    
    if (stats_.active_nodes == 0) {
        return false;
    }
    
    if (stats_.total_transactions_generated == 0 && config_.scenario == TestScenario::TRANSACTION_STORM) {
        return false;
    }
    
    if (stats_.total_blocks_produced == 0 && config_.scenario == TestScenario::BLOCK_PRODUCTION) {
        return false;
    }
    
    return true;
}

std::string ScaleTestingFramework::generateTestReport() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    nlohmann::json report;
    
    // Test configuration
    report["test_config"] = {
        {"scenario", static_cast<int>(config_.scenario)},
        {"num_nodes", config_.num_nodes},
        {"test_duration_seconds", config_.test_duration.count()},
        {"transactions_per_second", config_.transactions_per_second},
        {"max_transactions", config_.max_transactions},
        {"block_time_seconds", config_.block_time_seconds},
        {"max_blocks", config_.max_blocks}
    };
    
    // Test results
    report["test_results"] = {
        {"total_duration_seconds", stats_.total_duration.count()},
        {"total_nodes", stats_.total_nodes},
        {"active_nodes", stats_.active_nodes},
        {"failed_nodes", stats_.failed_nodes},
        {"total_transactions_generated", stats_.total_transactions_generated},
        {"total_transactions_processed", stats_.total_transactions_processed},
        {"total_transactions_confirmed", stats_.total_transactions_confirmed},
        {"total_transactions_failed", stats_.total_transactions_failed},
        {"peak_transaction_rate_tps", stats_.peak_transaction_rate_tps},
        {"total_blocks_produced", stats_.total_blocks_produced},
        {"total_blocks_propagated", stats_.total_blocks_propagated},
        {"peak_block_production_rate_bps", stats_.peak_block_production_rate_bps},
        {"total_peer_connections", stats_.total_peer_connections},
        {"total_peer_disconnections", stats_.total_peer_disconnections},
        {"total_messages_sent", stats_.total_messages_sent},
        {"total_messages_received", stats_.total_messages_received},
        {"peak_network_throughput_mbps", stats_.peak_network_throughput_mbps},
        {"total_sync_operations", stats_.total_sync_operations},
        {"successful_sync_operations", stats_.successful_sync_operations},
        {"failed_sync_operations", stats_.failed_sync_operations},
        {"peak_cpu_usage_percent", stats_.peak_cpu_usage_percent},
        {"peak_memory_usage_mb", stats_.peak_memory_usage_mb},
        {"peak_disk_usage_mb", stats_.peak_disk_usage_mb},
        {"total_errors", stats_.total_errors},
        {"network_errors", stats_.network_errors},
        {"validation_errors", stats_.validation_errors},
        {"sync_errors", stats_.sync_errors},
        {"storage_errors", stats_.storage_errors}
    };
    
    // Performance metrics
    report["performance_metrics"] = {
        {"average_transaction_processing_time_ms", stats_.average_transaction_processing_time_ms},
        {"average_block_production_time_ms", stats_.average_block_production_time_ms},
        {"average_network_latency_ms", stats_.average_network_latency_ms},
        {"average_sync_time_ms", stats_.average_sync_time_ms}
    };
    
    return report.dump(2);
}

// TransactionStormGenerator implementation
TransactionStormGenerator::TransactionStormGenerator(uint32_t num_nodes, uint32_t transactions_per_second)
    : num_nodes_(num_nodes), transactions_per_second_(transactions_per_second),
      rng_(std::chrono::system_clock::now().time_since_epoch().count()),
      node_distribution_(0, num_nodes - 1),
      amount_distribution_(1000, 1000000) {
}

TransactionStormGenerator::~TransactionStormGenerator() {
    stopGeneration();
}

bool TransactionStormGenerator::startGeneration(std::chrono::seconds /* duration */) {
    if (generation_active_.load()) {
        return false; // Already generating
    }
    
    generation_active_.store(true);
    transactions_generated_.store(0);
    
    generation_thread_ = std::thread(&TransactionStormGenerator::generationLoop, this);
    
    return true;
}

void TransactionStormGenerator::stopGeneration() {
    if (generation_active_.load()) {
        generation_active_.store(false);
        
        if (generation_thread_.joinable()) {
            generation_thread_.join();
        }
    }
}

bool TransactionStormGenerator::isGenerationActive() const {
    return generation_active_.load();
}

uint64_t TransactionStormGenerator::getGeneratedTransactionCount() const {
    return transactions_generated_.load();
}

double TransactionStormGenerator::getGenerationRate() const {
    // This would be calculated based on time elapsed and transactions generated
    // For now, return a placeholder
    return static_cast<double>(transactions_per_second_);
}

void TransactionStormGenerator::generationLoop() {
    while (generation_active_.load()) {
        // Generate transaction
        auto transaction = generateTransaction();
        if (transaction) {
            transactions_generated_++;
        }
        
        // Control generation rate
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / transactions_per_second_));
    }
}

std::shared_ptr<core::Transaction> TransactionStormGenerator::generateTransaction() {
    // Generate random transaction
    uint32_t from_node = node_distribution_(rng_);
    uint32_t to_node = node_distribution_(rng_);
    
    if (from_node == to_node) {
        return nullptr;
    }
    
    // Create transaction inputs
    std::vector<core::TransactionInput> inputs;
    core::TransactionInput input(
        "0000000000000000000000000000000000000000000000000000000000000000",
        0,
        "test_signature",
        "test_public_key_" + std::to_string(from_node),
        0xFFFFFFFF
    );
    inputs.push_back(input);
    
    // Create transaction outputs
    std::vector<core::TransactionOutput> outputs;
    uint64_t amount = amount_distribution_(rng_);
    core::TransactionOutput output(amount, "test_public_key_" + std::to_string(to_node));
    outputs.push_back(output);
    
    // Create transaction
    auto transaction = std::make_shared<core::Transaction>(inputs, outputs, core::Transaction::Type::REGULAR);
    transaction->setVersion(1);
    
    return transaction;
}

// PerformanceMonitor implementation
PerformanceMonitor::PerformanceMonitor() {
}

PerformanceMonitor::~PerformanceMonitor() {
    stopMonitoring();
}

bool PerformanceMonitor::startMonitoring(std::chrono::milliseconds /* interval */) {
    if (monitoring_active_.load()) {
        return false; // Already monitoring
    }
    
    monitoring_active_.store(true);
    monitoring_thread_ = std::thread(&PerformanceMonitor::monitoringLoop, this);
    
    return true;
}

void PerformanceMonitor::stopMonitoring() {
    if (monitoring_active_.load()) {
        monitoring_active_.store(false);
        
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }
}

bool PerformanceMonitor::isMonitoringActive() const {
    return monitoring_active_.load();
}

double PerformanceMonitor::getCurrentCpuUsage() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return current_cpu_usage_;
}

double PerformanceMonitor::getCurrentMemoryUsage() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return current_memory_usage_;
}

double PerformanceMonitor::getCurrentDiskUsage() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return current_disk_usage_;
}

double PerformanceMonitor::getPeakCpuUsage() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return peak_cpu_usage_;
}

double PerformanceMonitor::getPeakMemoryUsage() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return peak_memory_usage_;
}

double PerformanceMonitor::getPeakDiskUsage() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return peak_disk_usage_;
}

std::string PerformanceMonitor::getPerformanceStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    nlohmann::json stats;
    stats["current_cpu_usage_percent"] = current_cpu_usage_;
    stats["current_memory_usage_mb"] = current_memory_usage_;
    stats["current_disk_usage_mb"] = current_disk_usage_;
    stats["peak_cpu_usage_percent"] = peak_cpu_usage_;
    stats["peak_memory_usage_mb"] = peak_memory_usage_;
    stats["peak_disk_usage_mb"] = peak_disk_usage_;
    
    return stats.dump(2);
}

void PerformanceMonitor::monitoringLoop() {
    while (monitoring_active_.load()) {
        updatePerformanceMetrics();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void PerformanceMonitor::updatePerformanceMetrics() {
    double cpu_usage = getSystemCpuUsage();
    double memory_usage = getSystemMemoryUsage();
    double disk_usage = getSystemDiskUsage();
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    current_cpu_usage_ = cpu_usage;
    current_memory_usage_ = memory_usage;
    current_disk_usage_ = disk_usage;
    
    peak_cpu_usage_ = std::max(peak_cpu_usage_, cpu_usage);
    peak_memory_usage_ = std::max(peak_memory_usage_, memory_usage);
    peak_disk_usage_ = std::max(peak_disk_usage_, disk_usage);
}

double PerformanceMonitor::getSystemCpuUsage() const {
    // Simplified CPU usage calculation
    // In a real implementation, this would read from /proc/stat
    return 0.0;
}

double PerformanceMonitor::getSystemMemoryUsage() const {
    // Simplified memory usage calculation
    // In a real implementation, this would read from /proc/meminfo
    return 0.0;
}

double PerformanceMonitor::getSystemDiskUsage() const {
    // Simplified disk usage calculation
    // In a real implementation, this would use statvfs
    return 0.0;
}

} // namespace testing
} // namespace deo
