/**
 * @file scale_testing.h
 * @brief Scale testing framework for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cstdint>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <random>

#include "core/block.h"
#include "core/transaction.h"
#include "network/peer_manager.h"
#include "network/p2p_network_manager.h"
#include "storage/leveldb_storage.h"
#include "sync/fast_sync.h"
#include "crypto/key_pair.h"

namespace deo {
namespace testing {

/**
 * @brief Test scenario enumeration
 */
enum class TestScenario {
    TRANSACTION_STORM,      ///< High volume transaction processing
    BLOCK_PRODUCTION,       ///< Rapid block production and propagation
    NETWORK_PARTITION,      ///< Network partition and recovery
    PEER_CHURN,            ///< Rapid peer connection/disconnection
    MEMPOOL_STRESS,        ///< Mempool stress testing
    SYNC_STRESS,           ///< Sync stress testing
    CONTRACT_DEPLOYMENT,   ///< Mass contract deployment
    MIXED_LOAD             ///< Mixed workload simulation
};

/**
 * @brief Test configuration
 */
struct ScaleTestConfig {
    TestScenario scenario = TestScenario::TRANSACTION_STORM;
    
    // Node configuration
    uint32_t num_nodes = 5;                ///< Number of nodes to create
    uint32_t base_port = 8000;             ///< Base port for node communication
    std::string data_directory = "/tmp/deo_scale_test"; ///< Data directory for nodes
    
    // Test duration
    std::chrono::seconds test_duration{60}; ///< Test duration in seconds
    std::chrono::milliseconds warmup_duration{5000}; ///< Warmup duration
    
    // Transaction storm parameters
    uint32_t transactions_per_second = 100; ///< Transaction generation rate
    uint32_t max_transactions = 10000;      ///< Maximum transactions to generate
    uint32_t transaction_size_bytes = 250;  ///< Average transaction size
    
    // Block production parameters
    uint32_t block_time_seconds = 10;       ///< Target block time
    uint32_t max_blocks = 100;              ///< Maximum blocks to produce
    
    // Network parameters
    uint32_t max_peers_per_node = 8;        ///< Maximum peers per node
    uint32_t connection_timeout_ms = 5000;  ///< Connection timeout
    uint32_t message_timeout_ms = 10000;    ///< Message timeout
    
    // Stress test parameters
    uint32_t stress_interval_ms = 1000;     ///< Stress test interval
    uint32_t max_concurrent_operations = 50; ///< Maximum concurrent operations
    
    // Monitoring parameters
    bool enable_detailed_monitoring = true; ///< Enable detailed monitoring
    uint32_t monitoring_interval_ms = 1000; ///< Monitoring interval
    bool save_test_results = true;          ///< Save test results to file
    
    // Callbacks
    std::function<void(const std::string&)> log_callback;
    std::function<void(const std::string&)> error_callback;
    std::function<void(uint32_t, const std::string&)> progress_callback;
};

/**
 * @brief Test node information
 */
struct TestNode {
    uint32_t node_id;
    std::string address;
    uint16_t port;
    std::string data_dir;
    
    std::unique_ptr<network::PeerManager> peer_manager;
    std::unique_ptr<network::P2PNetworkManager> p2p_manager;
    std::unique_ptr<storage::LevelDBBlockStorage> block_storage;
    std::unique_ptr<storage::LevelDBStateStorage> state_storage;
    std::unique_ptr<sync::FastSyncManager> sync_manager;
    std::unique_ptr<crypto::KeyPair> key_pair;
    
    std::atomic<bool> is_running{false};
    std::atomic<uint64_t> blocks_produced{0};
    std::atomic<uint64_t> transactions_processed{0};
    std::atomic<uint64_t> peers_connected{0};
    
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_activity;
};

/**
 * @brief Test statistics
 */
struct ScaleTestStatistics {
    // Overall test statistics
    std::chrono::system_clock::time_point test_start_time;
    std::chrono::system_clock::time_point test_end_time;
    std::chrono::seconds total_duration{0};
    
    // Node statistics
    uint32_t total_nodes = 0;
    uint32_t active_nodes = 0;
    uint32_t failed_nodes = 0;
    
    // Transaction statistics
    uint64_t total_transactions_generated = 0;
    uint64_t total_transactions_processed = 0;
    uint64_t total_transactions_confirmed = 0;
    uint64_t total_transactions_failed = 0;
    double average_transaction_processing_time_ms = 0.0;
    double peak_transaction_rate_tps = 0.0;
    
    // Block statistics
    uint64_t total_blocks_produced = 0;
    uint64_t total_blocks_propagated = 0;
    double average_block_production_time_ms = 0.0;
    double peak_block_production_rate_bps = 0.0;
    
    // Network statistics
    uint64_t total_peer_connections = 0;
    uint64_t total_peer_disconnections = 0;
    uint64_t total_messages_sent = 0;
    uint64_t total_messages_received = 0;
    double average_network_latency_ms = 0.0;
    double peak_network_throughput_mbps = 0.0;
    
    // Sync statistics
    uint64_t total_sync_operations = 0;
    uint64_t successful_sync_operations = 0;
    uint64_t failed_sync_operations = 0;
    double average_sync_time_ms = 0.0;
    
    // Resource usage
    double peak_cpu_usage_percent = 0.0;
    double peak_memory_usage_mb = 0.0;
    double peak_disk_usage_mb = 0.0;
    
    // Error statistics
    uint64_t total_errors = 0;
    uint64_t network_errors = 0;
    uint64_t validation_errors = 0;
    uint64_t sync_errors = 0;
    uint64_t storage_errors = 0;
};

/**
 * @brief Scale testing framework
 * 
 * This class provides comprehensive scale testing capabilities for the Deo blockchain,
 * including multi-node simulation, transaction storm testing, and performance monitoring.
 */
class ScaleTestingFramework {
public:
    /**
     * @brief Constructor
     */
    ScaleTestingFramework();
    
    /**
     * @brief Destructor
     */
    ~ScaleTestingFramework();
    
    // Disable copy and move semantics
    ScaleTestingFramework(const ScaleTestingFramework&) = delete;
    ScaleTestingFramework& operator=(const ScaleTestingFramework&) = delete;
    ScaleTestingFramework(ScaleTestingFramework&&) = delete;
    ScaleTestingFramework& operator=(ScaleTestingFramework&&) = delete;

    /**
     * @brief Initialize the testing framework
     * @param config Test configuration
     * @return True if initialization was successful
     */
    bool initialize(const ScaleTestConfig& config);
    
    /**
     * @brief Shutdown the testing framework
     */
    void shutdown();

    /**
     * @brief Run the scale test
     * @return True if test completed successfully
     */
    bool runTest();
    
    /**
     * @brief Stop the running test
     */
    void stopTest();
    
    /**
     * @brief Check if test is running
     * @return True if test is currently running
     */
    bool isTestRunning() const;

    /**
     * @brief Get test statistics
     * @return Current test statistics
     */
    ScaleTestStatistics getTestStatistics() const;
    
    /**
     * @brief Get detailed test results
     * @return JSON string with detailed test results
     */
    std::string getDetailedTestResults() const;
    
    /**
     * @brief Save test results to file
     * @param filename Output filename
     * @return True if results were saved successfully
     */
    bool saveTestResults(const std::string& filename) const;

    /**
     * @brief Update test configuration
     * @param config New test configuration
     * @return True if configuration was updated successfully
     */
    bool updateConfig(const ScaleTestConfig& config);
    
    /**
     * @brief Get current test configuration
     * @return Current test configuration
     */
    const ScaleTestConfig& getConfig() const;

private:
    ScaleTestConfig config_;
    mutable std::mutex config_mutex_;
    
    // Test state
    std::atomic<bool> test_running_{false};
    std::atomic<bool> test_active_{false};
    std::chrono::system_clock::time_point test_start_time_;
    
    // Test nodes
    std::vector<std::unique_ptr<TestNode>> test_nodes_;
    mutable std::mutex nodes_mutex_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    ScaleTestStatistics stats_;
    
    // Worker threads
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> workers_active_{false};
    std::condition_variable worker_cv_;
    std::mutex worker_mutex_;
    
    // Test scenario handlers
    std::thread transaction_storm_thread_;
    std::thread block_production_thread_;
    std::thread network_monitoring_thread_;
    std::thread statistics_collection_thread_;
    
    // Random number generation
    std::mt19937 rng_;
    std::uniform_int_distribution<uint32_t> node_distribution_;
    std::uniform_int_distribution<uint64_t> amount_distribution_;
    
    /**
     * @brief Create test nodes
     * @return True if nodes were created successfully
     */
    bool createTestNodes();
    
    /**
     * @brief Initialize test nodes
     * @return True if nodes were initialized successfully
     */
    bool initializeTestNodes();
    
    /**
     * @brief Start test nodes
     * @return True if nodes were started successfully
     */
    bool startTestNodes();
    
    /**
     * @brief Stop test nodes
     */
    void stopTestNodes();
    
    /**
     * @brief Clean up test nodes
     */
    void cleanupTestNodes();
    
    /**
     * @brief Run transaction storm test
     */
    void runTransactionStormTest();
    
    /**
     * @brief Run block production test
     */
    void runBlockProductionTest();
    
    /**
     * @brief Run network monitoring
     */
    void runNetworkMonitoring();
    
    /**
     * @brief Collect statistics
     */
    void collectStatistics();
    
    /**
     * @brief Generate random transaction
     * @param from_node Source node
     * @param to_node Destination node
     * @return Generated transaction
     */
    std::shared_ptr<core::Transaction> generateRandomTransaction(uint32_t from_node, uint32_t to_node);
    
    /**
     * @brief Generate random block
     * @param node_id Node producing the block
     * @return Generated block
     */
    std::shared_ptr<core::Block> generateRandomBlock(uint32_t node_id);
    
    /**
     * @brief Simulate network partition
     * @param partition_size Number of nodes in each partition
     */
    void simulateNetworkPartition(uint32_t partition_size);
    
    /**
     * @brief Simulate peer churn
     * @param churn_rate Rate of peer connections/disconnections
     */
    void simulatePeerChurn(double churn_rate);
    
    /**
     * @brief Update test statistics
     */
    void updateTestStatistics();
    
    /**
     * @brief Calculate performance metrics
     */
    void calculatePerformanceMetrics();
    
    /**
     * @brief Log test message
     * @param message Log message
     */
    void logMessage(const std::string& message);
    
    /**
     * @brief Log test error
     * @param error Error message
     */
    void logError(const std::string& error);
    
    /**
     * @brief Update test progress
     * @param progress Progress percentage
     * @param message Progress message
     */
    void updateProgress(uint32_t progress, const std::string& message);
    
    /**
     * @brief Create test data directory
     * @param node_id Node ID
     * @return Data directory path
     */
    std::string createTestDataDirectory(uint32_t node_id);
    
    /**
     * @brief Clean up test data
     */
    void cleanupTestData();
    
    /**
     * @brief Validate test results
     * @return True if test results are valid
     */
    bool validateTestResults();
    
    /**
     * @brief Generate test report
     * @return Test report as JSON string
     */
    std::string generateTestReport() const;
};

/**
 * @brief Transaction storm generator
 * 
 * This class generates high-volume transactions for stress testing
 * the blockchain's transaction processing capabilities.
 */
class TransactionStormGenerator {
public:
    /**
     * @brief Constructor
     * @param num_nodes Number of nodes in the network
     * @param transactions_per_second Target transaction rate
     */
    TransactionStormGenerator(uint32_t num_nodes, uint32_t transactions_per_second);
    
    /**
     * @brief Destructor
     */
    ~TransactionStormGenerator();

    /**
     * @brief Start transaction generation
     * @param duration Generation duration
     * @return True if generation was started successfully
     */
    bool startGeneration(std::chrono::seconds duration);
    
    /**
     * @brief Stop transaction generation
     */
    void stopGeneration();
    
    /**
     * @brief Check if generation is active
     * @return True if generation is active
     */
    bool isGenerationActive() const;
    
    /**
     * @brief Get generated transaction count
     * @return Number of transactions generated
     */
    uint64_t getGeneratedTransactionCount() const;
    
    /**
     * @brief Get generation rate
     * @return Current generation rate in TPS
     */
    double getGenerationRate() const;

private:
    uint32_t num_nodes_;
    uint32_t transactions_per_second_;
    std::atomic<bool> generation_active_{false};
    std::atomic<uint64_t> transactions_generated_{0};
    
    std::thread generation_thread_;
    std::mt19937 rng_;
    std::uniform_int_distribution<uint32_t> node_distribution_;
    std::uniform_int_distribution<uint64_t> amount_distribution_;
    
    /**
     * @brief Transaction generation loop
     */
    void generationLoop();
    
    /**
     * @brief Generate single transaction
     * @return Generated transaction
     */
    std::shared_ptr<core::Transaction> generateTransaction();
};

/**
 * @brief Performance monitor
 * 
 * This class monitors system performance during scale testing
 * and provides real-time performance metrics.
 */
class PerformanceMonitor {
public:
    /**
     * @brief Constructor
     */
    PerformanceMonitor();
    
    /**
     * @brief Destructor
     */
    ~PerformanceMonitor();

    /**
     * @brief Start monitoring
     * @param interval Monitoring interval
     * @return True if monitoring was started successfully
     */
    bool startMonitoring(std::chrono::milliseconds interval);
    
    /**
     * @brief Stop monitoring
     */
    void stopMonitoring();
    
    /**
     * @brief Check if monitoring is active
     * @return True if monitoring is active
     */
    bool isMonitoringActive() const;
    
    /**
     * @brief Get current CPU usage
     * @return CPU usage percentage
     */
    double getCurrentCpuUsage() const;
    
    /**
     * @brief Get current memory usage
     * @return Memory usage in MB
     */
    double getCurrentMemoryUsage() const;
    
    /**
     * @brief Get current disk usage
     * @return Disk usage in MB
     */
    double getCurrentDiskUsage() const;
    
    /**
     * @brief Get peak CPU usage
     * @return Peak CPU usage percentage
     */
    double getPeakCpuUsage() const;
    
    /**
     * @brief Get peak memory usage
     * @return Peak memory usage in MB
     */
    double getPeakMemoryUsage() const;
    
    /**
     * @brief Get peak disk usage
     * @return Peak disk usage in MB
     */
    double getPeakDiskUsage() const;
    
    /**
     * @brief Get performance statistics
     * @return JSON string with performance statistics
     */
    std::string getPerformanceStatistics() const;

private:
    std::atomic<bool> monitoring_active_{false};
    std::thread monitoring_thread_;
    
    mutable std::mutex stats_mutex_;
    double current_cpu_usage_ = 0.0;
    double current_memory_usage_ = 0.0;
    double current_disk_usage_ = 0.0;
    double peak_cpu_usage_ = 0.0;
    double peak_memory_usage_ = 0.0;
    double peak_disk_usage_ = 0.0;
    
    /**
     * @brief Monitoring loop
     */
    void monitoringLoop();
    
    /**
     * @brief Update performance metrics
     */
    void updatePerformanceMetrics();
    
    /**
     * @brief Get system CPU usage
     * @return CPU usage percentage
     */
    double getSystemCpuUsage() const;
    
    /**
     * @brief Get system memory usage
     * @return Memory usage in MB
     */
    double getSystemMemoryUsage() const;
    
    /**
     * @brief Get system disk usage
     * @return Disk usage in MB
     */
    double getSystemDiskUsage() const;
};

} // namespace testing
} // namespace deo

