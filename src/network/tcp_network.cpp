/**
 * @file tcp_network.cpp
 * @brief TCP networking implementation for P2P blockchain communication
 */

#include "network/tcp_network.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace deo {
namespace network {

TcpConnection::TcpConnection(int socket_fd, const std::string& peer_address, uint16_t peer_port)
    : socket_fd_(socket_fd)
    , peer_address_(peer_address)
    , peer_port_(peer_port)
    , last_seen_(std::chrono::steady_clock::now())
    , connected_(true) {
    DEO_LOG_INFO(NETWORKING, "Created TCP connection to " + peer_address_ + ":" + std::to_string(peer_port_));
}

TcpConnection::~TcpConnection() {
    close();
}

bool TcpConnection::sendMessage(const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (!connected_ || socket_fd_ < 0) {
        return false;
    }
    
    try {
        // Serialize message
        nlohmann::json message_json = message.toJson();
        std::string serialized = message_json.dump();
        
        // Send message length first (4 bytes)
        uint32_t message_length = htonl(static_cast<uint32_t>(serialized.length()));
        ssize_t sent = send(socket_fd_, &message_length, sizeof(message_length), MSG_NOSIGNAL);
        if (sent != sizeof(message_length)) {
            DEO_LOG_ERROR(NETWORKING, "Failed to send message length to " + peer_address_);
            return false;
        }
        
        // Send message data
        sent = send(socket_fd_, serialized.c_str(), serialized.length(), MSG_NOSIGNAL);
        if (sent != static_cast<ssize_t>(serialized.length())) {
            DEO_LOG_ERROR(NETWORKING, "Failed to send message data to " + peer_address_);
            return false;
        }
        
        setLastSeen();
        DEO_LOG_DEBUG(NETWORKING, "Sent message to " + peer_address_ + ": " + std::to_string(static_cast<int>(message.getType())));
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Exception sending message to " + peer_address_ + ": " + e.what());
        return false;
    }
}

std::unique_ptr<NetworkMessage> TcpConnection::receiveMessage() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (!connected_ || socket_fd_ < 0) {
        return nullptr;
    }
    
    try {
        // Receive message length first (4 bytes)
        uint32_t message_length;
        ssize_t received = recv(socket_fd_, &message_length, sizeof(message_length), MSG_DONTWAIT);
        if (received <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return nullptr; // No data available
            }
            DEO_LOG_ERROR(NETWORKING, "Failed to receive message length from " + peer_address_);
            return nullptr;
        }
        
        message_length = ntohl(message_length);
        if (message_length > 1024 * 1024) { // 1MB limit
            DEO_LOG_ERROR(NETWORKING, "Message too large from " + peer_address_ + ": " + std::to_string(message_length));
            return nullptr;
        }
        
        // Receive message data
        std::vector<char> buffer(message_length);
        received = recv(socket_fd_, buffer.data(), message_length, MSG_DONTWAIT);
        if (received != static_cast<ssize_t>(message_length)) {
            DEO_LOG_ERROR(NETWORKING, "Failed to receive message data from " + peer_address_);
            return nullptr;
        }
        
        // Deserialize message
        std::string serialized(buffer.begin(), buffer.end());
        nlohmann::json message_json = nlohmann::json::parse(serialized);
        
        // Create appropriate message type based on JSON
        MessageType type = static_cast<MessageType>(message_json["type"].get<int>());
        auto message = MessageFactory::createMessage(type);
        if (message) {
            message->fromJson(message_json);
            setLastSeen();
            DEO_LOG_DEBUG(NETWORKING, "Received message from " + peer_address_ + ": " + std::to_string(static_cast<int>(message->getType())));
        }
        
        return message;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Exception receiving message from " + peer_address_ + ": " + e.what());
        return nullptr;
    }
}

void TcpConnection::close() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (connected_ && socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
        connected_ = false;
        DEO_LOG_INFO(NETWORKING, "Closed TCP connection to " + peer_address_);
    }
}

bool TcpConnection::isConnected() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return connected_ && socket_fd_ >= 0;
}

std::string TcpConnection::getPeerAddress() const {
    return peer_address_;
}

uint16_t TcpConnection::getPeerPort() const {
    return peer_port_;
}

void TcpConnection::setLastSeen() {
    last_seen_ = std::chrono::steady_clock::now();
}

std::chrono::steady_clock::time_point TcpConnection::getLastSeen() const {
    return last_seen_;
}

TcpNetworkManager::TcpNetworkManager(uint16_t listen_port)
    : listen_port_(listen_port)
    , listen_socket_(-1)
    , running_(false) {
    stats_.messages_sent = 0;
    stats_.messages_received = 0;
    stats_.bytes_sent = 0;
    stats_.bytes_received = 0;
    stats_.connection_attempts = 0;
    stats_.connection_failures = 0;
    stats_.start_time = std::chrono::steady_clock::now();
}

TcpNetworkManager::~TcpNetworkManager() {
    shutdown();
}

bool TcpNetworkManager::initialize() {
    DEO_LOG_INFO(NETWORKING, "Initializing TCP network manager on port " + std::to_string(listen_port_));
    
    // Create listening socket
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ < 0) {
        DEO_LOG_ERROR(NETWORKING, "Failed to create listening socket");
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        DEO_LOG_ERROR(NETWORKING, "Failed to set socket options");
        close(listen_socket_);
        return false;
    }
    
    // Bind to port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(listen_port_);
    
    if (bind(listen_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        DEO_LOG_ERROR(NETWORKING, "Failed to bind to port " + std::to_string(listen_port_));
        close(listen_socket_);
        return false;
    }
    
    // Start listening
    if (listen(listen_socket_, 10) < 0) {
        DEO_LOG_ERROR(NETWORKING, "Failed to start listening");
        close(listen_socket_);
        return false;
    }
    
    // Set socket to non-blocking
    if (!setSocketNonBlocking(listen_socket_)) {
        DEO_LOG_ERROR(NETWORKING, "Failed to set listening socket to non-blocking");
        close(listen_socket_);
        return false;
    }
    
    running_ = true;
    
    // Start threads
    listener_thread_ = std::thread(&TcpNetworkManager::listenerLoop, this);
    message_handler_thread_ = std::thread(&TcpNetworkManager::messageHandlerLoop, this);
    
    DEO_LOG_INFO(NETWORKING, "TCP network manager initialized successfully");
    return true;
}

bool TcpNetworkManager::initializeForTesting() {
    DEO_LOG_INFO(NETWORKING, "Initializing TCP network manager for testing (no background threads)");
    
    // Create listening socket
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ < 0) {
        DEO_LOG_ERROR(NETWORKING, "Failed to create listening socket");
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        DEO_LOG_ERROR(NETWORKING, "Failed to set socket options");
        close(listen_socket_);
        return false;
    }
    
    // Bind to port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(listen_port_);
    
    if (bind(listen_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        DEO_LOG_ERROR(NETWORKING, "Failed to bind to port " + std::to_string(listen_port_));
        close(listen_socket_);
        return false;
    }
    
    // Start listening
    if (listen(listen_socket_, 10) < 0) {
        DEO_LOG_ERROR(NETWORKING, "Failed to start listening");
        close(listen_socket_);
        return false;
    }
    
    // Set socket to non-blocking
    if (!setSocketNonBlocking(listen_socket_)) {
        DEO_LOG_ERROR(NETWORKING, "Failed to set listening socket to non-blocking");
        close(listen_socket_);
        return false;
    }
    
    running_ = true;
    
    // DO NOT start background threads in test mode
    // This prevents memory corruption in test environment
    
    DEO_LOG_INFO(NETWORKING, "TCP network manager initialized for testing (no background threads)");
    return true;
}

void TcpNetworkManager::shutdown() {
    if (!running_) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Shutting down TCP network manager");
    running_ = false;
    
    // Close listening socket
    if (listen_socket_ >= 0) {
        close(listen_socket_);
        listen_socket_ = -1;
    }
    
    // Close all peer connections
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        for (auto& [peer_key, connection] : peers_) {
            connection->close();
        }
        peers_.clear();
    }
    
    // Notify message handler thread
    message_condition_.notify_all();
    
    // Wait for threads to finish
    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }
    if (message_handler_thread_.joinable()) {
        message_handler_thread_.join();
    }
    
    DEO_LOG_INFO(NETWORKING, "TCP network manager shutdown complete");
}

bool TcpNetworkManager::connectToPeer(const std::string& address, uint16_t port) {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.connection_attempts++;
    
    std::string peer_key = formatPeerKey(address, port);
    
    // Check if already connected
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        if (peers_.find(peer_key) != peers_.end()) {
            DEO_LOG_WARNING(NETWORKING, "Already connected to peer " + peer_key);
            return true;
        }
    }
    
    DEO_LOG_INFO(NETWORKING, "Connecting to peer " + address + ":" + std::to_string(port));
    
    // Create socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        DEO_LOG_ERROR(NETWORKING, "Failed to create socket for peer " + address);
        stats_.connection_failures++;
        return false;
    }
    
    // Set socket to non-blocking
    if (!setSocketNonBlocking(socket_fd)) {
        DEO_LOG_ERROR(NETWORKING, "Failed to set socket to non-blocking for peer " + address);
        close(socket_fd);
        stats_.connection_failures++;
        return false;
    }
    
    // Connect to peer
    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address.c_str(), &peer_addr.sin_addr) <= 0) {
        DEO_LOG_ERROR(NETWORKING, "Invalid peer address: " + address);
        close(socket_fd);
        stats_.connection_failures++;
        return false;
    }
    
    int result = connect(socket_fd, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
    if (result < 0 && errno != EINPROGRESS) {
        DEO_LOG_ERROR(NETWORKING, "Failed to connect to peer " + address + ":" + std::to_string(port));
        close(socket_fd);
        stats_.connection_failures++;
        return false;
    }
    
    // Wait for connection to complete with timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create connection object
    auto connection = std::make_shared<TcpConnection>(socket_fd, address, port);
    
    // Add to peers
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_[peer_key] = connection;
    }
    
    DEO_LOG_INFO(NETWORKING, "Successfully connected to peer " + peer_key);
    return true;
}

void TcpNetworkManager::disconnectPeer(const std::string& address, uint16_t port) {
    std::string peer_key = formatPeerKey(address, port);
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second->close();
        peers_.erase(it);
        DEO_LOG_INFO(NETWORKING, "Disconnected peer " + peer_key);
    }
}

std::vector<std::string> TcpNetworkManager::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::vector<std::string> peer_list;
    
    for (const auto& [peer_key, connection] : peers_) {
        if (connection->isConnected()) {
            peer_list.push_back(peer_key);
        }
    }
    
    return peer_list;
}

size_t TcpNetworkManager::getPeerCount() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return peers_.size();
}

void TcpNetworkManager::broadcastMessage(const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    for (const auto& [peer_key, connection] : peers_) {
        if (connection->isConnected()) {
            if (connection->sendMessage(message)) {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.messages_sent++;
            }
        }
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Broadcasted message: " + std::to_string(static_cast<int>(message.getType())));
}

void TcpNetworkManager::sendToPeer(const std::string& peer_address, const NetworkMessage& message) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    for (const auto& [peer_key, connection] : peers_) {
        if (connection->getPeerAddress() == peer_address && connection->isConnected()) {
            if (connection->sendMessage(message)) {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.messages_sent++;
            }
            return;
        }
    }
    
    DEO_LOG_WARNING(NETWORKING, "Peer not found for sending message: " + peer_address);
}

void TcpNetworkManager::setMessageHandler(MessageType message_type, 
                                         std::function<void(const NetworkMessage&, const std::string&)> handler) {
    message_handlers_[message_type] = handler;
}

TcpNetworkManager::NetworkStats TcpNetworkManager::getNetworkStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void TcpNetworkManager::addBootstrapNode(const std::string& address, uint16_t port) {
    bootstrap_nodes_.push_back({address, port});
    DEO_LOG_INFO(NETWORKING, "Added bootstrap node: " + address + ":" + std::to_string(port));
}

void TcpNetworkManager::discoverPeers() {
    DEO_LOG_INFO(NETWORKING, "Discovering peers from bootstrap nodes");
    
    for (const auto& [address, port] : bootstrap_nodes_) {
        connectToPeer(address, port);
    }
}

void TcpNetworkManager::listenerLoop() {
    DEO_LOG_INFO(NETWORKING, "TCP listener thread started");
    
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(listen_socket_, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket >= 0) {
            char client_address[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_address, INET_ADDRSTRLEN);
            [[maybe_unused]] uint16_t client_port = ntohs(client_addr.sin_port);
            
            handleNewConnection(client_socket, std::string(client_address));
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            if (running_) {
                DEO_LOG_ERROR(NETWORKING, "Accept failed: " + std::string(strerror(errno)));
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    DEO_LOG_INFO(NETWORKING, "TCP listener thread stopped");
}

void TcpNetworkManager::messageHandlerLoop() {
    DEO_LOG_INFO(NETWORKING, "TCP message handler thread started");
    
    while (running_) {
        std::unique_lock<std::mutex> lock(message_queue_mutex_);
        message_condition_.wait(lock, [this] { return !message_queue_.empty() || !running_; });
        
        if (!running_) {
            break;
        }
        
        while (!message_queue_.empty()) {
            auto message_pair = std::move(message_queue_.front());
            message_queue_.pop();
            lock.unlock();
            
            auto& message = message_pair.first;
            auto& peer_address = message_pair.second;
            
            // Handle message
            if (message) {
                auto handler_it = message_handlers_.find(message->getType());
                if (handler_it != message_handlers_.end()) {
                    try {
                        handler_it->second(*message, peer_address);
                    } catch (const std::exception& e) {
                        DEO_LOG_ERROR(NETWORKING, "Exception in message handler: " + std::string(e.what()));
                    }
                }
            }
            
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.messages_received++;
            }
            
            lock.lock();
        }
    }
    
    DEO_LOG_INFO(NETWORKING, "TCP message handler thread stopped");
}

void TcpNetworkManager::handleNewConnection(int client_socket, const std::string& client_address) {
    DEO_LOG_INFO(NETWORKING, "New connection from " + client_address);
    
    // Set socket to non-blocking
    if (!setSocketNonBlocking(client_socket)) {
        DEO_LOG_ERROR(NETWORKING, "Failed to set client socket to non-blocking");
        close(client_socket);
        return;
    }
    
    // Create connection object
    auto connection = std::make_shared<TcpConnection>(client_socket, client_address, 0);
    
    // Add to peers
    std::string peer_key = formatPeerKey(client_address, 0);
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_[peer_key] = connection;
    }
    
    // Start receiving messages from this peer
    std::thread([this, connection, peer_key]() {
        while (running_ && connection->isConnected()) {
            auto message = connection->receiveMessage();
            if (message) {
                std::lock_guard<std::mutex> lock(message_queue_mutex_);
                message_queue_.push({std::move(message), connection->getPeerAddress()});
                message_condition_.notify_one();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        handlePeerDisconnection(peer_key);
    }).detach();
}

void TcpNetworkManager::handlePeerDisconnection(const std::string& peer_key) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_key);
    if (it != peers_.end()) {
        it->second->close();
        peers_.erase(it);
        DEO_LOG_INFO(NETWORKING, "Peer disconnected: " + peer_key);
    }
}

bool TcpNetworkManager::setSocketNonBlocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

std::string TcpNetworkManager::formatPeerKey(const std::string& address, uint16_t port) const {
    return address + ":" + std::to_string(port);
}

} // namespace network
} // namespace deo
