/**
 * @file adversarial_testing.h
 * @brief Adversarial testing framework for the Deo Blockchain
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
#include "crypto/key_pair.h"

namespace deo {
namespace testing {

/**
 * @brief Adversarial attack type enumeration
 */
enum class AttackType {
    TRANSACTION_SPAM,      ///< Flood network with invalid transactions
    BLOCK_SPAM,           ///< Flood network with invalid blocks
    ECLIPSE_ATTACK,       ///< Isolate target node from honest peers
    SYBIL_ATTACK,         ///< Create multiple fake identities
    PARTITION_ATTACK,     ///< Split network into isolated partitions
    DOUBLE_SPEND_ATTACK,  ///< Attempt double spending
    INVALID_SIGNATURE,    ///< Send transactions with invalid signatures
    MALFORMED_MESSAGES,   ///< Send malformed network messages
    PROTOCOL_VIOLATION,   ///< Violate network protocol rules
    RESOURCE_EXHAUSTION,  ///< Exhaust node resources
    TIMING_ATTACK,        ///< Exploit timing vulnerabilities
    REPLAY_ATTACK,        ///< Replay old valid messages
    MIXED_ATTACK          ///< Combination of multiple attacks
};

/**
 * @brief Attack severity level
 */
enum class AttackSeverity {
    LOW,        ///< Low impact, easily detected
    MEDIUM,     ///< Medium impact, requires some defense
    HIGH,       ///< High impact, requires strong defense
    CRITICAL    ///< Critical impact, can compromise network
};

/**
 * @brief Adversarial test configuration
 */
struct AdversarialTestConfig {
    AttackType attack_type = AttackType::TRANSACTION_SPAM;
    AttackSeverity severity = AttackSeverity::MEDIUM;
    
    // Attack parameters
    uint32_t num_adversarial_nodes = 2;     ///< Number of adversarial nodes
    uint32_t num_honest_nodes = 5;          ///< Number of honest nodes
    uint32_t target_node_id = 0;            ///< Target node for focused attacks
    
    // Attack intensity
    uint32_t attack_rate_per_second = 100;  ///< Attack messages per second
    uint32_t attack_duration_seconds = 60;  ///< Attack duration
    uint32_t max_attack_messages = 10000;   ///< Maximum attack messages
    
    // Network parameters
    uint32_t base_port = 10000;             ///< Base port for nodes
    std::string data_directory = "/tmp/deo_adversarial_test"; ///< Data directory
    
    // Defense parameters
    bool enable_defense_mechanisms = true;  ///< Enable defense mechanisms
    uint32_t max_peers_per_node = 8;        ///< Maximum peers per node
    uint32_t connection_timeout_ms = 5000;  ///< Connection timeout
    uint32_t message_timeout_ms = 10000;    ///< Message timeout
    
    // Monitoring parameters
    bool enable_detailed_monitoring = true; ///< Enable detailed monitoring
    uint32_t monitoring_interval_ms = 1000; ///< Monitoring interval
    bool save_attack_logs = true;           ///< Save attack logs
    
    // Callbacks
    std::function<void(const std::string&)> log_callback;
    std::function<void(const std::string&)> error_callback;
    std::function<void(uint32_t, const std::string&)> progress_callback;
    std::function<void(AttackType, const std::string&)> attack_callback;
};

/**
 * @brief Attack statistics
 */
struct AttackStatistics {
    // Attack statistics
    uint64_t total_attacks_launched = 0;
    uint64_t successful_attacks = 0;
    uint64_t failed_attacks = 0;
    uint64_t blocked_attacks = 0;
    
    // Message statistics
    uint64_t total_messages_sent = 0;
    uint64_t invalid_messages_sent = 0;
    uint64_t malformed_messages_sent = 0;
    uint64_t spam_messages_sent = 0;
    
    // Network impact
    uint64_t network_partitions_created = 0;
    uint64_t nodes_eclipsed = 0;
    uint64_t connections_disrupted = 0;
    uint64_t messages_dropped = 0;
    
    // Defense statistics
    uint64_t attacks_detected = 0;
    uint64_t peers_banned = 0;
    uint64_t messages_filtered = 0;
    uint64_t rate_limits_triggered = 0;
    
    // Performance impact
    double average_response_time_ms = 0.0;
    double peak_cpu_usage_percent = 0.0;
    double peak_memory_usage_mb = 0.0;
    double network_throughput_reduction_percent = 0.0;
    
    // Timing
    std::chrono::system_clock::time_point attack_start_time;
    std::chrono::system_clock::time_point attack_end_time;
    std::chrono::seconds total_attack_duration{0};
};

/**
 * @brief Adversarial node information
 */
struct AdversarialNode {
    uint32_t node_id;
    std::string address;
    uint16_t port;
    std::string data_dir;
    
    AttackType attack_type;
    AttackSeverity severity;
    
    std::unique_ptr<network::PeerManager> peer_manager;
    std::unique_ptr<network::P2PNetworkManager> p2p_manager;
    std::unique_ptr<storage::LevelDBBlockStorage> block_storage;
    std::unique_ptr<storage::LevelDBStateStorage> state_storage;
    std::unique_ptr<crypto::KeyPair> key_pair;
    
    std::atomic<bool> is_attacking{false};
    std::atomic<uint64_t> attacks_launched{0};
    std::atomic<uint64_t> successful_attacks{0};
    std::atomic<uint64_t> failed_attacks{0};
    
    std::chrono::system_clock::time_point attack_start_time;
    std::chrono::system_clock::time_point last_attack_time;
};

/**
 * @brief Adversarial testing framework
 * 
 * This class provides comprehensive adversarial testing capabilities for the Deo blockchain,
 * including various attack simulations, defense mechanism testing, and security validation.
 */
class AdversarialTestingFramework {
public:
    /**
     * @brief Constructor
     */
    AdversarialTestingFramework();
    
    /**
     * @brief Destructor
     */
    ~AdversarialTestingFramework();
    
    // Disable copy and move semantics
    AdversarialTestingFramework(const AdversarialTestingFramework&) = delete;
    AdversarialTestingFramework& operator=(const AdversarialTestingFramework&) = delete;
    AdversarialTestingFramework(AdversarialTestingFramework&&) = delete;
    AdversarialTestingFramework& operator=(AdversarialTestingFramework&&) = delete;

    /**
     * @brief Initialize the adversarial testing framework
     * @param config Test configuration
     * @return True if initialization was successful
     */
    bool initialize(const AdversarialTestConfig& config);
    
    /**
     * @brief Shutdown the adversarial testing framework
     */
    void shutdown();

    /**
     * @brief Run adversarial test
     * @return True if test completed successfully
     */
    bool runAdversarialTest();
    
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
     * @brief Get attack statistics
     * @return Current attack statistics
     */
    AttackStatistics getAttackStatistics() const;
    
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
    bool updateConfig(const AdversarialTestConfig& config);
    
    /**
     * @brief Get current test configuration
     * @return Current test configuration
     */
    const AdversarialTestConfig& getConfig() const;

private:
    AdversarialTestConfig config_;
    mutable std::mutex config_mutex_;
    
    // Test state
    std::atomic<bool> test_running_{false};
    std::atomic<bool> test_active_{false};
    std::chrono::system_clock::time_point test_start_time_;
    
    // Test nodes
    std::vector<std::unique_ptr<AdversarialNode>> adversarial_nodes_;
    std::vector<std::unique_ptr<AdversarialNode>> honest_nodes_;
    mutable std::mutex nodes_mutex_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    AttackStatistics stats_;
    
    // Attack workers
    std::vector<std::thread> attack_threads_;
    std::atomic<bool> attacks_active_{false};
    
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
     * @brief Launch attack
     * @param node_id Attacking node ID
     * @param attack_type Type of attack to launch
     */
    void launchAttack(uint32_t node_id, AttackType attack_type);
    
    /**
     * @brief Transaction spam attack
     * @param node_id Attacking node ID
     */
    void transactionSpamAttack(uint32_t node_id);
    
    /**
     * @brief Block spam attack
     * @param node_id Attacking node ID
     */
    void blockSpamAttack(uint32_t node_id);
    
    /**
     * @brief Eclipse attack
     * @param node_id Attacking node ID
     */
    void eclipseAttack(uint32_t node_id);
    
    /**
     * @brief Sybil attack
     * @param node_id Attacking node ID
     */
    void sybilAttack(uint32_t node_id);
    
    /**
     * @brief Partition attack
     * @param node_id Attacking node ID
     */
    void partitionAttack(uint32_t node_id);
    
    /**
     * @brief Double spend attack
     * @param node_id Attacking node ID
     */
    void doubleSpendAttack(uint32_t node_id);
    
    /**
     * @brief Invalid signature attack
     * @param node_id Attacking node ID
     */
    void invalidSignatureAttack(uint32_t node_id);
    
    /**
     * @brief Malformed message attack
     * @param node_id Attacking node ID
     */
    void malformedMessageAttack(uint32_t node_id);
    
    /**
     * @brief Protocol violation attack
     * @param node_id Attacking node ID
     */
    void protocolViolationAttack(uint32_t node_id);
    
    /**
     * @brief Resource exhaustion attack
     * @param node_id Attacking node ID
     */
    void resourceExhaustionAttack(uint32_t node_id);
    
    /**
     * @brief Timing attack
     * @param node_id Attacking node ID
     */
    void timingAttack(uint32_t node_id);
    
    /**
     * @brief Replay attack
     * @param node_id Attacking node ID
     */
    void replayAttack(uint32_t node_id);
    
    /**
     * @brief Generate malicious transaction
     * @param node_id Attacking node ID
     * @param attack_type Type of malicious transaction
     * @return Generated malicious transaction
     */
    std::shared_ptr<core::Transaction> generateMaliciousTransaction(uint32_t node_id, AttackType attack_type);
    
    /**
     * @brief Generate malicious block
     * @param node_id Attacking node ID
     * @param attack_type Type of malicious block
     * @return Generated malicious block
     */
    std::shared_ptr<core::Block> generateMaliciousBlock(uint32_t node_id, AttackType attack_type);
    
    /**
     * @brief Generate malformed message
     * @param node_id Attacking node ID
     * @return Generated malformed message
     */
    std::string generateMalformedMessage(uint32_t node_id);
    
    /**
     * @brief Update attack statistics
     */
    void updateAttackStatistics();
    
    /**
     * @brief Log attack message
     * @param attack_type Type of attack
     * @param message Attack message
     */
    void logAttack(AttackType attack_type, const std::string& message);
    
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
     * @param is_adversarial Whether node is adversarial
     * @return Data directory path
     */
    std::string createTestDataDirectory(uint32_t node_id, bool is_adversarial);
    
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
 * @brief Attack simulator
 * 
 * This class simulates various types of attacks against the blockchain network
 * to test defense mechanisms and network resilience.
 */
class AttackSimulator {
public:
    /**
     * @brief Constructor
     * @param attack_type Type of attack to simulate
     * @param severity Attack severity level
     */
    AttackSimulator(AttackType attack_type, AttackSeverity severity);
    
    /**
     * @brief Destructor
     */
    ~AttackSimulator();

    /**
     * @brief Start attack simulation
     * @param duration Attack duration
     * @return True if attack was started successfully
     */
    bool startAttack(std::chrono::seconds duration);
    
    /**
     * @brief Stop attack simulation
     */
    void stopAttack();
    
    /**
     * @brief Check if attack is active
     * @return True if attack is currently active
     */
    bool isAttackActive() const;
    
    /**
     * @brief Get attack statistics
     * @return Attack statistics
     */
    AttackStatistics getAttackStatistics() const;
    
    /**
     * @brief Get attack success rate
     * @return Attack success rate (0.0 to 1.0)
     */
    double getAttackSuccessRate() const;

private:
    AttackType attack_type_;
    AttackSeverity severity_;
    std::atomic<bool> attack_active_{false};
    std::atomic<uint64_t> attacks_launched_{0};
    std::atomic<uint64_t> successful_attacks_{0};
    std::atomic<uint64_t> failed_attacks_{0};
    
    std::thread attack_thread_;
    std::mt19937 rng_;
    
    /**
     * @brief Attack simulation loop
     */
    void attackLoop();
    
    /**
     * @brief Execute single attack
     * @return True if attack was successful
     */
    bool executeAttack();
    
    /**
     * @brief Calculate attack intensity
     * @return Attack intensity (0.0 to 1.0)
     */
    double calculateAttackIntensity() const;
};

/**
 * @brief Defense mechanism tester
 * 
 * This class tests various defense mechanisms against adversarial attacks
 * and measures their effectiveness.
 */
class DefenseMechanismTester {
public:
    /**
     * @brief Constructor
     */
    DefenseMechanismTester();
    
    /**
     * @brief Destructor
     */
    ~DefenseMechanismTester();

    /**
     * @brief Test rate limiting
     * @param attack_rate Attack rate to test
     * @return True if rate limiting was effective
     */
    bool testRateLimiting(uint32_t attack_rate);
    
    /**
     * @brief Test peer banning
     * @param malicious_peers Number of malicious peers
     * @return True if peer banning was effective
     */
    bool testPeerBanning(uint32_t malicious_peers);
    
    /**
     * @brief Test message filtering
     * @param invalid_messages Number of invalid messages
     * @return True if message filtering was effective
     */
    bool testMessageFiltering(uint32_t invalid_messages);
    
    /**
     * @brief Test resource protection
     * @param resource_attack_intensity Attack intensity
     * @return True if resource protection was effective
     */
    bool testResourceProtection(double resource_attack_intensity);
    
    /**
     * @brief Test consensus resilience
     * @param adversarial_nodes Number of adversarial nodes
     * @return True if consensus remained resilient
     */
    bool testConsensusResilience(uint32_t adversarial_nodes);
    
    /**
     * @brief Get defense effectiveness
     * @return Defense effectiveness score (0.0 to 1.0)
     */
    double getDefenseEffectiveness() const;
    
    /**
     * @brief Get defense statistics
     * @return JSON string with defense statistics
     */
    std::string getDefenseStatistics() const;

private:
    std::atomic<uint64_t> attacks_blocked_{0};
    std::atomic<uint64_t> attacks_detected_{0};
    std::atomic<uint64_t> false_positives_{0};
    std::atomic<uint64_t> false_negatives_{0};
    
    /**
     * @brief Update defense statistics
     * @param attack_blocked Whether attack was blocked
     * @param attack_detected Whether attack was detected
     */
    void updateDefenseStatistics(bool attack_blocked, bool attack_detected);
    
    /**
     * @brief Calculate defense effectiveness
     * @return Defense effectiveness score
     */
    double calculateDefenseEffectiveness() const;
};

} // namespace testing
} // namespace deo

