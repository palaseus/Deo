/**
 * @file peer_authentication.cpp
 * @brief Cryptographic peer authentication system implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "network/peer_authentication.h"
#include "utils/error_handler.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace deo {
namespace network {

PeerAuthentication::PeerAuthentication()
    : is_initialized_(false)
    , rng_(rd_()) {
    
    DEO_LOG_DEBUG(NETWORKING, "Peer Authentication created");
}

PeerAuthentication::~PeerAuthentication() {
    shutdown();
    DEO_LOG_DEBUG(NETWORKING, "Peer Authentication destroyed");
}

bool PeerAuthentication::initialize(const AuthConfig& config, std::shared_ptr<crypto::KeyPair> node_keypair) {
    std::lock_guard<std::mutex> lock(challenges_mutex_);
    
    if (is_initialized_) {
        DEO_LOG_WARNING(NETWORKING, "Peer Authentication already initialized");
        return true;
    }
    
    if (!node_keypair) {
        DEO_LOG_ERROR(NETWORKING, "Node keypair is required for authentication");
        return false;
    }
    
    try {
        config_ = config;
        node_keypair_ = node_keypair;
        node_id_ = calculateNodeId(node_keypair_->getPublicKey());
        
        // Initialize trust management
        trusted_public_keys_ = config_.trusted_public_keys;
        blacklisted_public_keys_ = config_.blacklisted_public_keys;
        
        is_initialized_ = true;
        
        DEO_LOG_INFO(NETWORKING, "Peer Authentication initialized successfully");
        DEO_LOG_INFO(NETWORKING, "Node ID: " + node_id_);
        DEO_LOG_INFO(NETWORKING, "Public Key: " + node_keypair_->getPublicKey());
        
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to initialize Peer Authentication: " + std::string(e.what()));
        return false;
    }
}

void PeerAuthentication::shutdown() {
    if (!is_initialized_) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Shutting down Peer Authentication");
    
    {
        std::lock_guard<std::mutex> lock(challenges_mutex_);
        active_challenges_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(identities_mutex_);
        peer_identities_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(trust_mutex_);
        trusted_public_keys_.clear();
        blacklisted_public_keys_.clear();
    }
    
    is_initialized_ = false;
    DEO_LOG_INFO(NETWORKING, "Peer Authentication shutdown complete");
}

std::string PeerAuthentication::generateChallenge(const std::string& peer_address, uint16_t peer_port) {
    if (!is_initialized_) {
        DEO_LOG_ERROR(NETWORKING, "Peer Authentication not initialized");
        return "";
    }
    
    std::lock_guard<std::mutex> lock(challenges_mutex_);
    
    std::string peer_key = getPeerKey(peer_address, peer_port);
    
    // Check if peer is blacklisted
    {
        std::lock_guard<std::mutex> trust_lock(trust_mutex_);
        // We can't check blacklist here since we don't have the peer's public key yet
        // This will be checked during response verification
    }
    
    // Generate new challenge
    AuthChallenge challenge;
    challenge.challenge_id = generateChallengeId();
    challenge.challenge_data = generateChallengeData();
    challenge.timestamp = std::chrono::system_clock::now();
    challenge.peer_address = peer_address;
    challenge.peer_port = peer_port;
    challenge.is_verified = false;
    
    active_challenges_[challenge.challenge_id] = challenge;
    
    DEO_LOG_DEBUG(NETWORKING, "Generated authentication challenge for " + peer_key + 
                 " (ID: " + challenge.challenge_id + ")");
    
    return challenge.challenge_id;
}

bool PeerAuthentication::verifyResponse(const AuthResponse& response) {
    if (!is_initialized_) {
        DEO_LOG_ERROR(NETWORKING, "Peer Authentication not initialized");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(challenges_mutex_);
    
    // Find the challenge
    auto challenge_it = active_challenges_.find(response.challenge_id);
    if (challenge_it == active_challenges_.end()) {
        DEO_LOG_WARNING(NETWORKING, "Unknown challenge ID: " + response.challenge_id);
        return false;
    }
    
    AuthChallenge& challenge = challenge_it->second;
    
    // Check if challenge is still valid
    if (!isChallengeValid(challenge)) {
        DEO_LOG_WARNING(NETWORKING, "Challenge expired: " + response.challenge_id);
        active_challenges_.erase(challenge_it);
        return false;
    }
    
    // Check if peer is blacklisted
    {
        std::lock_guard<std::mutex> trust_lock(trust_mutex_);
        if (blacklisted_public_keys_.find(response.public_key) != blacklisted_public_keys_.end()) {
            DEO_LOG_WARNING(NETWORKING, "Blacklisted peer attempted authentication: " + response.public_key);
            active_challenges_.erase(challenge_it);
            return false;
        }
    }
    
    // Verify signature
    if (!crypto::Signature::verify(challenge.challenge_data, response.signature, response.public_key)) {
        DEO_LOG_WARNING(NETWORKING, "Invalid signature in authentication response");
        active_challenges_.erase(challenge_it);
        return false;
    }
    
    // Verify node ID matches public key
    std::string calculated_node_id = calculateNodeId(response.public_key);
    if (calculated_node_id != response.node_id) {
        DEO_LOG_WARNING(NETWORKING, "Node ID mismatch in authentication response");
        active_challenges_.erase(challenge_it);
        return false;
    }
    
    // Check if trusted peer is required
    if (config_.require_trusted_peers) {
        std::lock_guard<std::mutex> trust_lock(trust_mutex_);
        if (trusted_public_keys_.find(response.public_key) == trusted_public_keys_.end()) {
            DEO_LOG_WARNING(NETWORKING, "Untrusted peer attempted authentication: " + response.public_key);
            active_challenges_.erase(challenge_it);
            return false;
        }
    }
    
    // Authentication successful - create peer identity
    std::string peer_key = getPeerKey(challenge.peer_address, challenge.peer_port);
    bool is_trusted = false;
    
    {
        std::lock_guard<std::mutex> identity_lock(identities_mutex_);
        auto identity = std::make_shared<PeerIdentity>();
        identity->node_id = response.node_id;
        identity->public_key = response.public_key;
        identity->address = challenge.peer_address;
        identity->port = challenge.peer_port;
        identity->first_seen = std::chrono::system_clock::now();
        identity->last_seen = std::chrono::system_clock::now();
        identity->is_authenticated = true;
        
        // Check if peer is trusted
        {
            std::lock_guard<std::mutex> trust_lock(trust_mutex_);
            identity->is_trusted = (trusted_public_keys_.find(response.public_key) != trusted_public_keys_.end());
            is_trusted = identity->is_trusted;
        }
        
        peer_identities_[peer_key] = identity;
    }
    
    // Mark challenge as verified and remove it
    challenge.is_verified = true;
    active_challenges_.erase(challenge_it);
    
    DEO_LOG_INFO(NETWORKING, "Peer authentication successful: " + peer_key + 
                 " (Node ID: " + response.node_id + ", Trusted: " + 
                 (is_trusted ? "Yes" : "No") + ")");
    
    return true;
}

bool PeerAuthentication::isPeerAuthenticated(const std::string& peer_address, uint16_t peer_port) const {
    if (!is_initialized_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(identities_mutex_);
    
    std::string peer_key = getPeerKey(peer_address, peer_port);
    auto it = peer_identities_.find(peer_key);
    
    if (it == peer_identities_.end()) {
        return false;
    }
    
    // Check if session is still valid
    return it->second->is_authenticated && isSessionValid(*it->second);
}

bool PeerAuthentication::isPeerTrusted(const std::string& peer_address, uint16_t peer_port) const {
    if (!is_initialized_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(identities_mutex_);
    
    std::string peer_key = getPeerKey(peer_address, peer_port);
    auto it = peer_identities_.find(peer_key);
    
    if (it == peer_identities_.end()) {
        return false;
    }
    
    return it->second->is_trusted && isSessionValid(*it->second);
}

std::shared_ptr<PeerIdentity> PeerAuthentication::getPeerIdentity(const std::string& peer_address, uint16_t peer_port) const {
    if (!is_initialized_) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(identities_mutex_);
    
    std::string peer_key = getPeerKey(peer_address, peer_port);
    auto it = peer_identities_.find(peer_key);
    
    if (it == peer_identities_.end()) {
        return nullptr;
    }
    
    return it->second;
}

void PeerAuthentication::addTrustedPeer(const std::string& public_key, const std::string& node_id) {
    if (!is_initialized_) {
        DEO_LOG_ERROR(NETWORKING, "Peer Authentication not initialized");
        return;
    }
    
    std::lock_guard<std::mutex> lock(trust_mutex_);
    
    trusted_public_keys_.insert(public_key);
    
    DEO_LOG_INFO(NETWORKING, "Added trusted peer: " + public_key + 
                 (node_id.empty() ? "" : " (Node ID: " + node_id + ")"));
}

void PeerAuthentication::removeTrustedPeer(const std::string& public_key) {
    if (!is_initialized_) {
        DEO_LOG_ERROR(NETWORKING, "Peer Authentication not initialized");
        return;
    }
    
    std::lock_guard<std::mutex> lock(trust_mutex_);
    
    trusted_public_keys_.erase(public_key);
    
    DEO_LOG_INFO(NETWORKING, "Removed trusted peer: " + public_key);
}

void PeerAuthentication::addBlacklistedPeer(const std::string& public_key, const std::string& reason) {
    if (!is_initialized_) {
        DEO_LOG_ERROR(NETWORKING, "Peer Authentication not initialized");
        return;
    }
    
    std::lock_guard<std::mutex> lock(trust_mutex_);
    
    blacklisted_public_keys_.insert(public_key);
    
    DEO_LOG_WARNING(NETWORKING, "Added blacklisted peer: " + public_key + 
                   (reason.empty() ? "" : " (Reason: " + reason + ")"));
}

void PeerAuthentication::removeBlacklistedPeer(const std::string& public_key) {
    if (!is_initialized_) {
        DEO_LOG_ERROR(NETWORKING, "Peer Authentication not initialized");
        return;
    }
    
    std::lock_guard<std::mutex> lock(trust_mutex_);
    
    blacklisted_public_keys_.erase(public_key);
    
    DEO_LOG_INFO(NETWORKING, "Removed blacklisted peer: " + public_key);
}

std::string PeerAuthentication::signData(const std::string& data) const {
    if (!is_initialized_ || !node_keypair_) {
        DEO_LOG_ERROR(NETWORKING, "Peer Authentication not initialized or no keypair");
        return "";
    }
    
    return crypto::Signature::sign(data, node_keypair_->getPrivateKey());
}

bool PeerAuthentication::verifySignature(const std::string& data, const std::string& signature, 
                                        const std::string& peer_address, uint16_t peer_port) const {
    if (!is_initialized_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(identities_mutex_);
    
    std::string peer_key = getPeerKey(peer_address, peer_port);
    auto it = peer_identities_.find(peer_key);
    
    if (it == peer_identities_.end()) {
        return false;
    }
    
    return crypto::Signature::verify(data, signature, it->second->public_key);
}

std::string PeerAuthentication::getNodePublicKey() const {
    if (!is_initialized_ || !node_keypair_) {
        return "";
    }
    
    return node_keypair_->getPublicKey();
}

std::string PeerAuthentication::getNodeId() const {
    if (!is_initialized_) {
        return "";
    }
    
    return node_id_;
}

void PeerAuthentication::updatePeerActivity(const std::string& peer_address, uint16_t peer_port) {
    if (!is_initialized_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(identities_mutex_);
    
    std::string peer_key = getPeerKey(peer_address, peer_port);
    auto it = peer_identities_.find(peer_key);
    
    if (it != peer_identities_.end()) {
        it->second->last_seen = std::chrono::system_clock::now();
    }
}

void PeerAuthentication::cleanupExpired() {
    if (!is_initialized_) {
        return;
    }
    
    removeExpiredChallenges();
    removeExpiredSessions();
}

std::string PeerAuthentication::getStatistics() const {
    if (!is_initialized_) {
        return "{}";
    }
    
    std::lock_guard<std::mutex> challenge_lock(challenges_mutex_);
    std::lock_guard<std::mutex> identity_lock(identities_mutex_);
    std::lock_guard<std::mutex> trust_lock(trust_mutex_);
    
    nlohmann::json stats;
    stats["node_id"] = node_id_;
    stats["public_key"] = node_keypair_ ? node_keypair_->getPublicKey() : "";
    stats["active_challenges"] = active_challenges_.size();
    stats["authenticated_peers"] = peer_identities_.size();
    stats["trusted_peers"] = trusted_public_keys_.size();
    stats["blacklisted_peers"] = blacklisted_public_keys_.size();
    stats["require_authentication"] = config_.require_authentication;
    stats["require_trusted_peers"] = config_.require_trusted_peers;
    
    return stats.dump();
}

std::vector<PeerIdentity> PeerAuthentication::getAuthenticatedPeers() const {
    std::vector<PeerIdentity> peers;
    
    if (!is_initialized_) {
        return peers;
    }
    
    std::lock_guard<std::mutex> lock(identities_mutex_);
    
    for (const auto& [peer_key, identity] : peer_identities_) {
        if (identity->is_authenticated && isSessionValid(*identity)) {
            peers.push_back(*identity);
        }
    }
    
    return peers;
}

std::vector<PeerIdentity> PeerAuthentication::getTrustedPeers() const {
    std::vector<PeerIdentity> peers;
    
    if (!is_initialized_) {
        return peers;
    }
    
    std::lock_guard<std::mutex> lock(identities_mutex_);
    
    for (const auto& [peer_key, identity] : peer_identities_) {
        if (identity->is_trusted && isSessionValid(*identity)) {
            peers.push_back(*identity);
        }
    }
    
    return peers;
}

// Private methods

std::string PeerAuthentication::generateChallengeId() {
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t random_value = dist(rng_);
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << random_value;
    return oss.str();
}

std::string PeerAuthentication::generateChallengeData() {
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t random_value = dist(rng_);
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << random_value;
    oss << "_" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return oss.str();
}

std::string PeerAuthentication::getPeerKey(const std::string& address, uint16_t port) const {
    return address + ":" + std::to_string(port);
}

bool PeerAuthentication::isChallengeValid(const AuthChallenge& challenge) const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - challenge.timestamp;
    return elapsed < config_.challenge_timeout;
}

bool PeerAuthentication::isSessionValid(const PeerIdentity& identity) const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = now - identity.last_seen;
    return elapsed < config_.session_timeout;
}

void PeerAuthentication::removeExpiredChallenges() {
    std::lock_guard<std::mutex> lock(challenges_mutex_);
    
    auto it = active_challenges_.begin();
    while (it != active_challenges_.end()) {
        if (!isChallengeValid(it->second)) {
            DEO_LOG_DEBUG(NETWORKING, "Removing expired challenge: " + it->first);
            it = active_challenges_.erase(it);
        } else {
            ++it;
        }
    }
}

void PeerAuthentication::removeExpiredSessions() {
    std::lock_guard<std::mutex> lock(identities_mutex_);
    
    auto it = peer_identities_.begin();
    while (it != peer_identities_.end()) {
        if (!isSessionValid(*it->second)) {
            DEO_LOG_DEBUG(NETWORKING, "Removing expired session: " + it->first);
            it = peer_identities_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string PeerAuthentication::calculateNodeId(const std::string& public_key) const {
    return crypto::Hash::sha256(public_key).substr(0, 16); // Use first 16 characters of hash
}

} // namespace network
} // namespace deo
