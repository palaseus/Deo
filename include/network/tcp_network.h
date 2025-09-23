/**
 * @file tcp_network.h
 * @brief TCP networking implementation for P2P blockchain communication
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <map>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "network_messages.h"
#include "utils/logger.h"

namespace deo {
namespace network {

/**
 * @brief TCP connection handler for individual peer connections
 */
class TcpConnection {
public:
    TcpConnection(int socket_fd, const std::string& peer_address, uint16_t peer_port);
    ~TcpConnection();

    bool sendMessage(const NetworkMessage& message);
    std::unique_ptr<NetworkMessage> receiveMessage();
    void close();
    bool isConnected() const;
    std::string getPeerAddress() const;
    uint16_t getPeerPort() const;
    void setLastSeen();
    std::chrono::steady_clock::time_point getLastSeen() const;

private:
    int socket_fd_;
    std::string peer_address_;
    uint16_t peer_port_;
    std::chrono::steady_clock::time_point last_seen_;
    mutable std::mutex connection_mutex_;
    bool connected_;
};

/**
 * @brief TCP network manager for P2P communication
 */
class TcpNetworkManager {
public:
    TcpNetworkManager(uint16_t listen_port = 8333);
    ~TcpNetworkManager();

    bool initialize();
    void shutdown();
    
    // Peer management
    bool connectToPeer(const std::string& address, uint16_t port);
    void disconnectPeer(const std::string& address, uint16_t port);
    std::vector<std::string> getConnectedPeers() const;
    size_t getPeerCount() const;
    
    // Message broadcasting
    void broadcastMessage(const NetworkMessage& message);
    void sendToPeer(const std::string& peer_address, const NetworkMessage& message);
    
    // Message handling
    void setMessageHandler(MessageType message_type, 
                          std::function<void(const NetworkMessage&, const std::string&)> handler);
    
    // Network statistics
    struct NetworkStats {
        size_t messages_sent;
        size_t messages_received;
        size_t bytes_sent;
        size_t bytes_received;
        size_t connection_attempts;
        size_t connection_failures;
        std::chrono::steady_clock::time_point start_time;
    };
    
    NetworkStats getNetworkStats() const;
    
    // Bootstrap nodes
    void addBootstrapNode(const std::string& address, uint16_t port);
    void discoverPeers();

private:
    uint16_t listen_port_;
    int listen_socket_;
    std::atomic<bool> running_;
    std::thread listener_thread_;
    std::thread message_handler_thread_;
    
    mutable std::mutex peers_mutex_;
    std::map<std::string, std::shared_ptr<TcpConnection>> peers_;
    
    mutable std::mutex message_queue_mutex_;
    std::queue<std::pair<std::unique_ptr<NetworkMessage>, std::string>> message_queue_;
    std::condition_variable message_condition_;
    
    std::map<MessageType, std::function<void(const NetworkMessage&, const std::string&)>> message_handlers_;
    
    std::vector<std::pair<std::string, uint16_t>> bootstrap_nodes_;
    
    NetworkStats stats_;
    mutable std::mutex stats_mutex_;
    
    void listenerLoop();
    void messageHandlerLoop();
    void handleNewConnection(int client_socket, const std::string& client_address);
    void handlePeerDisconnection(const std::string& peer_address);
    bool setSocketNonBlocking(int socket_fd);
    std::string formatPeerKey(const std::string& address, uint16_t port) const;
};

} // namespace network
} // namespace deo
