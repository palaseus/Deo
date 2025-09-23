/**
 * @file peer_manager.h
 * @brief Peer management with discovery, scoring, and banning
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <chrono>
#include <atomic>
#include <thread>
#include <random>

#include "tcp_network.h"
#include "utils/logger.h"

namespace deo {
namespace network {

/**
 * @brief Peer information and behavior tracking
 */
struct PeerInfo {
    std::string address;
    uint16_t port;
    std::chrono::steady_clock::time_point first_seen;
    std::chrono::steady_clock::time_point last_seen;
    std::chrono::steady_clock::time_point last_activity;
    
    // Behavior scoring
    int32_t misbehavior_score;
    int32_t good_behavior_score;
    size_t messages_received;
    size_t invalid_messages;
    size_t blocks_provided;
    size_t transactions_provided;
    
    // Connection status
    bool connected;
    bool banned;
    std::chrono::steady_clock::time_point ban_until;
    
    // Network capabilities
    uint32_t version;
    std::string user_agent;
    uint64_t best_height;
    
    PeerInfo() : port(0), misbehavior_score(0), good_behavior_score(0),
                 messages_received(0), invalid_messages(0), blocks_provided(0),
                 transactions_provided(0), connected(false), banned(false),
                 version(0), best_height(0) {
        auto now = std::chrono::steady_clock::now();
        first_seen = now;
        last_seen = now;
        last_activity = now;
    }
};

/**
 * @brief Peer discovery and management system
 */
class PeerManager {
public:
    PeerManager();
    ~PeerManager();
    
    bool initialize();
    void shutdown();
    
    // Peer discovery
    void addBootstrapNode(const std::string& address, uint16_t port);
    void discoverPeers();
    void requestPeerList(const std::string& peer_address);
    
    // Peer management
    bool addPeer(const std::string& address, uint16_t port);
    void removePeer(const std::string& address, uint16_t port);
    void updatePeerActivity(const std::string& address, uint16_t port);
    void updatePeerHeight(const std::string& address, uint16_t port, uint64_t height);
    
    // Behavior scoring
    void reportGoodBehavior(const std::string& address, uint16_t port, int32_t score = 1);
    void reportMisbehavior(const std::string& address, uint16_t port, int32_t score = 1);
    void banPeer(const std::string& address, uint16_t port, std::chrono::seconds duration);
    void unbanPeer(const std::string& address, uint16_t port);
    
    // Peer queries
    std::vector<std::string> getConnectedPeers() const;
    std::vector<std::string> getBannedPeers() const;
    std::vector<std::string> getBestPeers(size_t count = 8) const;
    PeerInfo getPeerInfo(const std::string& address, uint16_t port) const;
    bool isPeerBanned(const std::string& address, uint16_t port) const;
    bool isPeerConnected(const std::string& address, uint16_t port) const;
    
    // Statistics
    struct PeerStats {
        size_t total_peers;
        size_t connected_peers;
        size_t banned_peers;
        size_t bootstrap_nodes;
        double average_misbehavior_score;
        double average_good_behavior_score;
    };
    
    PeerStats getPeerStats() const;
    
    // Rate limiting
    bool checkRateLimit(const std::string& address, uint16_t port, const std::string& message_type);
    void recordMessage(const std::string& address, uint16_t port, const std::string& message_type);
    
    // Cleanup
    void cleanupStalePeers();
    void cleanupExpiredBans();

private:
    mutable std::mutex peers_mutex_;
    std::map<std::string, PeerInfo> peers_;
    std::vector<std::pair<std::string, uint16_t>> bootstrap_nodes_;
    
    // Rate limiting
    struct RateLimitEntry {
        std::chrono::steady_clock::time_point last_request;
        size_t request_count;
        std::chrono::seconds window_duration;
        size_t max_requests;
    };
    
    mutable std::mutex rate_limit_mutex_;
    std::map<std::string, std::map<std::string, RateLimitEntry>> rate_limits_;
    
    // Cleanup thread
    std::atomic<bool> running_;
    std::thread cleanup_thread_;
    
    // Random number generation for peer selection
    mutable std::mt19937 rng_;
    
    std::string formatPeerKey(const std::string& address, uint16_t port) const;
    void cleanupLoop();
    bool isRateLimited(const std::string& peer_key, const std::string& message_type) const;
    void updateRateLimit(const std::string& peer_key, const std::string& message_type);
    std::vector<std::string> selectRandomPeers(size_t count) const;
};

/**
 * @brief Gossip protocol implementation
 */
class GossipProtocol {
public:
    GossipProtocol(std::shared_ptr<TcpNetworkManager> network_manager,
                   std::shared_ptr<PeerManager> peer_manager);
    ~GossipProtocol();
    
    // Message broadcasting
    void broadcastTransaction(const std::string& transaction_data);
    void broadcastBlock(const std::string& block_data);
    void broadcastMessage(const NetworkMessage& message);
    
    // Message handling
    void handleNewTransaction(const NetworkMessage& message, const std::string& peer_address);
    void handleNewBlock(const NetworkMessage& message, const std::string& peer_address);
    void handleRequestBlock(const NetworkMessage& message, const std::string& peer_address);
    void handleResponseBlock(const NetworkMessage& message, const std::string& peer_address);
    
    // Flood protection
    bool shouldPropagateMessage(const std::string& message_hash, const std::string& peer_address);
    void recordMessagePropagation(const std::string& message_hash, const std::string& peer_address);
    
    // Statistics
    struct GossipStats {
        size_t transactions_broadcasted;
        size_t blocks_broadcasted;
        size_t messages_propagated;
        size_t duplicate_messages_filtered;
        std::chrono::steady_clock::time_point start_time;
    };
    
    GossipStats getGossipStats() const;

private:
    std::shared_ptr<TcpNetworkManager> network_manager_;
    std::shared_ptr<PeerManager> peer_manager_;
    
    // Flood protection
    mutable std::mutex seen_messages_mutex_;
    std::map<std::string, std::set<std::string>> seen_messages_; // message_hash -> set of peer_addresses
    std::map<std::string, std::chrono::steady_clock::time_point> message_timestamps_;
    
    GossipStats stats_;
    mutable std::mutex stats_mutex_;
    
    void cleanupOldMessages();
    std::string calculateMessageHash(const NetworkMessage& message) const;
};

} // namespace network
} // namespace deo