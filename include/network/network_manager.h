/**
 * @file network_manager.h
 * @brief Network manager for P2P networking
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#ifndef DEO_NETWORK_NETWORK_MANAGER_H
#define DEO_NETWORK_NETWORK_MANAGER_H

#include "network/peer_manager.h"
#include "network/network_messages.h"
#include "network/tcp_network.h"
#include "core/blockchain.h"
#include "core/transaction.h"
#include "consensus/consensus_engine.h"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

namespace deo {
namespace network {

/**
 * @brief Network configuration
 */
struct NetworkConfig {
    uint16_t listen_port;
    std::string listen_address;
    size_t max_connections;
    uint32_t connection_timeout_ms;
    uint32_t message_timeout_ms;
    bool enable_listening;
    std::vector<std::pair<std::string, uint16_t>> bootstrap_nodes;
    
    NetworkConfig() : listen_port(8333), listen_address("0.0.0.0"), 
                      max_connections(50), connection_timeout_ms(30000),
                      message_timeout_ms(5000), enable_listening(true) {}
};

/**
 * @brief Network statistics
 */
struct NetworkStats {
    uint64_t total_bytes_sent;
    uint64_t total_bytes_received;
    uint64_t total_messages_sent;
    uint64_t total_messages_received;
    uint64_t blocks_sent;
    uint64_t blocks_received;
    uint64_t transactions_sent;
    uint64_t transactions_received;
    uint32_t connection_attempts;
    uint32_t successful_connections;
    uint32_t failed_connections;
    uint32_t peers_banned;
    
    NetworkStats() : total_bytes_sent(0), total_bytes_received(0),
                     total_messages_sent(0), total_messages_received(0),
                     blocks_sent(0), blocks_received(0),
                     transactions_sent(0), transactions_received(0),
                     connection_attempts(0), successful_connections(0),
                     failed_connections(0), peers_banned(0) {}
};

/**
 * @brief Network manager class
 */
class NetworkManager {
public:
    /**
     * @brief Constructor
     * @param config Network configuration
     */
    explicit NetworkManager(const NetworkConfig& config);
    
    /**
     * @brief Destructor
     */
    ~NetworkManager();
    
    /**
     * @brief Initialize network manager
     * @param blockchain Blockchain instance
     * @param consensus_engine Consensus engine instance
     * @return True if successful
     */
    bool initialize(std::shared_ptr<core::Blockchain> blockchain,
                   std::shared_ptr<consensus::ConsensusEngine> consensus_engine);
    
    /**
     * @brief Shutdown network manager
     */
    void shutdown();
    
    /**
     * @brief Start networking
     * @return True if successful
     */
    bool start();
    
    /**
     * @brief Stop networking
     */
    void stop();
    
    /**
     * @brief Broadcast block to all peers
     * @param block Block to broadcast
     * @return Number of peers block was sent to
     */
    size_t broadcastBlock(std::shared_ptr<core::Block> block);
    
    /**
     * @brief Broadcast transaction to all peers
     * @param transaction Transaction to broadcast
     * @return Number of peers transaction was sent to
     */
    size_t broadcastTransaction(std::shared_ptr<core::Transaction> transaction);
    
    /**
     * @brief Send block to specific peer
     * @param address Peer address
     * @param port Peer port
     * @param block Block to send
     * @return True if sent successfully
     */
    bool sendBlockToPeer(const std::string& address, uint16_t port, 
                        std::shared_ptr<core::Block> block);
    
    /**
     * @brief Send transaction to specific peer
     * @param address Peer address
     * @param port Peer port
     * @param transaction Transaction to send
     * @return True if sent successfully
     */
    bool sendTransactionToPeer(const std::string& address, uint16_t port,
                              std::shared_ptr<core::Transaction> transaction);
    
    /**
     * @brief Request blocks from peer
     * @param address Peer address
     * @param port Peer port
     * @param block_hashes Block hashes to request
     * @return True if request sent successfully
     */
    bool requestBlocks(const std::string& address, uint16_t port,
                      const std::vector<std::string>& block_hashes);
    
    /**
     * @brief Request transactions from peer
     * @param address Peer address
     * @param port Peer port
     * @param tx_hashes Transaction hashes to request
     * @return True if request sent successfully
     */
    bool requestTransactions(const std::string& address, uint16_t port,
                            const std::vector<std::string>& tx_hashes);
    
    /**
     * @brief Connect to peer
     * @param address Peer address
     * @param port Peer port
     * @return True if connected successfully
     */
    bool connectToPeer(const std::string& address, uint16_t port);
    
    /**
     * @brief Disconnect from peer
     * @param address Peer address
     * @param port Peer port
     * @return True if disconnected successfully
     */
    bool disconnectFromPeer(const std::string& address, uint16_t port);
    
    /**
     * @brief Get connected peers
     * @return Vector of connected peer info
     */
    std::vector<PeerInfo> getConnectedPeers() const;
    
    /**
     * @brief Get network statistics
     * @return Network statistics
     */
    NetworkStats getNetworkStats() const;
    
    /**
     * @brief Get peer count
     * @return Number of connected peers
     */
    size_t getPeerCount() const;
    
    /**
     * @brief Check if peer is connected
     * @param address Peer address
     * @param port Peer port
     * @return True if connected
     */
    bool isPeerConnected(const std::string& address, uint16_t port) const;
    
    /**
     * @brief Ban peer
     * @param address Peer address
     * @param port Peer port
     * @param reason Ban reason
     */
    void banPeer(const std::string& address, uint16_t port, const std::string& reason = "");
    
    /**
     * @brief Set block handler callback
     * @param handler Function to call when block is received
     */
    void setBlockHandler(std::function<void(std::shared_ptr<core::Block>)> handler);
    
    /**
     * @brief Set transaction handler callback
     * @param handler Function to call when transaction is received
     */
    void setTransactionHandler(std::function<void(std::shared_ptr<core::Transaction>)> handler);
    
    /**
     * @brief Set peer connection handler callback
     * @param handler Function to call when peer connects
     */
    void setPeerConnectionHandler(std::function<void(const std::string&, uint16_t)> handler);
    
    /**
     * @brief Set peer disconnection handler callback
     * @param handler Function to call when peer disconnects
     */
    void setPeerDisconnectionHandler(std::function<void(const std::string&, uint16_t)> handler);

private:
    NetworkConfig config_;
    NetworkStats stats_;
    std::shared_ptr<PeerManager> peer_manager_;
    std::shared_ptr<TcpNetworkManager> tcp_network_manager_;
    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<consensus::ConsensusEngine> consensus_engine_;
    
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
    
    // Listening socket
    int listen_socket_;
    std::thread listen_thread_;
    std::thread message_thread_;
    
    // Message queue
    std::queue<std::pair<std::string, std::vector<uint8_t>>> message_queue_;
    std::mutex message_queue_mutex_;
    std::condition_variable message_queue_cv_;
    
    // Handlers
    std::function<void(std::shared_ptr<core::Block>)> block_handler_;
    std::function<void(std::shared_ptr<core::Transaction>)> transaction_handler_;
    std::function<void(const std::string&, uint16_t)> peer_connection_handler_;
    std::function<void(const std::string&, uint16_t)> peer_disconnection_handler_;
    
    /**
     * @brief Listening thread function
     */
    void listenThreadFunction();
    
    /**
     * @brief Message processing thread function
     */
    void messageThreadFunction();
    
    /**
     * @brief Handle incoming message
     * @param peer_address Peer address
     * @param peer_port Peer port
     * @param message_data Message data
     */
    void handleMessage(const std::string& peer_address, uint16_t peer_port,
                      const std::vector<uint8_t>& message_data);
    
    /**
     * @brief Handle HELLO message
     * @param peer_address Peer address
     * @param peer_port Peer port
     * @param message HELLO message
     */
    void handleHelloMessage(const std::string& peer_address, uint16_t peer_port,
                           std::shared_ptr<HelloMessage> message);
    
    /**
     * @brief Handle INV message
     * @param peer_address Peer address
     * @param peer_port Peer port
     * @param message INV message
     */
    void handleInvMessage(const std::string& peer_address, uint16_t peer_port,
                         std::shared_ptr<InvMessage> message);
    
    /**
     * @brief Handle GETDATA message
     * @param peer_address Peer address
     * @param peer_port Peer port
     * @param message GETDATA message
     */
    void handleGetDataMessage(const std::string& peer_address, uint16_t peer_port,
                             std::shared_ptr<GetDataMessage> message);
    
    /**
     * @brief Handle BLOCK message
     * @param peer_address Peer address
     * @param peer_port Peer port
     * @param message BLOCK message
     */
    void handleBlockMessage(const std::string& peer_address, uint16_t peer_port,
                           std::shared_ptr<BlockMessage> message);
    
    /**
     * @brief Handle TX message
     * @param peer_address Peer address
     * @param peer_port Peer port
     * @param message TX message
     */
    void handleTxMessage(const std::string& peer_address, uint16_t peer_port,
                        std::shared_ptr<TxMessage> message);
    
    /**
     * @brief Handle PING message
     * @param peer_address Peer address
     * @param peer_port Peer port
     * @param message PING message
     */
    void handlePingMessage(const std::string& peer_address, uint16_t peer_port,
                          std::shared_ptr<PingMessage> message);
    
    /**
     * @brief Handle PONG message
     * @param peer_address Peer address
     * @param peer_port Peer port
     * @param message PONG message
     */
    void handlePongMessage(const std::string& peer_address, uint16_t peer_port,
                          std::shared_ptr<PongMessage> message);
    
    /**
     * @brief Send HELLO message to peer
     * @param address Peer address
     * @param port Peer port
     * @return True if sent successfully
     */
    bool sendHelloMessage(const std::string& address, uint16_t port);
    
    /**
     * @brief Send PING message to peer
     * @param address Peer address
     * @param port Peer port
     * @return True if sent successfully
     */
    bool sendPingMessage(const std::string& address, uint16_t port);
    
    /**
     * @brief Validate incoming block
     * @param block Block to validate
     * @return True if valid
     */
    bool validateIncomingBlock(std::shared_ptr<core::Block> block);
    
    /**
     * @brief Validate incoming transaction
     * @param transaction Transaction to validate
     * @return True if valid
     */
    bool validateIncomingTransaction(std::shared_ptr<core::Transaction> transaction);
    
    /**
     * @brief Update network statistics
     * @param bytes_sent Bytes sent
     * @param bytes_received Bytes received
     * @param messages_sent Messages sent
     * @param messages_received Messages received
     */
    void updateStats(uint64_t bytes_sent, uint64_t bytes_received,
                    uint64_t messages_sent, uint64_t messages_received);
    
    /**
     * @brief Check if we have an item (block or transaction)
     * @param hash Item hash
     * @return True if we have the item
     */
    bool hasItem(const std::string& hash) const;
    
    /**
     * @brief Request an item from a peer
     * @param peer_address Peer address
     * @param peer_port Peer port
     * @param item_hash Hash of item to request
     */
    void requestItem(const std::string& peer_address, uint16_t peer_port, const std::string& item_hash);
};

} // namespace network
} // namespace deo

#endif // DEO_NETWORK_NETWORK_MANAGER_H
