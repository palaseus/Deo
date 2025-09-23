/**
 * @file peer_connection_manager.h
 * @brief Advanced peer connection management with NAT traversal and reliability
 * @author Deo Blockchain Team
 * @version 1.0.0
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
#include <memory>
#include <queue>
#include <condition_variable>

#include "peer_manager.h"
#include "tcp_network.h"
#include "peer_authentication.h"
#include "utils/logger.h"
#include "crypto/key_pair.h"

namespace deo {
namespace network {

/**
 * @brief Connection state for a peer
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    BANNED,
    FAILED
};

/**
 * @brief NAT traversal method
 */
enum class NATTraversalMethod {
    NONE,
    UPNP,
    STUN,
    TURN,
    MANUAL
};

/**
 * @brief Enhanced peer connection information
 */
struct PeerConnectionInfo {
    std::string address;
    uint16_t port;
    std::string node_id;
    std::string public_key;
    
    // Connection state
    ConnectionState state;
    std::chrono::steady_clock::time_point last_connected;
    std::chrono::steady_clock::time_point last_seen;
    std::chrono::steady_clock::time_point last_attempt;
    
    // Reliability metrics
    uint32_t connection_attempts;
    uint32_t successful_connections;
    uint32_t failed_connections;
    uint32_t consecutive_failures;
    std::chrono::seconds backoff_duration;
    
    // Network information
    std::string external_ip;
    uint16_t external_port;
    NATTraversalMethod nat_method;
    bool is_nat_traversed;
    
    // Performance metrics
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t messages_sent;
    uint32_t messages_received;
    std::chrono::milliseconds avg_latency;
    
    // Reputation and behavior tracking
    int32_t reputation_score;
    std::chrono::system_clock::time_point last_activity;
    std::chrono::system_clock::time_point last_misbehavior;
    uint32_t misbehavior_count;
    uint32_t good_behavior_count;
    std::map<std::string, uint32_t> behavior_history; // behavior_type -> count
    std::chrono::system_clock::time_point ban_expiry;
    std::string ban_reason;
    uint32_t ban_count;
    std::vector<std::string> capabilities;
    
    PeerConnectionInfo() 
        : port(0)
        , state(ConnectionState::DISCONNECTED)
        , connection_attempts(0)
        , successful_connections(0)
        , failed_connections(0)
        , consecutive_failures(0)
        , backoff_duration(std::chrono::seconds(1))
        , external_port(0)
        , nat_method(NATTraversalMethod::NONE)
        , is_nat_traversed(false)
        , bytes_sent(0)
        , bytes_received(0)
        , messages_sent(0)
        , messages_received(0)
        , avg_latency(std::chrono::milliseconds(0))
        , reputation_score(100)
        , last_activity(std::chrono::system_clock::now())
        , last_misbehavior(std::chrono::system_clock::now())
        , misbehavior_count(0)
        , good_behavior_count(0)
        , ban_expiry(std::chrono::system_clock::now())
        , ban_reason("")
        , ban_count(0) {}
};

/**
 * @brief Connection attempt result
 */
struct ConnectionResult {
    bool success;
    std::string error_message;
    std::chrono::milliseconds connection_time;
    std::string external_ip;
    uint16_t external_port;
    
    ConnectionResult() 
        : success(false)
        , connection_time(std::chrono::milliseconds(0))
        , external_port(0) {}
};

/**
 * @brief Advanced peer connection manager with NAT traversal
 */
class PeerConnectionManager {
public:
    PeerConnectionManager();
    ~PeerConnectionManager();
    
    // Initialization and configuration
    bool initialize(const std::string& node_id, const std::string& private_key);
    void shutdown();
    
    // Peer management
    bool addPeer(const std::string& address, uint16_t port, const std::string& node_id = "");
    bool removePeer(const std::string& address, uint16_t port);
    bool connectToPeer(const std::string& address, uint16_t port);
    bool disconnectFromPeer(const std::string& address, uint16_t port);
    
    // Persistent peer management
    bool loadPeerList(const std::string& file_path);
    bool savePeerList(const std::string& file_path);
    void addPersistentPeer(const std::string& address, uint16_t port);
    void removePersistentPeer(const std::string& address, uint16_t port);
    
    // NAT traversal
    bool enableNATTraversal(NATTraversalMethod method);
    bool discoverExternalAddress();
    std::string getExternalIP() const;
    uint16_t getExternalPort() const;
    
    // Connection reliability
    void startReconnectionLoop();
    void stopReconnectionLoop();
    bool isReconnecting(const std::string& address, uint16_t port) const;
    
    // Peer information
    std::vector<PeerConnectionInfo> getConnectedPeers() const;
    std::vector<PeerConnectionInfo> getAllPeers() const;
    PeerConnectionInfo getPeerInfo(const std::string& address, uint16_t port) const;
    bool isConnected(const std::string& address, uint16_t port) const;
    
    // Statistics
    uint32_t getConnectedPeerCount() const;
    uint32_t getTotalPeerCount() const;
    uint32_t getReconnectingPeerCount() const;
    std::string getConnectionStatistics() const;
    
    // Reputation management
    void updatePeerReputation(const std::string& address, uint16_t port, int32_t score_delta);
    void banPeer(const std::string& address, uint16_t port, const std::string& reason);
    void unbanPeer(const std::string& address, uint16_t port);
    bool isPeerBanned(const std::string& address, uint16_t port) const;
    
    // Advanced reputation and behavior tracking
    void reportGoodBehavior(const std::string& address, uint16_t port, const std::string& behavior_type);
    void reportMisbehavior(const std::string& address, uint16_t port, const std::string& behavior_type, int32_t severity = 1);
    int32_t calculateReputationScore(const std::string& address, uint16_t port) const;
    std::chrono::seconds calculateBanDuration(const std::string& address, uint16_t port) const;
    void applyReputationDecay();
    void startReputationDecayLoop();
    void stopReputationDecayLoop();
    std::vector<std::string> getBannedPeers() const;
    std::vector<std::string> getLowReputationPeers(int32_t threshold = -100) const;
    std::string getReputationReport(const std::string& address, uint16_t port) const;
    
    // Peer authentication
    bool enableAuthentication(const AuthConfig& auth_config);
    void disableAuthentication();
    bool isAuthenticationEnabled() const;
    std::string generateAuthChallenge(const std::string& address, uint16_t port);
    bool verifyAuthResponse(const AuthResponse& response);
    bool isPeerAuthenticated(const std::string& address, uint16_t port) const;
    bool isPeerTrusted(const std::string& address, uint16_t port) const;
    std::shared_ptr<PeerIdentity> getPeerIdentity(const std::string& address, uint16_t port) const;
    void addTrustedPeer(const std::string& public_key, const std::string& node_id = "");
    void removeTrustedPeer(const std::string& public_key);
    void addBlacklistedPeer(const std::string& public_key, const std::string& reason = "");
    void removeBlacklistedPeer(const std::string& public_key);
    std::string signData(const std::string& data) const;
    bool verifyPeerSignature(const std::string& data, const std::string& signature, 
                            const std::string& address, uint16_t port) const;
    std::vector<PeerIdentity> getAuthenticatedPeers() const;
    std::vector<PeerIdentity> getTrustedPeers() const;
    std::string getAuthenticationStatistics() const;
    
    // Network events
    void onPeerConnected(const std::string& address, uint16_t port);
    void onPeerDisconnected(const std::string& address, uint16_t port, const std::string& reason);
    void onPeerMessage(const std::string& address, uint16_t port, const std::string& message_type);
    
private:
    // Core components
    std::unique_ptr<PeerManager> peer_manager_;
    std::unique_ptr<TcpNetworkManager> network_manager_;
    std::unique_ptr<crypto::KeyPair> node_keypair_;
    
    // Peer storage
    std::map<std::string, PeerConnectionInfo> peers_;
    std::set<std::string> persistent_peers_;
    std::set<std::string> banned_peers_;
    mutable std::mutex peers_mutex_;
    
    // Connection management
    std::atomic<bool> is_initialized_;
    std::atomic<bool> is_shutdown_;
    std::atomic<bool> reconnection_enabled_;
    
    // NAT traversal
    std::string external_ip_;
    uint16_t external_port_;
    NATTraversalMethod nat_method_;
    std::atomic<bool> nat_traversal_enabled_;
    
    // Reconnection logic
    std::thread reconnection_thread_;
    std::queue<std::string> reconnection_queue_;
    std::mutex reconnection_mutex_;
    std::condition_variable reconnection_cv_;
    
    // Configuration
    std::string node_id_;
    uint32_t max_connection_attempts_;
    std::chrono::seconds max_backoff_duration_;
    std::chrono::seconds connection_timeout_;
    
    // Internal methods
    void reconnectionLoop();
    ConnectionResult attemptConnection(const std::string& address, uint16_t port);
    void updateConnectionMetrics(const std::string& peer_key, bool success, 
                                std::chrono::milliseconds connection_time);
    void calculateBackoffDuration(PeerConnectionInfo& peer_info);
    bool shouldAttemptReconnection(const PeerConnectionInfo& peer_info) const;
    std::string getPeerKey(const std::string& address, uint16_t port) const;
    
    // NAT traversal methods
    bool performUPNPTraversal();
    bool performSTUNTraversal();
    bool performTURNTraversal();
    
    // Reputation calculation and decay
    int32_t calculateReputationScoreInternal(const PeerConnectionInfo& peer_info) const;
    void reputationDecayLoop();
    
    // Reputation decay thread management
    std::atomic<bool> reputation_decay_running_;
    std::thread reputation_decay_thread_;
    std::condition_variable reputation_decay_cv_;
    mutable std::mutex reputation_decay_mutex_;
    
    // Authentication management
    std::unique_ptr<PeerAuthentication> auth_manager_;
    bool authentication_enabled_;
    
    // Event handlers
    void handleConnectionSuccess(const std::string& peer_key);
    void handleConnectionFailure(const std::string& peer_key, const std::string& reason);
    void handlePeerMessage(const std::string& peer_key, const std::string& message_type);
};

} // namespace network
} // namespace deo
