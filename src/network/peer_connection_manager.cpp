/**
 * @file peer_connection_manager.cpp
 * @brief Advanced peer connection management with NAT traversal and reliability
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "network/peer_connection_manager.h"
#include "utils/error_handler.h"
#include "utils/config.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <chrono>
#include <cstdlib>

namespace deo {
namespace network {

PeerConnectionManager::PeerConnectionManager()
    : is_initialized_(false)
    , is_shutdown_(false)
    , reconnection_enabled_(false)
    , external_port_(0)
    , nat_method_(NATTraversalMethod::NONE)
    , nat_traversal_enabled_(false)
    , max_connection_attempts_(5)
    , max_backoff_duration_(std::chrono::seconds(300)) // 5 minutes
    , connection_timeout_(std::chrono::seconds(30))
    , reputation_decay_running_(false)
    , authentication_enabled_(false) {
    
    DEO_LOG_DEBUG(NETWORKING, "Peer Connection Manager created");
}

PeerConnectionManager::~PeerConnectionManager() {
    shutdown();
    DEO_LOG_DEBUG(NETWORKING, "Peer Connection Manager destroyed");
}

bool PeerConnectionManager::initialize(const std::string& node_id, const std::string& private_key) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    if (is_initialized_) {
        DEO_LOG_WARNING(NETWORKING, "Peer Connection Manager already initialized");
        return true;
    }
    
    try {
        // Initialize node keypair
        if (!private_key.empty()) {
            node_keypair_ = std::make_unique<crypto::KeyPair>(private_key);
        } else {
            node_keypair_ = std::make_unique<crypto::KeyPair>();
        }
        
        node_id_ = node_id;
        
        // Initialize peer manager
        peer_manager_ = std::make_unique<PeerManager>();
        if (!peer_manager_->initialize()) {
            DEO_ERROR(NETWORKING, "Failed to initialize peer manager");
            return false;
        }
        
        // Initialize network manager with a random port to avoid conflicts
        uint16_t random_port = 8000 + (std::rand() % 1000); // Use port 8000-8999
        network_manager_ = std::make_unique<TcpNetworkManager>(random_port);
        if (!network_manager_->initialize()) {
            DEO_ERROR(NETWORKING, "Failed to initialize network manager on port " + std::to_string(random_port));
            return false;
        }
        
        is_initialized_ = true;
        DEO_LOG_INFO(NETWORKING, "Peer Connection Manager initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_ERROR(NETWORKING, "Failed to initialize Peer Connection Manager: " + std::string(e.what()));
        return false;
    }
}

bool PeerConnectionManager::initializeForTesting(const std::string& node_id, const std::string& private_key) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    if (is_initialized_) {
        DEO_LOG_WARNING(NETWORKING, "Peer Connection Manager already initialized");
        return true;
    }
    
    try {
        // Initialize node keypair
        if (!private_key.empty()) {
            node_keypair_ = std::make_unique<crypto::KeyPair>(private_key);
        } else {
            node_keypair_ = std::make_unique<crypto::KeyPair>();
        }
        
        node_id_ = node_id;
        
        // Initialize peer manager
        peer_manager_ = std::make_unique<PeerManager>();
        if (!peer_manager_->initialize()) {
            DEO_ERROR(NETWORKING, "Failed to initialize peer manager");
            return false;
        }
        
        // Initialize network manager with a random port to avoid conflicts
        // Use test-friendly initialization that doesn't start background threads
        uint16_t random_port = 8000 + (std::rand() % 1000); // Use port 8000-8999
        network_manager_ = std::make_unique<TcpNetworkManager>(random_port);
        if (!network_manager_->initializeForTesting()) {
            DEO_ERROR(NETWORKING, "Failed to initialize network manager for testing on port " + std::to_string(random_port));
            return false;
        }
        
        is_initialized_ = true;
        DEO_LOG_INFO(NETWORKING, "Peer Connection Manager initialized for testing successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_ERROR(NETWORKING, "Failed to initialize Peer Connection Manager for testing: " + std::string(e.what()));
        return false;
    }
}

void PeerConnectionManager::shutdown() {
    if (is_shutdown_) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Shutting down Peer Connection Manager");
    
    // Stop reconnection loop
    stopReconnectionLoop();
    
    // Stop reputation decay loop
    stopReputationDecayLoop();
    
    // Disconnect all peers
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        for (auto& [peer_key, peer_info] : peers_) {
            if (peer_info.state == ConnectionState::CONNECTED) {
                network_manager_->disconnectPeer(peer_info.address, peer_info.port);
                peer_info.state = ConnectionState::DISCONNECTED;
            }
        }
    }
    
    // Shutdown components
    if (network_manager_) {
        network_manager_->shutdown();
    }
    
    if (peer_manager_) {
        peer_manager_->shutdown();
    }
    
    is_shutdown_ = true;
    is_initialized_ = false;
    
    DEO_LOG_INFO(NETWORKING, "Peer Connection Manager shutdown complete");
}

bool PeerConnectionManager::addPeer(const std::string& address, uint16_t port, const std::string& node_id) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    if (!is_initialized_) {
        DEO_ERROR(NETWORKING, "Peer Connection Manager not initialized");
        return false;
    }
    
    std::string peer_key = getPeerKey(address, port);
    
    // Check if peer already exists
    if (peers_.find(peer_key) != peers_.end()) {
        DEO_LOG_WARNING(NETWORKING, "Peer already exists: " + peer_key);
        return true;
    }
    
    // Create new peer info
    PeerConnectionInfo peer_info;
    peer_info.address = address;
    peer_info.port = port;
    peer_info.node_id = node_id;
    peer_info.state = ConnectionState::DISCONNECTED;
    peer_info.last_attempt = std::chrono::steady_clock::now();
    
    peers_[peer_key] = peer_info;
    
    DEO_LOG_INFO(NETWORKING, "Added peer: " + peer_key);
    return true;
}

bool PeerConnectionManager::removePeer(const std::string& address, uint16_t port) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it == peers_.end()) {
        DEO_LOG_WARNING(NETWORKING, "Peer not found: " + peer_key);
        return false;
    }
    
    // Disconnect if connected
    if (it->second.state == ConnectionState::CONNECTED) {
        network_manager_->disconnectPeer(address, port);
    }
    
    peers_.erase(it);
    persistent_peers_.erase(peer_key);
    
    DEO_LOG_INFO(NETWORKING, "Removed peer: " + peer_key);
    return true;
}

bool PeerConnectionManager::connectToPeer(const std::string& address, uint16_t port) {
    if (!is_initialized_) {
        DEO_ERROR(NETWORKING, "Peer Connection Manager not initialized");
        return false;
    }
    
    std::string peer_key = getPeerKey(address, port);
    
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        auto it = peers_.find(peer_key);
        if (it == peers_.end()) {
            // Add peer if not exists
            if (!addPeer(address, port)) {
                return false;
            }
        }
    }
    
    // Attempt connection
    ConnectionResult result = attemptConnection(address, port);
    
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        auto it = peers_.find(peer_key);
        if (it != peers_.end()) {
            updateConnectionMetrics(peer_key, result.success, result.connection_time);
            
            if (result.success) {
                it->second.state = ConnectionState::CONNECTED;
                it->second.last_connected = std::chrono::steady_clock::now();
                it->second.external_ip = result.external_ip;
                it->second.external_port = result.external_port;
                handleConnectionSuccess(peer_key);
            } else {
                it->second.state = ConnectionState::FAILED;
                handleConnectionFailure(peer_key, result.error_message);
            }
        }
    }
    
    return result.success;
}

bool PeerConnectionManager::disconnectFromPeer(const std::string& address, uint16_t port) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it == peers_.end() || it->second.state != ConnectionState::CONNECTED) {
        DEO_LOG_WARNING(NETWORKING, "Peer not connected: " + peer_key);
        return false;
    }
    
    network_manager_->disconnectPeer(address, port);
    it->second.state = ConnectionState::DISCONNECTED;
    
    onPeerDisconnected(address, port, "Manual disconnect");
    
    DEO_LOG_INFO(NETWORKING, "Disconnected from peer: " + peer_key);
    return true;
}

bool PeerConnectionManager::loadPeerList(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        DEO_LOG_WARNING(NETWORKING, "Could not open peer list file: " + file_path);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string line;
    uint32_t loaded_count = 0;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue; // Skip empty lines and comments
        }
        
        std::istringstream iss(line);
        std::string address;
        uint16_t port;
        std::string node_id;
        
        if (iss >> address >> port >> node_id) {
            std::string peer_key = getPeerKey(address, port);
            
            PeerConnectionInfo peer_info;
            peer_info.address = address;
            peer_info.port = port;
            peer_info.node_id = node_id;
            peer_info.state = ConnectionState::DISCONNECTED;
            peer_info.last_attempt = std::chrono::steady_clock::now();
            
            peers_[peer_key] = peer_info;
            persistent_peers_.insert(peer_key);
            loaded_count++;
        }
    }
    
    file.close();
    
    DEO_LOG_INFO(NETWORKING, "Loaded " + std::to_string(loaded_count) + " peers from: " + file_path);
    return true;
}

bool PeerConnectionManager::savePeerList(const std::string& file_path) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        DEO_ERROR(NETWORKING, "Could not open peer list file for writing: " + file_path);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    file << "# Deo Blockchain Peer List\n";
    file << "# Format: address port node_id\n";
    file << "# Generated: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "\n\n";
    
    uint32_t saved_count = 0;
    for (const auto& [peer_key, peer_info] : peers_) {
        if (persistent_peers_.find(peer_key) != persistent_peers_.end()) {
            file << peer_info.address << " " << peer_info.port << " " << peer_info.node_id << "\n";
            saved_count++;
        }
    }
    
    file.close();
    
    DEO_LOG_INFO(NETWORKING, "Saved " + std::to_string(saved_count) + " peers to: " + file_path);
    return true;
}

void PeerConnectionManager::addPersistentPeer(const std::string& address, uint16_t port) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    persistent_peers_.insert(peer_key);
    
    DEO_LOG_INFO(NETWORKING, "Added persistent peer: " + peer_key);
}

void PeerConnectionManager::removePersistentPeer(const std::string& address, uint16_t port) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    persistent_peers_.erase(peer_key);
    
    DEO_LOG_INFO(NETWORKING, "Removed persistent peer: " + peer_key);
}

bool PeerConnectionManager::enableNATTraversal(NATTraversalMethod method) {
    nat_method_ = method;
    nat_traversal_enabled_ = true;
    
    DEO_LOG_INFO(NETWORKING, "Enabled NAT traversal method: " + std::to_string(static_cast<int>(method)));
    
    return discoverExternalAddress();
}

bool PeerConnectionManager::discoverExternalAddress() {
    if (!nat_traversal_enabled_) {
        return false;
    }
    
    bool success = false;
    
    switch (nat_method_) {
        case NATTraversalMethod::UPNP:
            success = performUPNPTraversal();
            break;
        case NATTraversalMethod::STUN:
            success = performSTUNTraversal();
            break;
        case NATTraversalMethod::TURN:
            success = performTURNTraversal();
            break;
        default:
            DEO_LOG_WARNING(NETWORKING, "Unknown NAT traversal method");
            break;
    }
    
    if (success) {
        DEO_LOG_INFO(NETWORKING, "External address discovered: " + external_ip_ + ":" + std::to_string(external_port_));
    } else {
        DEO_LOG_WARNING(NETWORKING, "Failed to discover external address");
    }
    
    return success;
}

std::string PeerConnectionManager::getExternalIP() const {
    return external_ip_;
}

uint16_t PeerConnectionManager::getExternalPort() const {
    return external_port_;
}

void PeerConnectionManager::startReconnectionLoop() {
    if (reconnection_enabled_) {
        return;
    }
    
    reconnection_enabled_ = true;
    reconnection_thread_ = std::thread(&PeerConnectionManager::reconnectionLoop, this);
    
    DEO_LOG_INFO(NETWORKING, "Started reconnection loop");
}

void PeerConnectionManager::stopReconnectionLoop() {
    if (!reconnection_enabled_) {
        return;
    }
    
    reconnection_enabled_ = false;
    reconnection_cv_.notify_all();
    
    if (reconnection_thread_.joinable()) {
        reconnection_thread_.join();
    }
    
    DEO_LOG_INFO(NETWORKING, "Stopped reconnection loop");
}

bool PeerConnectionManager::isReconnecting(const std::string& address, uint16_t port) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    return it != peers_.end() && it->second.state == ConnectionState::RECONNECTING;
}

std::vector<PeerConnectionInfo> PeerConnectionManager::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::vector<PeerConnectionInfo> connected_peers;
    for (const auto& [peer_key, peer_info] : peers_) {
        if (peer_info.state == ConnectionState::CONNECTED) {
            connected_peers.push_back(peer_info);
        }
    }
    
    return connected_peers;
}

std::vector<PeerConnectionInfo> PeerConnectionManager::getAllPeers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::vector<PeerConnectionInfo> all_peers;
    for (const auto& [peer_key, peer_info] : peers_) {
        all_peers.push_back(peer_info);
    }
    
    return all_peers;
}

PeerConnectionInfo PeerConnectionManager::getPeerInfo(const std::string& address, uint16_t port) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it != peers_.end()) {
        return it->second;
    }
    
    return PeerConnectionInfo(); // Return default info if not found
}

bool PeerConnectionManager::isConnected(const std::string& address, uint16_t port) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    return it != peers_.end() && it->second.state == ConnectionState::CONNECTED;
}

uint32_t PeerConnectionManager::getConnectedPeerCount() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    uint32_t count = 0;
    for (const auto& [peer_key, peer_info] : peers_) {
        if (peer_info.state == ConnectionState::CONNECTED) {
            count++;
        }
    }
    
    return count;
}

uint32_t PeerConnectionManager::getTotalPeerCount() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return static_cast<uint32_t>(peers_.size());
}

uint32_t PeerConnectionManager::getReconnectingPeerCount() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    uint32_t count = 0;
    for (const auto& [peer_key, peer_info] : peers_) {
        if (peer_info.state == ConnectionState::RECONNECTING) {
            count++;
        }
    }
    
    return count;
}

std::string PeerConnectionManager::getConnectionStatistics() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    uint32_t connected = 0;
    uint32_t connecting = 0;
    uint32_t reconnecting = 0;
    uint32_t failed = 0;
    uint32_t banned = 0;
    
    for (const auto& [peer_key, peer_info] : peers_) {
        switch (peer_info.state) {
            case ConnectionState::CONNECTED:
                connected++;
                break;
            case ConnectionState::CONNECTING:
                connecting++;
                break;
            case ConnectionState::RECONNECTING:
                reconnecting++;
                break;
            case ConnectionState::FAILED:
                failed++;
                break;
            case ConnectionState::BANNED:
                banned++;
                break;
            default:
                break;
        }
    }
    
    std::ostringstream oss;
    oss << "Connection Statistics:\n"
        << "  Total peers: " << peers_.size() << "\n"
        << "  Connected: " << connected << "\n"
        << "  Connecting: " << connecting << "\n"
        << "  Reconnecting: " << reconnecting << "\n"
        << "  Failed: " << failed << "\n"
        << "  Banned: " << banned << "\n"
        << "  External IP: " << external_ip_ << ":" << external_port_;
    
    return oss.str();
}

void PeerConnectionManager::updatePeerReputation(const std::string& address, uint16_t port, int32_t score_delta) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it != peers_.end()) {
        it->second.reputation_score += score_delta;
        it->second.reputation_score = std::max(-1000, std::min(1000, it->second.reputation_score));
        
        DEO_LOG_DEBUG(NETWORKING, "Updated reputation for " + peer_key + 
                     " by " + std::to_string(score_delta) + 
                     " (new score: " + std::to_string(it->second.reputation_score) + ")");
    }
}

void PeerConnectionManager::banPeer(const std::string& address, uint16_t port, const std::string& reason) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    banned_peers_.insert(peer_key);
    
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        // Calculate ban duration based on reputation and ban history
        std::chrono::seconds ban_duration = calculateBanDuration(address, port);
        
        it->second.state = ConnectionState::BANNED;
        it->second.reputation_score = -1000;
        it->second.ban_expiry = std::chrono::system_clock::now() + ban_duration;
        it->second.ban_reason = reason;
        it->second.ban_count++;
        
        // Disconnect if connected
        if (it->second.state == ConnectionState::CONNECTED) {
            network_manager_->disconnectPeer(address, port);
        }
        
        DEO_LOG_WARNING(NETWORKING, "Banned peer " + peer_key + " for: " + reason + 
                       " (duration: " + std::to_string(ban_duration.count()) + "s, ban count: " + 
                       std::to_string(it->second.ban_count) + ")");
    } else {
        DEO_LOG_WARNING(NETWORKING, "Banned unknown peer " + peer_key + " for: " + reason);
    }
}

void PeerConnectionManager::unbanPeer(const std::string& address, uint16_t port) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    banned_peers_.erase(peer_key);
    
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.state = ConnectionState::DISCONNECTED;
        it->second.reputation_score = 0;
    }
    
    DEO_LOG_INFO(NETWORKING, "Unbanned peer: " + peer_key);
}

bool PeerConnectionManager::isPeerBanned(const std::string& address, uint16_t port) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    return banned_peers_.find(peer_key) != banned_peers_.end();
}

void PeerConnectionManager::onPeerConnected(const std::string& address, uint16_t port) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it != peers_.end()) {
        it->second.state = ConnectionState::CONNECTED;
        it->second.last_connected = std::chrono::steady_clock::now();
        it->second.last_seen = std::chrono::steady_clock::now();
        it->second.consecutive_failures = 0;
        it->second.successful_connections++;
        
        updatePeerReputation(address, port, 10); // Reward successful connection
    }
    
    DEO_LOG_INFO(NETWORKING, "Peer connected: " + peer_key);
}

void PeerConnectionManager::onPeerDisconnected(const std::string& address, uint16_t port, const std::string& reason) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it != peers_.end()) {
        it->second.state = ConnectionState::DISCONNECTED;
        it->second.consecutive_failures++;
        it->second.failed_connections++;
        
        updatePeerReputation(address, port, -5); // Penalty for disconnection
        
        // Add to reconnection queue if persistent peer
        if (persistent_peers_.find(peer_key) != persistent_peers_.end() && 
            !isPeerBanned(address, port)) {
            std::lock_guard<std::mutex> queue_lock(reconnection_mutex_);
            reconnection_queue_.push(peer_key);
            reconnection_cv_.notify_one();
        }
    }
    
    DEO_LOG_INFO(NETWORKING, "Peer disconnected: " + peer_key + " (" + reason + ")");
}

void PeerConnectionManager::onPeerMessage(const std::string& address, uint16_t port, const std::string& message_type) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it != peers_.end()) {
        it->second.last_seen = std::chrono::steady_clock::now();
        it->second.messages_received++;
        
        // Update reputation based on message type
        if (message_type == "block" || message_type == "transaction") {
            updatePeerReputation(address, port, 1); // Reward useful messages
        }
    }
    
    handlePeerMessage(peer_key, message_type);
}

// Private methods implementation
void PeerConnectionManager::reconnectionLoop() {
    while (reconnection_enabled_) {
        std::unique_lock<std::mutex> lock(reconnection_mutex_);
        
        // Wait for reconnection requests or shutdown
        reconnection_cv_.wait(lock, [this] { 
            return !reconnection_queue_.empty() || !reconnection_enabled_; 
        });
        
        if (!reconnection_enabled_) {
            break;
        }
        
        if (!reconnection_queue_.empty()) {
            std::string peer_key = reconnection_queue_.front();
            reconnection_queue_.pop();
            lock.unlock();
            
            // Parse peer key to get address and port
            size_t colon_pos = peer_key.find(':');
            if (colon_pos != std::string::npos) {
                std::string address = peer_key.substr(0, colon_pos);
                uint16_t port = static_cast<uint16_t>(std::stoi(peer_key.substr(colon_pos + 1)));
                
                // Check if we should attempt reconnection
                {
                    std::lock_guard<std::mutex> peers_lock(peers_mutex_);
                    auto it = peers_.find(peer_key);
                    if (it != peers_.end() && shouldAttemptReconnection(it->second)) {
                        it->second.state = ConnectionState::RECONNECTING;
                    } else {
                        continue; // Skip this peer
                    }
                }
                
                // Attempt reconnection
                DEO_LOG_INFO(NETWORKING, "Attempting to reconnect to: " + peer_key);
                bool success = connectToPeer(address, port);
                
                if (!success) {
                    // Calculate backoff and schedule next attempt
                    std::lock_guard<std::mutex> peers_lock(peers_mutex_);
                    auto it = peers_.find(peer_key);
                    if (it != peers_.end()) {
                        calculateBackoffDuration(it->second);
                        
                        // Schedule next attempt
                        std::this_thread::sleep_for(it->second.backoff_duration);
                        
                        std::lock_guard<std::mutex> queue_lock(reconnection_mutex_);
                        reconnection_queue_.push(peer_key);
                    }
                }
            }
        }
    }
}

ConnectionResult PeerConnectionManager::attemptConnection(const std::string& address, uint16_t port) {
    ConnectionResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // Attempt TCP connection
        bool success = network_manager_->connectToPeer(address, port);
        result.connection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        if (success) {
            result.success = true;
            result.external_ip = external_ip_;
            result.external_port = external_port_;
        } else {
            result.success = false;
            result.error_message = "TCP connection failed";
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Connection exception: " + std::string(e.what());
        result.connection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
    }
    
    return result;
}

void PeerConnectionManager::updateConnectionMetrics(const std::string& peer_key, bool success, 
                                                   std::chrono::milliseconds connection_time) {
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.connection_attempts++;
        it->second.last_attempt = std::chrono::steady_clock::now();
        
        if (success) {
            it->second.successful_connections++;
            it->second.consecutive_failures = 0;
            it->second.avg_latency = connection_time;
        } else {
            it->second.failed_connections++;
            it->second.consecutive_failures++;
        }
    }
}

void PeerConnectionManager::calculateBackoffDuration(PeerConnectionInfo& peer_info) {
    // Exponential backoff with jitter
    uint32_t base_delay = std::min(static_cast<uint32_t>(peer_info.consecutive_failures * 2), 60U);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, base_delay);
    
    uint32_t jitter = dis(gen);
    peer_info.backoff_duration = std::chrono::seconds(base_delay + jitter);
    
    // Cap at maximum backoff duration
    if (peer_info.backoff_duration > max_backoff_duration_) {
        peer_info.backoff_duration = max_backoff_duration_;
    }
}

bool PeerConnectionManager::shouldAttemptReconnection(const PeerConnectionInfo& peer_info) const {
    // Don't reconnect if banned
    if (peer_info.state == ConnectionState::BANNED) {
        return false;
    }
    
    // Don't reconnect if already connected
    if (peer_info.state == ConnectionState::CONNECTED) {
        return false;
    }
    
    // Don't reconnect if too many failures
    if (peer_info.consecutive_failures >= max_connection_attempts_) {
        return false;
    }
    
    // Check if enough time has passed since last attempt
    auto now = std::chrono::steady_clock::now();
    auto time_since_attempt = now - peer_info.last_attempt;
    
    return time_since_attempt >= peer_info.backoff_duration;
}

std::string PeerConnectionManager::getPeerKey(const std::string& address, uint16_t port) const {
    return address + ":" + std::to_string(port);
}

// NAT traversal methods (simplified implementations)
bool PeerConnectionManager::performUPNPTraversal() {
    // Implement UPNP traversal
    DEO_LOG_DEBUG(NETWORKING, "UPNP traversal not implemented yet");
    return false;
}

bool PeerConnectionManager::performSTUNTraversal() {
    // Implement STUN traversal
    DEO_LOG_DEBUG(NETWORKING, "STUN traversal not implemented yet");
    return false;
}

bool PeerConnectionManager::performTURNTraversal() {
    // Implement TURN traversal
    DEO_LOG_DEBUG(NETWORKING, "TURN traversal not implemented yet");
    return false;
}

int32_t PeerConnectionManager::calculateReputationScoreInternal(const PeerConnectionInfo& peer_info) const {
    int32_t score = 100; // Base score
    
    // Factor in connection success rate
    if (peer_info.connection_attempts > 0) {
        double success_rate = static_cast<double>(peer_info.successful_connections) / peer_info.connection_attempts;
        score += static_cast<int32_t>(success_rate * 50);
    }
    
    // Factor in consecutive failures
    score -= peer_info.consecutive_failures * 10;
    
    // Factor in message activity
    score += std::min(static_cast<uint32_t>(peer_info.messages_received / 10), 50U);
    
    return std::max(-1000, std::min(1000, score));
}

void PeerConnectionManager::reportGoodBehavior(const std::string& address, uint16_t port, const std::string& behavior_type) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it != peers_.end()) {
        it->second.good_behavior_count++;
        it->second.last_activity = std::chrono::system_clock::now();
        it->second.behavior_history[behavior_type]++;
        
        // Award reputation points based on behavior type
        int32_t reward = 0;
        if (behavior_type == "valid_block") reward = 10;
        else if (behavior_type == "valid_transaction") reward = 5;
        else if (behavior_type == "helpful_response") reward = 3;
        else if (behavior_type == "connection_stable") reward = 2;
        else reward = 1;
        
        it->second.reputation_score += reward;
        it->second.reputation_score = std::max(-1000, std::min(1000, it->second.reputation_score));
        
        DEO_LOG_DEBUG(NETWORKING, "Good behavior reported for " + peer_key + 
                     " (" + behavior_type + "): +" + std::to_string(reward) + 
                     " reputation (total: " + std::to_string(it->second.reputation_score) + ")");
    }
}

void PeerConnectionManager::reportMisbehavior(const std::string& address, uint16_t port, const std::string& behavior_type, int32_t severity) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it != peers_.end()) {
        it->second.misbehavior_count++;
        it->second.last_misbehavior = std::chrono::system_clock::now();
        it->second.behavior_history[behavior_type]++;
        
        // Penalize reputation based on behavior type and severity
        int32_t penalty = 0;
        if (behavior_type == "invalid_block") penalty = 50 * severity;
        else if (behavior_type == "invalid_transaction") penalty = 20 * severity;
        else if (behavior_type == "spam") penalty = 15 * severity;
        else if (behavior_type == "connection_abuse") penalty = 10 * severity;
        else if (behavior_type == "timeout") penalty = 5 * severity;
        else penalty = 5 * severity;
        
        it->second.reputation_score -= penalty;
        it->second.reputation_score = std::max(-1000, std::min(1000, it->second.reputation_score));
        
        DEO_LOG_WARNING(NETWORKING, "Misbehavior reported for " + peer_key + 
                       " (" + behavior_type + ", severity " + std::to_string(severity) + "): -" + 
                       std::to_string(penalty) + " reputation (total: " + std::to_string(it->second.reputation_score) + ")");
        
        // Auto-ban if reputation drops too low
        if (it->second.reputation_score <= -500) {
            banPeer(address, port, "Reputation too low: " + std::to_string(it->second.reputation_score));
        }
    }
}

int32_t PeerConnectionManager::calculateReputationScore(const std::string& address, uint16_t port) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it != peers_.end()) {
        return calculateReputationScoreInternal(it->second);
    }
    
    return 0; // Unknown peer
}

std::chrono::seconds PeerConnectionManager::calculateBanDuration(const std::string& address, uint16_t port) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it != peers_.end()) {
        const auto& peer_info = it->second;
        
        // Base ban duration
        std::chrono::seconds base_duration(60); // 1 minute
        
        // Exponential backoff based on ban count
        if (peer_info.ban_count > 0) {
            base_duration = std::chrono::seconds(60 * (1 << std::min(peer_info.ban_count, 10U))); // Max ~17 hours
        }
        
        // Adjust based on reputation
        if (peer_info.reputation_score < -800) {
            base_duration *= 2; // Double for very bad reputation
        }
        
        // Cap at 24 hours
        return std::min(base_duration, std::chrono::seconds(24 * 60 * 60));
    }
    
    return std::chrono::seconds(60); // Default 1 minute
}

void PeerConnectionManager::applyReputationDecay() {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    auto now = std::chrono::system_clock::now();
    const auto decay_interval = std::chrono::hours(24); // Decay every 24 hours
    const int32_t decay_amount = 1; // Small positive decay
    
    for (auto& [peer_key, peer_info] : peers_) {
        // Only apply decay to peers with negative reputation
        if (peer_info.reputation_score < 0) {
            auto time_since_activity = now - peer_info.last_activity;
            
            // Apply decay if peer has been inactive for decay_interval
            if (time_since_activity >= decay_interval) {
                peer_info.reputation_score += decay_amount;
                peer_info.reputation_score = std::max(-1000, std::min(1000, peer_info.reputation_score));
                
                DEO_LOG_DEBUG(NETWORKING, "Applied reputation decay to " + peer_key + 
                             ": +" + std::to_string(decay_amount) + 
                             " (new score: " + std::to_string(peer_info.reputation_score) + ")");
            }
        }
        
        // Check for expired bans
        if (peer_info.state == ConnectionState::BANNED && now >= peer_info.ban_expiry) {
            banned_peers_.erase(peer_key);
            peer_info.state = ConnectionState::DISCONNECTED;
            peer_info.reputation_score = std::max(peer_info.reputation_score, -100); // Partial recovery
            
            DEO_LOG_INFO(NETWORKING, "Ban expired for peer: " + peer_key + 
                        " (reputation: " + std::to_string(peer_info.reputation_score) + ")");
        }
    }
}

void PeerConnectionManager::startReputationDecayLoop() {
    if (reputation_decay_running_) {
        return;
    }
    
    reputation_decay_running_ = true;
    reputation_decay_thread_ = std::thread(&PeerConnectionManager::reputationDecayLoop, this);
    
    DEO_LOG_INFO(NETWORKING, "Started reputation decay loop");
}

void PeerConnectionManager::stopReputationDecayLoop() {
    if (!reputation_decay_running_) {
        return;
    }
    
    reputation_decay_running_ = false;
    reputation_decay_cv_.notify_all();
    
    if (reputation_decay_thread_.joinable()) {
        reputation_decay_thread_.join();
    }
    
    DEO_LOG_INFO(NETWORKING, "Stopped reputation decay loop");
}

void PeerConnectionManager::reputationDecayLoop() {
    while (reputation_decay_running_) {
        std::unique_lock<std::mutex> lock(reputation_decay_mutex_);
        
        // Wait for 1 hour or until shutdown
        if (reputation_decay_cv_.wait_for(lock, std::chrono::hours(1), 
            [this] { return !reputation_decay_running_; })) {
            break; // Shutdown requested
        }
        
        lock.unlock();
        
        // Apply reputation decay
        applyReputationDecay();
    }
}

std::vector<std::string> PeerConnectionManager::getBannedPeers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::vector<std::string> banned_peers;
    for (const auto& peer_key : banned_peers_) {
        banned_peers.push_back(peer_key);
    }
    
    return banned_peers;
}

std::vector<std::string> PeerConnectionManager::getLowReputationPeers(int32_t threshold) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::vector<std::string> low_reputation_peers;
    for (const auto& [peer_key, peer_info] : peers_) {
        if (peer_info.reputation_score <= threshold) {
            low_reputation_peers.push_back(peer_key);
        }
    }
    
    return low_reputation_peers;
}

std::string PeerConnectionManager::getReputationReport(const std::string& address, uint16_t port) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    std::string peer_key = getPeerKey(address, port);
    auto it = peers_.find(peer_key);
    
    if (it == peers_.end()) {
        return "Peer not found: " + peer_key;
    }
    
    const auto& peer_info = it->second;
    std::ostringstream report;
    
    report << "Reputation Report for " << peer_key << ":\n";
    report << "  Current Score: " << peer_info.reputation_score << "\n";
    report << "  Good Behaviors: " << peer_info.good_behavior_count << "\n";
    report << "  Misbehaviors: " << peer_info.misbehavior_count << "\n";
    report << "  Ban Count: " << peer_info.ban_count << "\n";
    report << "  Last Activity: " << std::chrono::duration_cast<std::chrono::seconds>(
        peer_info.last_activity.time_since_epoch()).count() << "s ago\n";
    
    if (peer_info.state == ConnectionState::BANNED) {
        report << "  Status: BANNED\n";
        report << "  Ban Reason: " << peer_info.ban_reason << "\n";
        report << "  Ban Expires: " << std::chrono::duration_cast<std::chrono::seconds>(
            peer_info.ban_expiry.time_since_epoch()).count() << "s\n";
    } else {
        report << "  Status: " << static_cast<int>(peer_info.state) << "\n";
    }
    
    report << "  Behavior History:\n";
    for (const auto& [behavior, count] : peer_info.behavior_history) {
        report << "    " << behavior << ": " << count << "\n";
    }
    
    return report.str();
}

void PeerConnectionManager::handleConnectionSuccess(const std::string& peer_key) {
    DEO_LOG_DEBUG(NETWORKING, "Connection success for: " + peer_key);
}

void PeerConnectionManager::handleConnectionFailure(const std::string& peer_key, const std::string& reason) {
    DEO_LOG_DEBUG(NETWORKING, "Connection failure for " + peer_key + ": " + reason);
}

void PeerConnectionManager::handlePeerMessage(const std::string& peer_key, const std::string& message_type) {
    DEO_LOG_DEBUG(NETWORKING, "Message from " + peer_key + ": " + message_type);
}

// Authentication methods

bool PeerConnectionManager::enableAuthentication(const AuthConfig& auth_config) {
    if (!is_initialized_) {
        DEO_LOG_ERROR(NETWORKING, "Peer Connection Manager not initialized");
        return false;
    }
    
    if (!node_keypair_) {
        DEO_LOG_ERROR(NETWORKING, "Node keypair required for authentication");
        return false;
    }
    
    auth_manager_ = std::make_unique<PeerAuthentication>();
    if (!auth_manager_->initialize(auth_config, std::shared_ptr<crypto::KeyPair>(node_keypair_.get(), [](crypto::KeyPair*){}))) {
        DEO_LOG_ERROR(NETWORKING, "Failed to initialize authentication manager");
        auth_manager_.reset();
        return false;
    }
    
    authentication_enabled_ = true;
    DEO_LOG_INFO(NETWORKING, "Peer authentication enabled");
    return true;
}

void PeerConnectionManager::disableAuthentication() {
    if (auth_manager_) {
        auth_manager_->shutdown();
        auth_manager_.reset();
    }
    authentication_enabled_ = false;
    DEO_LOG_INFO(NETWORKING, "Peer authentication disabled");
}

bool PeerConnectionManager::isAuthenticationEnabled() const {
    return authentication_enabled_ && auth_manager_ != nullptr;
}

std::string PeerConnectionManager::generateAuthChallenge(const std::string& address, uint16_t port) {
    if (!isAuthenticationEnabled()) {
        DEO_LOG_ERROR(NETWORKING, "Authentication not enabled");
        return "";
    }
    
    return auth_manager_->generateChallenge(address, port);
}

bool PeerConnectionManager::verifyAuthResponse(const AuthResponse& response) {
    if (!isAuthenticationEnabled()) {
        DEO_LOG_ERROR(NETWORKING, "Authentication not enabled");
        return false;
    }
    
    return auth_manager_->verifyResponse(response);
}

bool PeerConnectionManager::isPeerAuthenticated(const std::string& address, uint16_t port) const {
    if (!isAuthenticationEnabled()) {
        return true; // If authentication is disabled, all peers are considered authenticated
    }
    
    return auth_manager_->isPeerAuthenticated(address, port);
}

bool PeerConnectionManager::isPeerTrusted(const std::string& address, uint16_t port) const {
    if (!isAuthenticationEnabled()) {
        return false; // If authentication is disabled, no peers are trusted
    }
    
    return auth_manager_->isPeerTrusted(address, port);
}

std::shared_ptr<PeerIdentity> PeerConnectionManager::getPeerIdentity(const std::string& address, uint16_t port) const {
    if (!isAuthenticationEnabled()) {
        return nullptr;
    }
    
    return auth_manager_->getPeerIdentity(address, port);
}

void PeerConnectionManager::addTrustedPeer(const std::string& public_key, const std::string& node_id) {
    if (!isAuthenticationEnabled()) {
        DEO_LOG_ERROR(NETWORKING, "Authentication not enabled");
        return;
    }
    
    auth_manager_->addTrustedPeer(public_key, node_id);
}

void PeerConnectionManager::removeTrustedPeer(const std::string& public_key) {
    if (!isAuthenticationEnabled()) {
        DEO_LOG_ERROR(NETWORKING, "Authentication not enabled");
        return;
    }
    
    auth_manager_->removeTrustedPeer(public_key);
}

void PeerConnectionManager::addBlacklistedPeer(const std::string& public_key, const std::string& reason) {
    if (!isAuthenticationEnabled()) {
        DEO_LOG_ERROR(NETWORKING, "Authentication not enabled");
        return;
    }
    
    auth_manager_->addBlacklistedPeer(public_key, reason);
}

void PeerConnectionManager::removeBlacklistedPeer(const std::string& public_key) {
    if (!isAuthenticationEnabled()) {
        DEO_LOG_ERROR(NETWORKING, "Authentication not enabled");
        return;
    }
    
    auth_manager_->removeBlacklistedPeer(public_key);
}

std::string PeerConnectionManager::signData(const std::string& data) const {
    if (!isAuthenticationEnabled()) {
        DEO_LOG_ERROR(NETWORKING, "Authentication not enabled");
        return "";
    }
    
    return auth_manager_->signData(data);
}

bool PeerConnectionManager::verifyPeerSignature(const std::string& data, const std::string& signature, 
                                               const std::string& address, uint16_t port) const {
    if (!isAuthenticationEnabled()) {
        return true; // If authentication is disabled, all signatures are considered valid
    }
    
    return auth_manager_->verifySignature(data, signature, address, port);
}

std::vector<PeerIdentity> PeerConnectionManager::getAuthenticatedPeers() const {
    if (!isAuthenticationEnabled()) {
        return {};
    }
    
    return auth_manager_->getAuthenticatedPeers();
}

std::vector<PeerIdentity> PeerConnectionManager::getTrustedPeers() const {
    if (!isAuthenticationEnabled()) {
        return {};
    }
    
    return auth_manager_->getTrustedPeers();
}

std::string PeerConnectionManager::getAuthenticationStatistics() const {
    if (!isAuthenticationEnabled()) {
        return "{\"authentication_enabled\": false}";
    }
    
    return auth_manager_->getStatistics();
}

} // namespace network
} // namespace deo
