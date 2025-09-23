/**
 * @file peer_authentication.h
 * @brief Cryptographic peer authentication system for P2P network security
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
#include <memory>
#include <random>

#include "crypto/key_pair.h"
#include "crypto/signature.h"
#include "crypto/hash.h"
#include "utils/logger.h"

namespace deo {
namespace network {

/**
 * @brief Authentication challenge for peer verification
 */
struct AuthChallenge {
    std::string challenge_id;
    std::string challenge_data;
    std::chrono::system_clock::time_point timestamp;
    std::string peer_address;
    uint16_t peer_port;
    bool is_verified;
    
    AuthChallenge() 
        : challenge_id("")
        , challenge_data("")
        , timestamp(std::chrono::system_clock::now())
        , peer_address("")
        , peer_port(0)
        , is_verified(false) {}
};

/**
 * @brief Authentication response from peer
 */
struct AuthResponse {
    std::string challenge_id;
    std::string signature;
    std::string public_key;
    std::string node_id;
    std::chrono::system_clock::time_point timestamp;
    
    AuthResponse() 
        : challenge_id("")
        , signature("")
        , public_key("")
        , node_id("")
        , timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Peer identity information
 */
struct PeerIdentity {
    std::string node_id;
    std::string public_key;
    std::string address;
    uint16_t port;
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    bool is_authenticated;
    bool is_trusted;
    std::vector<std::string> capabilities;
    
    PeerIdentity() 
        : node_id("")
        , public_key("")
        , address("")
        , port(0)
        , first_seen(std::chrono::system_clock::now())
        , last_seen(std::chrono::system_clock::now())
        , is_authenticated(false)
        , is_trusted(false) {}
};

/**
 * @brief Authentication configuration
 */
struct AuthConfig {
    bool require_authentication;
    bool require_trusted_peers;
    std::chrono::seconds challenge_timeout;
    std::chrono::seconds session_timeout;
    uint32_t max_failed_attempts;
    std::set<std::string> trusted_public_keys;
    std::set<std::string> blacklisted_public_keys;
    
    AuthConfig() 
        : require_authentication(true)
        , require_trusted_peers(false)
        , challenge_timeout(std::chrono::seconds(30))
        , session_timeout(std::chrono::hours(24))
        , max_failed_attempts(3) {}
};

/**
 * @brief Peer authentication manager
 * 
 * Handles cryptographic authentication of peers in the P2P network,
 * including challenge-response authentication, identity verification,
 * and trust management.
 */
class PeerAuthentication {
public:
    /**
     * @brief Constructor
     */
    PeerAuthentication();
    
    /**
     * @brief Destructor
     */
    ~PeerAuthentication();
    
    // Disable copy and move semantics
    PeerAuthentication(const PeerAuthentication&) = delete;
    PeerAuthentication& operator=(const PeerAuthentication&) = delete;
    PeerAuthentication(PeerAuthentication&&) = delete;
    PeerAuthentication& operator=(PeerAuthentication&&) = delete;
    
    /**
     * @brief Initialize authentication system
     * @param config Authentication configuration
     * @param node_keypair Node's key pair for authentication
     * @return True if initialization successful
     */
    bool initialize(const AuthConfig& config, std::shared_ptr<crypto::KeyPair> node_keypair);
    
    /**
     * @brief Shutdown authentication system
     */
    void shutdown();
    
    /**
     * @brief Generate authentication challenge for peer
     * @param peer_address Peer's IP address
     * @param peer_port Peer's port
     * @return Challenge ID if successful, empty string if failed
     */
    std::string generateChallenge(const std::string& peer_address, uint16_t peer_port);
    
    /**
     * @brief Verify authentication response from peer
     * @param response Authentication response
     * @return True if authentication successful
     */
    bool verifyResponse(const AuthResponse& response);
    
    /**
     * @brief Check if peer is authenticated
     * @param peer_address Peer's IP address
     * @param peer_port Peer's port
     * @return True if peer is authenticated
     */
    bool isPeerAuthenticated(const std::string& peer_address, uint16_t peer_port) const;
    
    /**
     * @brief Check if peer is trusted
     * @param peer_address Peer's IP address
     * @param peer_port Peer's port
     * @return True if peer is trusted
     */
    bool isPeerTrusted(const std::string& peer_address, uint16_t peer_port) const;
    
    /**
     * @brief Get peer identity
     * @param peer_address Peer's IP address
     * @param peer_port Peer's port
     * @return Peer identity if found, nullptr otherwise
     */
    std::shared_ptr<PeerIdentity> getPeerIdentity(const std::string& peer_address, uint16_t peer_port) const;
    
    /**
     * @brief Add trusted peer
     * @param public_key Peer's public key
     * @param node_id Peer's node ID
     */
    void addTrustedPeer(const std::string& public_key, const std::string& node_id = "");
    
    /**
     * @brief Remove trusted peer
     * @param public_key Peer's public key
     */
    void removeTrustedPeer(const std::string& public_key);
    
    /**
     * @brief Add blacklisted peer
     * @param public_key Peer's public key
     * @param reason Blacklist reason
     */
    void addBlacklistedPeer(const std::string& public_key, const std::string& reason = "");
    
    /**
     * @brief Remove blacklisted peer
     * @param public_key Peer's public key
     */
    void removeBlacklistedPeer(const std::string& public_key);
    
    /**
     * @brief Sign data for peer authentication
     * @param data Data to sign
     * @return Signature as hexadecimal string
     */
    std::string signData(const std::string& data) const;
    
    /**
     * @brief Verify signature from peer
     * @param data Original data
     * @param signature Signature to verify
     * @param peer_address Peer's IP address
     * @param peer_port Peer's port
     * @return True if signature is valid
     */
    bool verifySignature(const std::string& data, const std::string& signature, 
                        const std::string& peer_address, uint16_t peer_port) const;
    
    /**
     * @brief Get node's public key
     * @return Node's public key as hexadecimal string
     */
    std::string getNodePublicKey() const;
    
    /**
     * @brief Get node's node ID
     * @return Node's unique identifier
     */
    std::string getNodeId() const;
    
    /**
     * @brief Update peer activity
     * @param peer_address Peer's IP address
     * @param peer_port Peer's port
     */
    void updatePeerActivity(const std::string& peer_address, uint16_t peer_port);
    
    /**
     * @brief Clean up expired challenges and sessions
     */
    void cleanupExpired();
    
    /**
     * @brief Get authentication statistics
     * @return JSON string with authentication statistics
     */
    std::string getStatistics() const;
    
    /**
     * @brief Get all authenticated peers
     * @return Vector of peer identities
     */
    std::vector<PeerIdentity> getAuthenticatedPeers() const;
    
    /**
     * @brief Get all trusted peers
     * @return Vector of trusted peer identities
     */
    std::vector<PeerIdentity> getTrustedPeers() const;

private:
    // Configuration
    AuthConfig config_;
    std::shared_ptr<crypto::KeyPair> node_keypair_;
    std::string node_id_;
    bool is_initialized_;
    
    // Challenge management
    std::map<std::string, AuthChallenge> active_challenges_;
    mutable std::mutex challenges_mutex_;
    
    // Peer identity management
    std::map<std::string, std::shared_ptr<PeerIdentity>> peer_identities_;
    mutable std::mutex identities_mutex_;
    
    // Trust management
    std::set<std::string> trusted_public_keys_;
    std::set<std::string> blacklisted_public_keys_;
    mutable std::mutex trust_mutex_;
    
    // Random number generation
    std::random_device rd_;
    std::mt19937_64 rng_;
    
    // Internal methods
    std::string generateChallengeId();
    std::string generateChallengeData();
    std::string getPeerKey(const std::string& address, uint16_t port) const;
    bool isChallengeValid(const AuthChallenge& challenge) const;
    bool isSessionValid(const PeerIdentity& identity) const;
    void removeExpiredChallenges();
    void removeExpiredSessions();
    std::string calculateNodeId(const std::string& public_key) const;
};

} // namespace network
} // namespace deo
