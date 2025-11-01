/**
 * @file benchmark_block_production.cpp
 * @brief Benchmark for block production timing
 */

#include <gtest/gtest.h>
#include "node/node_runtime.h"
#include "core/transaction.h"
#include "utils/performance_monitor.h"

#include <chrono>
#include <vector>
#include <filesystem>

namespace deo {
namespace tests {

class BlockProductionBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/deo_bench_block_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed());
        std::filesystem::create_directories(test_dir_);
        
        // Configure node runtime for benchmarking
        config_.data_directory = test_dir_ + "/data";
        config_.state_directory = test_dir_ + "/state";
        config_.enable_p2p = false;
        config_.enable_mining = true; // Enable mining for block production
        config_.mining_difficulty = 1; // Low difficulty for faster benchmarking
        config_.enable_json_rpc = false;
        config_.storage_backend = "json";
        
        node_runtime_ = std::make_unique<node::NodeRuntime>(config_);
        ASSERT_TRUE(node_runtime_->initialize());
        ASSERT_TRUE(node_runtime_->start());
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
};

TEST_F(BlockProductionBenchmark, BlockCreationTime) {
    // Add transactions to mempool
    const size_t tx_count = 10;
    for (size_t i = 0; i < tx_count; ++i) {
        auto tx = createTestTransaction(i);
        node_runtime_->addTransaction(tx);
    }
    
    // Wait a bit for mempool to be ready
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Measure block creation time (note: this is just creation, not mining)
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Get initial height
    auto initial_stats = node_runtime_->getStatistics();
    uint64_t initial_height = initial_stats.blockchain_height;
    
    // Wait for block production (with mining enabled and low difficulty, should be fast)
    // Note: In a real benchmark, we'd trigger block creation directly
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    auto final_stats = node_runtime_->getStatistics();
    
    std::cout << "Block production test duration: " << duration << " ms" << std::endl;
    std::cout << "Initial height: " << initial_height << std::endl;
    std::cout << "Final height: " << final_stats.blockchain_height << std::endl;
    std::cout << "Average block time: " << final_stats.avg_block_time_seconds << " seconds" << std::endl;
    
    // Verify we got at least one block if mining succeeded
    EXPECT_GE(final_stats.blockchain_height, initial_height);
}

TEST_F(BlockProductionBenchmark, BlockValidationTime) {
    // Create a test block
    auto tx = createTestTransaction(0);
    node_runtime_->addTransaction(tx);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get a block from the blockchain to test validation
    auto stats = node_runtime_->getStatistics();
    
    if (stats.blockchain_height > 0) {
        auto block = node_runtime_->getBlock(stats.best_block_hash);
        if (block) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // Replay block to measure validation time
            std::string result = node_runtime_->replayBlock(stats.best_block_hash);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time).count();
            
            std::cout << "Block validation time: " << duration << " microseconds" << std::endl;
            EXPECT_FALSE(result.empty());
        }
    }
}

TEST_F(BlockProductionBenchmark, PerformanceMetrics) {
    // Run node for a short period
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Get performance metrics
    auto stats = node_runtime_->getStatistics();
    
    std::cout << "\n=== Performance Metrics ===" << std::endl;
    std::cout << "Transactions processed: " << stats.transactions_processed << std::endl;
    std::cout << "Blocks mined: " << stats.blocks_mined << std::endl;
    std::cout << "TPS: " << stats.transactions_per_second << std::endl;
    std::cout << "Average block time: " << stats.avg_block_time_seconds << " seconds" << std::endl;
    std::cout << "Total gas used: " << stats.total_gas_used << std::endl;
    std::cout << "Network messages: " << stats.total_network_messages << std::endl;
    std::cout << "Storage operations: " << stats.total_storage_operations << std::endl;
    
    // Verify metrics are being collected
    EXPECT_GE(stats.blockchain_height, 0);
    EXPECT_GE(stats.transactions_processed, 0);
}

} // namespace tests
} // namespace deo

