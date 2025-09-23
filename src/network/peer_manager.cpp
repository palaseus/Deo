/**
 * @file peer_manager.cpp
 * @brief Peer management with discovery, scoring, and banning
 */

#include "network/peer_manager.h"
#include <algorithm>
#include <random>
#include <sstream>

namespace deo {
namespace network {

PeerManager::PeerManager() : running_(false), rng_(std::random_device{}()) {
}

PeerManager::~PeerManager() {
    shutdown();
}

bool PeerManager::initialize() {
    DEO_LOG_INFO(NETWORKING, "Initializing peer manager");
    
    running_ = true;
    cleanup_thread_ = std::thread(&PeerManager::cleanupLoop, this);
    
    DEO_LOG_INFO(NETWORKING, "Peer manager initialized successfully");
    return true;
}

void PeerManager::shutdown() {
    if (!running_) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Shutting down peer manager");
    running_ = false;
    
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    
    DEO_LOG_INFO(NETWORKING, "Peer manager shutdown complete");
}

void PeerManager::addBootstrapNode(const std::string& address, uint16_t port) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    bootstrap_nodes_.push_back({address, port});
    DEO_LOG_INFO(NETWORKING, "Added bootstrap node: " + address + ":" + std::to_string(port));
}

void PeerManager::discoverPeers() {
    DEO_LOG_INFO(NETWORKING, "Starting peer discovery");
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (const auto& [address, port] : bootstrap_nodes_) {
        addPeer(address, port);
    }
}

void PeerManager::requestPeerList(const std::string& peer_address) {
    DEO_LOG_DEBUG(NETWORKING, "Requesting peer list from " + peer_address);
}

bool PeerManager::addPeer(const std::string& address, uint16_t port) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.last_seen = std::chrono::steady_clock::now();
        it->second.connected = true;
        return true;
    }
    
    PeerInfo peer_info;
    peer_info.address = address;
    peer_info.port = port;
    peer_info.connected = true;
    peer_info.first_seen = std::chrono::steady_clock::now();
    peer_info.last_seen = std::chrono::steady_clock::now();
    peer_info.last_activity = std::chrono::steady_clock::now();
    
    peers_[peer_key] = peer_info;
    
    DEO_LOG_INFO(NETWORKING, "Added new peer: " + peer_key);
    return true;
}

void PeerManager::removePeer(const std::string& address, uint16_t port) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.connected = false;
        DEO_LOG_INFO(NETWORKING, "Removed peer: " + peer_key);
    }
}

void PeerManager::updatePeerActivity(const std::string& address, uint16_t port) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.last_activity = std::chrono::steady_clock::now();
        it->second.messages_received++;
    }
}

void PeerManager::updatePeerHeight(const std::string& address, uint16_t port, uint64_t height) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.best_height = height;
        DEO_LOG_DEBUG(NETWORKING, "Updated peer " + peer_key + " height to " + std::to_string(height));
    }
}

void PeerManager::reportGoodBehavior(const std::string& address, uint16_t port, int32_t score) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.good_behavior_score += score;
        it->second.misbehavior_score = std::max(0, it->second.misbehavior_score - score);
        DEO_LOG_DEBUG(NETWORKING, "Reported good behavior for peer " + peer_key + " (+" + std::to_string(score) + ")");
    }
}

void PeerManager::reportMisbehavior(const std::string& address, uint16_t port, int32_t score) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.misbehavior_score += score;
        it->second.invalid_messages++;
        
        DEO_LOG_WARNING(NETWORKING, "Reported misbehavior for peer " + peer_key + " (+" + std::to_string(score) + ")");
        
        if (it->second.misbehavior_score >= 100) {
            banPeer(address, port, std::chrono::hours(24));
        }
    }
}

void PeerManager::banPeer(const std::string& address, uint16_t port, std::chrono::seconds duration) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.banned = true;
        it->second.ban_until = std::chrono::steady_clock::now() + duration;
        it->second.connected = false;
        
        DEO_LOG_WARNING(NETWORKING, "Banned peer " + peer_key + " for " + std::to_string(duration.count()) + " seconds");
    }
}

void PeerManager::unbanPeer(const std::string& address, uint16_t port) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second.banned = false;
        it->second.ban_until = std::chrono::steady_clock::time_point();
        
        DEO_LOG_INFO(NETWORKING, "Unbanned peer " + peer_key);
    }
}

std::vector<std::string> PeerManager::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::vector<std::string> connected_peers;
    
    for (const auto& [peer_key, peer_info] : peers_) {
        if (peer_info.connected && !peer_info.banned) {
            connected_peers.push_back(peer_key);
        }
    }
    
    return connected_peers;
}

std::vector<std::string> PeerManager::getBannedPeers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::vector<std::string> banned_peers;
    
    for (const auto& [peer_key, peer_info] : peers_) {
        if (peer_info.banned) {
            banned_peers.push_back(peer_key);
        }
    }
    
    return banned_peers;
}

std::vector<std::string> PeerManager::getBestPeers(size_t count) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::vector<std::pair<std::string, PeerInfo>> peer_list;
    
    for (const auto& [peer_key, peer_info] : peers_) {
        if (peer_info.connected && !peer_info.banned) {
            peer_list.push_back({peer_key, peer_info});
        }
    }
    
    std::sort(peer_list.begin(), peer_list.end(), 
        [](const auto& a, const auto& b) {
            if (a.second.good_behavior_score != b.second.good_behavior_score) {
                return a.second.good_behavior_score > b.second.good_behavior_score;
            }
            return a.second.misbehavior_score < b.second.misbehavior_score;
        });
    
    std::vector<std::string> best_peers;
    for (size_t i = 0; i < std::min(count, peer_list.size()); ++i) {
        best_peers.push_back(peer_list[i].first);
    }
    
    return best_peers;
}

PeerInfo PeerManager::getPeerInfo(const std::string& address, uint16_t port) const {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        return it->second;
    }
    
    return PeerInfo();
}

bool PeerManager::isPeerBanned(const std::string& address, uint16_t port) const {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        if (it->second.banned) {
            if (std::chrono::steady_clock::now() > it->second.ban_until) {
                return false;
            }
            return true;
        }
    }
    
    return false;
}

bool PeerManager::isPeerConnected(const std::string& address, uint16_t port) const {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        return it->second.connected && !it->second.banned;
    }
    
    return false;
}

PeerManager::PeerStats PeerManager::getPeerStats() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    PeerStats stats;
    stats.total_peers = peers_.size();
    stats.bootstrap_nodes = bootstrap_nodes_.size();
    
    int32_t total_misbehavior = 0;
    int32_t total_good_behavior = 0;
    
    for (const auto& [peer_key, peer_info] : peers_) {
        if (peer_info.connected) {
            stats.connected_peers++;
        }
        if (peer_info.banned) {
            stats.banned_peers++;
        }
        
        total_misbehavior += peer_info.misbehavior_score;
        total_good_behavior += peer_info.good_behavior_score;
    }
    
    if (stats.total_peers > 0) {
        stats.average_misbehavior_score = static_cast<double>(total_misbehavior) / stats.total_peers;
        stats.average_good_behavior_score = static_cast<double>(total_good_behavior) / stats.total_peers;
    }
    
    return stats;
}

bool PeerManager::checkRateLimit(const std::string& address, uint16_t port, const std::string& message_type) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    
    auto peer_it = rate_limits_.find(peer_key);
    if (peer_it == rate_limits_.end()) {
        return false;
    }
    
    auto message_it = peer_it->second.find(message_type);
    if (message_it == peer_it->second.end()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto& entry = message_it->second;
    
    if (now - entry.last_request > entry.window_duration) {
        entry.request_count = 0;
        entry.last_request = now;
        return false;
    }
    
    return entry.request_count >= entry.max_requests;
}

void PeerManager::recordMessage(const std::string& address, uint16_t port, const std::string& message_type) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    
    auto& entry = rate_limits_[peer_key][message_type];
    entry.request_count++;
    entry.last_request = std::chrono::steady_clock::now();
    
    if (entry.window_duration.count() == 0) {
        entry.window_duration = std::chrono::seconds(60);
        entry.max_requests = 100;
    }
}

void PeerManager::cleanupStalePeers() {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto stale_threshold = std::chrono::hours(24);
    
    auto it = peers_.begin();
    while (it != peers_.end()) {
        if (now - it->second.last_seen > stale_threshold) {
            DEO_LOG_INFO(NETWORKING, "Removing stale peer: " + it->first);
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }
}

void PeerManager::cleanupExpiredBans() {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto& [peer_key, peer_info] : peers_) {
        if (peer_info.banned && now > peer_info.ban_until) {
            peer_info.banned = false;
            peer_info.ban_until = std::chrono::steady_clock::time_point();
            DEO_LOG_INFO(NETWORKING, "Ban expired for peer: " + peer_key);
        }
    }
}

std::string PeerManager::formatPeerKey(const std::string& address, uint16_t port) const {
    return address + ":" + std::to_string(port);
}

void PeerManager::cleanupLoop() {
    DEO_LOG_INFO(NETWORKING, "Peer cleanup thread started");
    
    while (running_) {
        cleanupStalePeers();
        cleanupExpiredBans();
        
        std::this_thread::sleep_for(std::chrono::minutes(10));
    }
    
    DEO_LOG_INFO(NETWORKING, "Peer cleanup thread stopped");
}

} // namespace network
} // namespace deo