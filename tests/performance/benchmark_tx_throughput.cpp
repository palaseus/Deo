/**
 * @file benchmark_tx_throughput.cpp
 * @brief Benchmark for transaction throughput
 */

#include <gtest/gtest.h>
#include "node/node_runtime.h"
#include "core/transaction.h"
#include "utils/performance_monitor.h"

#include <chrono>
#include <vector>
#include <thread>
#include <filesystem>

namespace deo {
namespace tests {

class TransactionThroughputBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/deo_bench_tx_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed());
        std::filesystem::create_directories(test_dir_);
        
        // Configure node runtime for benchmarking
        config_.data_directory = test_dir_ + "/data";
        config_.state_directory = test_dir_ + "/state";
        config_.enable_p2p = false; // Disable P2P for simpler benchmarking
        config_.enable_mining = false;
        config_.enable_json_rpc = false;
        config_.storage_backend = storage_backend_; // Use configured backend
        
        node_runtime_ = std::make_unique<node::NodeRuntime>(config_);
        ASSERT_TRUE(node_runtime_->initialize());
        ASSERT_TRUE(node_runtime_->start());
        
        // Reset performance monitor
        utils::PerformanceMonitor::getInstance().resetMetrics();
    }
    
    void TearDown() override {
        if (node_runtime_) {
            node_runtime_->stop();
            node_runtime_.reset();
        }
        
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }
    
    std::shared_ptr<core::Transaction> createTestTransaction(size_t index) {
        auto tx = std::make_shared<core::Transaction>();
        core::TransactionOutput output;
        output.recipient_address = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
        output.value = 1000 + index;
        tx->addOutput(output);
        return tx;
    }
    
    node::NodeConfig config_;
    std::unique_ptr<node::NodeRuntime> node_runtime_;
    std::string test_dir_;
    std::string storage_backend_ = "json"; // Default, can be overridden in test
    
    // Helper to run test with different backends
    void runWithBackend(const std::string& backend) {
        storage_backend_ = backend;
        TearDown();
        SetUp();
    }
};

TEST_F(TransactionThroughputBenchmark, SingleTransactionProcessing) {
    auto tx = createTestTransaction(0);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool added = node_runtime_->addTransaction(tx);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    EXPECT_TRUE(added);
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();
    
    // Single transaction should be processed very quickly (< 10ms)
    EXPECT_LT(duration, 10000);
    
    std::cout << "Single transaction processing time: " << duration << " microseconds" << std::endl;
}

TEST_F(TransactionThroughputBenchmark, BatchTransactionThroughput) {
    const size_t transaction_count = 100;
    std::vector<std::shared_ptr<core::Transaction>> transactions;
    
    // Create transactions
    for (size_t i = 0; i < transaction_count; ++i) {
        transactions.push_back(createTestTransaction(i));
    }
    
    // Process all transactions
    auto start_time = std::chrono::high_resolution_clock::now();
    for (auto& tx : transactions) {
        node_runtime_->addTransaction(tx);
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Wait a bit for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    double tps = (transaction_count * 1000.0) / duration;
    
    std::cout << "Processed " << transaction_count << " transactions in " 
              << duration << " ms" << std::endl;
    std::cout << "Throughput: " << tps << " TPS" << std::endl;
    
    // Verify all transactions are in mempool
    EXPECT_GE(node_runtime_->getMempoolSize(), transaction_count);
    
    // Get TPS from node statistics
    auto stats = node_runtime_->getStatistics();
    std::cout << "Node-reported TPS: " << stats.transactions_per_second << std::endl;
}

TEST_F(TransactionThroughputBenchmark, ConcurrentTransactionSubmission) {
    const size_t transaction_count = 50;
    const size_t thread_count = 4;
    const size_t transactions_per_thread = transaction_count / thread_count;
    
    std::vector<std::thread> threads;
    std::atomic<size_t> success_count(0);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Submit transactions from multiple threads
    for (size_t t = 0; t < thread_count; ++t) {
        threads.emplace_back([this, transactions_per_thread, t, &success_count]() {
            for (size_t i = 0; i < transactions_per_thread; ++i) {
                auto tx = createTestTransaction(t * transactions_per_thread + i);
                if (node_runtime_->addTransaction(tx)) {
                    success_count++;
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    double tps = (success_count.load() * 1000.0) / duration;
    
    std::cout << "Concurrent submission: " << success_count.load() 
              << " transactions in " << duration << " ms" << std::endl;
    std::cout << "Throughput: " << tps << " TPS" << std::endl;
    
    EXPECT_EQ(success_count.load(), transaction_count);
}

TEST_F(TransactionThroughputBenchmark, MempoolCapacity) {
    const size_t transaction_count = 1000;
    size_t added_count = 0;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < transaction_count; ++i) {
        auto tx = createTestTransaction(i);
        if (node_runtime_->addTransaction(tx)) {
            added_count++;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    std::cout << "Added " << added_count << " transactions to mempool in " 
              << duration << " ms" << std::endl;
    std::cout << "Mempool size: " << node_runtime_->getMempoolSize() << std::endl;
    
    EXPECT_GT(added_count, 0);
    EXPECT_EQ(node_runtime_->getMempoolSize(), added_count);
}

// Backend comparison test
TEST_F(TransactionThroughputBenchmark, BackendComparison) {
    std::cout << "\n=== Backend Performance Comparison ===" << std::endl;
    
    const size_t transaction_count = 100;
    std::vector<std::shared_ptr<core::Transaction>> transactions;
    
    for (size_t i = 0; i < transaction_count; ++i) {
        transactions.push_back(createTestTransaction(i));
    }
    
    // Test JSON backend
    runWithBackend("json");
    auto json_start = std::chrono::high_resolution_clock::now();
    for (auto& tx : transactions) {
        node_runtime_->addTransaction(tx);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto json_end = std::chrono::high_resolution_clock::now();
    auto json_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        json_end - json_start).count();
    double json_tps = (transaction_count * 1000.0) / json_duration;
    
    // Test LevelDB backend
    runWithBackend("leveldb");
    auto leveldb_start = std::chrono::high_resolution_clock::now();
    for (auto& tx : transactions) {
        node_runtime_->addTransaction(tx);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto leveldb_end = std::chrono::high_resolution_clock::now();
    auto leveldb_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        leveldb_end - leveldb_start).count();
    double leveldb_tps = (transaction_count * 1000.0) / leveldb_duration;
    
    std::cout << "JSON Backend:" << std::endl;
    std::cout << "  Duration: " << json_duration << " ms" << std::endl;
    std::cout << "  Throughput: " << json_tps << " TPS" << std::endl;
    std::cout << "\nLevelDB Backend:" << std::endl;
    std::cout << "  Duration: " << leveldb_duration << " ms" << std::endl;
    std::cout << "  Throughput: " << leveldb_tps << " TPS" << std::endl;
    std::cout << "\nPerformance ratio: " << (leveldb_tps / json_tps) << "x" << std::endl;
}

} // namespace tests
} // namespace deo

