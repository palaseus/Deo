/**
 * @file advanced_network.h
 * @brief Advanced network optimizations and features for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#ifndef DEO_ADVANCED_NETWORK_H
#define DEO_ADVANCED_NETWORK_H

#include "network_manager.h"
#include "tcp_network.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>

namespace deo::network {

/**
 * @brief Advanced network manager with load balancing and optimization
 */
class AdvancedNetworkManager : public NetworkManager {
public:
    AdvancedNetworkManager(const NetworkConfig& config);
    ~AdvancedNetworkManager();
    
    // Enhanced network operations
    bool initialize(std::shared_ptr<core::Blockchain> blockchain,
                   std::shared_ptr<consensus::ConsensusEngine> consensus_engine) override;
    void shutdown() override;
    
    // Load balancing
    bool enableLoadBalancing(bool enable);
    bool isLoadBalancingEnabled() const;
    void rebalanceConnections();
    std::unordered_map<std::string, double> getNodeLoads() const;
    
    // Connection optimization
    bool optimizeConnections();
    void analyzeConnectionPerformance();
    std::vector<std::string> getConnectionOptimizationRecommendations() const;
    
    // Message prioritization
    void setMessagePriority(const std::string& message_type, int priority);
    int getMessagePriority(const std::string& message_type) const;
    void processPriorityQueue();
    
    // Network statistics
    std::unordered_map<std::string, int> getStatistics() const override;
    std::unordered_map<std::string, double> getPerformanceMetrics() const;
    
private:
    bool load_balancing_enabled_;
    std::unordered_map<std::string, int> message_priorities_;
    std::queue<std::pair<int, NetworkMessage>> priority_queue_;
    std::mutex priority_queue_mutex_;
    std::unordered_map<std::string, double> node_loads_;
    
    void processPriorityMessage(const NetworkMessage& message);
    void updateNodeLoad(const std::string& node_id, double load);
    std::string selectOptimalNode() const;
};

/**
 * @brief Network load balancer for distributing network traffic
 */
class NetworkLoadBalancer {
public:
    NetworkLoadBalancer();
    ~NetworkLoadBalancer();
    
    // Load balancing operations
    bool addNode(const std::string& node_id, const std::string& address, uint16_t port);
    bool removeNode(const std::string& node_id);
    std::string selectNode(const std::string& request_type) const;
    
    // Load balancing algorithms
    void setLoadBalancingAlgorithm(const std::string& algorithm);
    std::string getLoadBalancingAlgorithm() const;
    void updateNodeLoad(const std::string& node_id, double load);
    
    // Health checking
    bool isNodeHealthy(const std::string& node_id) const;
    void performHealthCheck(const std::string& node_id);
    std::vector<std::string> getHealthyNodes() const;
    
    // Statistics
    std::unordered_map<std::string, double> getNodeStatistics() const;
    std::unordered_map<std::string, int> getLoadBalancingStatistics() const;
    
private:
    enum class LoadBalancingAlgorithm {
        ROUND_ROBIN,
        LEAST_CONNECTIONS,
        WEIGHTED_ROUND_ROBIN,
        LEAST_RESPONSE_TIME,
        IP_HASH
    };
    
    struct NetworkNode {
        std::string node_id;
        std::string address;
        uint16_t port;
        std::atomic<double> load{0.0};
        std::atomic<int> connections{0};
        std::atomic<uint64_t> requests_processed{0};
        std::atomic<bool> is_healthy{true};
        std::chrono::system_clock::time_point last_health_check;
    };
    
    std::unordered_map<std::string, NetworkNode> nodes_;
    LoadBalancingAlgorithm algorithm_;
    std::atomic<size_t> current_node_index_{0};
    std::mutex nodes_mutex_;
    
    std::string selectRoundRobin();
    std::string selectLeastConnections();
    std::string selectWeightedRoundRobin();
    std::string selectLeastResponseTime();
    std::string selectIpHash(const std::string& client_ip);
    void updateNodeHealth(const std::string& node_id, bool is_healthy);
};

/**
 * @brief Network message router for intelligent message routing
 */
class NetworkMessageRouter {
public:
    NetworkMessageRouter();
    ~NetworkMessageRouter();
    
    // Routing operations
    bool addRoute(const std::string& message_type, const std::string& target_node);
    bool removeRoute(const std::string& message_type);
    std::string getRoute(const std::string& message_type) const;
    
    // Message routing
    bool routeMessage(const NetworkMessage& message);
    bool broadcastMessage(const NetworkMessage& message);
    bool multicastMessage(const NetworkMessage& message, const std::vector<std::string>& target_nodes);
    
    // Routing optimization
    void optimizeRoutes();
    void analyzeRoutingPerformance();
    std::vector<std::string> getRoutingOptimizationRecommendations() const;
    
    // Statistics
    std::unordered_map<std::string, int> getRoutingStatistics() const;
    std::unordered_map<std::string, double> getRoutingPerformanceMetrics() const;
    
private:
    std::unordered_map<std::string, std::string> message_routes_;
    std::unordered_map<std::string, std::vector<std::string>> multicast_groups_;
    std::unordered_map<std::string, int> routing_statistics_;
    std::mutex routing_mutex_;
    
    bool isValidRoute(const std::string& message_type, const std::string& target_node) const;
    void updateRoutingStatistics(const std::string& message_type);
    std::string selectOptimalRoute(const std::string& message_type) const;
};

/**
 * @brief Network security manager for advanced security features
 */
class NetworkSecurityManager {
public:
    NetworkSecurityManager();
    ~NetworkSecurityManager();
    
    // Security operations
    bool enableEncryption(bool enable);
    bool isEncryptionEnabled() const;
    bool enableAuthentication(bool enable);
    bool isAuthenticationEnabled() const;
    
    // Access control
    bool addAllowedNode(const std::string& node_id, const std::string& public_key);
    bool removeAllowedNode(const std::string& node_id);
    bool isNodeAllowed(const std::string& node_id) const;
    
    // Threat detection
    bool detectThreat(const std::string& node_id, const std::string& activity);
    void blockNode(const std::string& node_id, const std::string& reason);
    void unblockNode(const std::string& node_id);
    bool isNodeBlocked(const std::string& node_id) const;
    
    // Security monitoring
    void startSecurityMonitoring();
    void stopSecurityMonitoring();
    std::unordered_map<std::string, int> getSecurityStatistics() const;
    
private:
    bool encryption_enabled_;
    bool authentication_enabled_;
    std::unordered_map<std::string, std::string> allowed_nodes_;
    std::unordered_map<std::string, std::string> blocked_nodes_;
    std::unordered_map<std::string, int> security_statistics_;
    std::atomic<bool> monitoring_active_;
    std::mutex security_mutex_;
    
    bool validateNodeAuthentication(const std::string& node_id, const std::string& signature) const;
    void updateSecurityStatistics(const std::string& threat_type);
    bool isThreatDetected(const std::string& activity) const;
};

/**
 * @brief Network performance monitor for monitoring network performance
 */
class NetworkPerformanceMonitor {
public:
    NetworkPerformanceMonitor();
    ~NetworkPerformanceMonitor();
    
    // Performance monitoring
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoringActive() const;
    
    // Performance metrics
    void recordMessageSent(const std::string& message_type, size_t size, double latency);
    void recordMessageReceived(const std::string& message_type, size_t size, double latency);
    void recordConnectionEstablished(const std::string& node_id);
    void recordConnectionLost(const std::string& node_id);
    
    // Performance analysis
    std::unordered_map<std::string, double> getPerformanceMetrics() const;
    std::vector<std::string> getPerformanceRecommendations() const;
    void analyzePerformance();
    
    // Statistics
    std::unordered_map<std::string, int> getStatistics() const;
    std::unordered_map<std::string, double> getDetailedMetrics() const;
    
private:
    struct PerformanceMetrics {
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<double> average_latency{0.0};
        std::atomic<double> peak_latency{0.0};
        std::atomic<uint64_t> connections_established{0};
        std::atomic<uint64_t> connections_lost{0};
        std::chrono::system_clock::time_point start_time;
    };
    
    PerformanceMetrics metrics_;
    std::unordered_map<std::string, PerformanceMetrics> message_metrics_;
    std::atomic<bool> monitoring_active_;
    std::mutex metrics_mutex_;
    
    void updateMetrics(PerformanceMetrics& metrics, size_t size, double latency);
    void calculateAverageLatency();
    void generatePerformanceRecommendations();
};

/**
 * @brief Network optimizer for optimizing network performance
 */
class NetworkOptimizer {
public:
    NetworkOptimizer();
    ~NetworkOptimizer();
    
    // Optimization operations
    void optimizeNetworkPerformance();
    void analyzeNetworkMetrics();
    std::vector<std::string> getOptimizationRecommendations() const;
    
    // Network tuning
    void tuneNetworkParameters(const std::unordered_map<std::string, double>& parameters);
    std::unordered_map<std::string, double> getOptimalParameters() const;
    
    // Performance optimization
    void optimizeMessageRouting();
    void optimizeConnectionPooling();
    void optimizeBandwidthUsage();
    
    // Statistics
    std::unordered_map<std::string, double> getOptimizationMetrics() const;
    std::unordered_map<std::string, int> getOptimizationStatistics() const;
    
private:
    std::unordered_map<std::string, double> network_parameters_;
    std::unordered_map<std::string, double> optimization_metrics_;
    std::vector<std::string> optimization_recommendations_;
    std::atomic<bool> optimization_active_;
    
    void analyzeNetworkPerformance();
    void generateOptimizationRecommendations();
    void updateOptimizationMetrics();
    void identifyOptimizationOpportunities();
};

} // namespace deo::network

#endif // DEO_ADVANCED_NETWORK_H
