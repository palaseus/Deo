/**
 * @file network_manager.cpp
 * @brief Network manager implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "network/network_manager.h"
#include "utils/logger.h"
#include "utils/error_handler.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <random>
#include <sstream>

namespace deo {
namespace network {

NetworkManager::NetworkManager(const NetworkConfig& config)
    : config_(config), stats_(), peer_manager_(nullptr), blockchain_(nullptr),
      consensus_engine_(nullptr), initialized_(false), running_(false),
      should_stop_(false), listen_socket_(-1) {
}

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize(std::shared_ptr<core::Blockchain> blockchain,
                               std::shared_ptr<consensus::ConsensusEngine> consensus_engine) {
    if (initialized_) {
        return true;
    }
    
    blockchain_ = blockchain;
    consensus_engine_ = consensus_engine;
    
    // Initialize peer manager
    peer_manager_ = std::make_shared<PeerManager>(config_.max_connections, 100);
    if (!peer_manager_->initialize()) {
        DEO_LOG_ERROR(NETWORKING, "Failed to initialize peer manager");
        return false;
    }
    
    // Set up peer manager handlers
    peer_manager_->setMessageHandler([this](const std::string& address, uint16_t port, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        message_queue_.push({address + ":" + std::to_string(port), data});
        message_queue_cv_.notify_one();
    });
    
    peer_manager_->setConnectionHandler([this](const std::string& address, uint16_t port) {
        DEO_LOG_INFO(NETWORKING, "Peer connected: " + address + ":" + std::to_string(port));
        stats_.successful_connections++;
        
        // Send HELLO message
        sendHelloMessage(address, port);
        
        if (peer_connection_handler_) {
            peer_connection_handler_(address, port);
        }
    });
    
    peer_manager_->setDisconnectionHandler([this](const std::string& address, uint16_t port) {
        DEO_LOG_INFO(NETWORKING, "Peer disconnected: " + address + ":" + std::to_string(port));
        
        if (peer_disconnection_handler_) {
            peer_disconnection_handler_(address, port);
        }
    });
    
    initialized_ = true;
    DEO_LOG_INFO(NETWORKING, "Network manager initialized");
    return true;
}

void NetworkManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    stop();
    
    if (peer_manager_) {
        peer_manager_->shutdown();
        peer_manager_.reset();
    }
    
    initialized_ = false;
    DEO_LOG_INFO(NETWORKING, "Network manager shutdown");
}

bool NetworkManager::start() {
    if (!initialized_) {
        DEO_LOG_ERROR(NETWORKING, "Network manager not initialized");
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    should_stop_ = false;
    
    // Start listening if enabled
    if (config_.enable_listening) {
        listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket_ < 0) {
            DEO_LOG_ERROR(NETWORKING, "Failed to create listen socket");
            return false;
        }
        
        // Set socket options
        int opt = 1;
        if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            DEO_LOG_ERROR(NETWORKING, "Failed to set socket options");
            close(listen_socket_);
            listen_socket_ = -1;
            return false;
        }
        
        // Bind socket
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.listen_port);
        
        if (inet_pton(AF_INET, config_.listen_address.c_str(), &addr.sin_addr) <= 0) {
            DEO_LOG_ERROR(NETWORKING, "Invalid listen address: " + config_.listen_address);
            close(listen_socket_);
            listen_socket_ = -1;
            return false;
        }
        
        if (bind(listen_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            DEO_LOG_ERROR(NETWORKING, "Failed to bind socket to " + config_.listen_address + ":" + std::to_string(config_.listen_port));
            close(listen_socket_);
            listen_socket_ = -1;
            return false;
        }
        
        if (listen(listen_socket_, 10) < 0) {
            DEO_LOG_ERROR(NETWORKING, "Failed to listen on socket");
            close(listen_socket_);
            listen_socket_ = -1;
            return false;
        }
        
        // Start listen thread
        listen_thread_ = std::thread(&NetworkManager::listenThreadFunction, this);
        
        DEO_LOG_INFO(NETWORKING, "Started listening on " + config_.listen_address + ":" + std::to_string(config_.listen_port));
    }
    
    // Start message processing thread
    message_thread_ = std::thread(&NetworkManager::messageThreadFunction, this);
    
    // Start peer discovery
    peer_manager_->startPeerDiscovery(config_.bootstrap_nodes);
    
    running_ = true;
    DEO_LOG_INFO(NETWORKING, "Network manager started");
    return true;
}

void NetworkManager::stop() {
    if (!running_) {
        return;
    }
    
    should_stop_ = true;
    
    // Stop listening
    if (listen_socket_ >= 0) {
        close(listen_socket_);
        listen_socket_ = -1;
    }
    
    // Stop peer discovery
    peer_manager_->stopPeerDiscovery();
    
    // Wait for threads
    if (listen_thread_.joinable()) {
        listen_thread_.join();
    }
    
    if (message_thread_.joinable()) {
        message_thread_.join();
    }
    
    running_ = false;
    DEO_LOG_INFO(NETWORKING, "Network manager stopped");
}

size_t NetworkManager::broadcastBlock(std::shared_ptr<core::Block> block) {
    if (!block) {
        return 0;
    }
    
    // Serialize block
    auto block_data = block->serialize();
    if (block_data.empty()) {
        DEO_LOG_ERROR(NETWORKING, "Failed to serialize block for broadcasting");
        return 0;
    }
    
    // Create BLOCK message
    BlockMessage block_msg(block_data);
    auto message_data = block_msg.serialize();
    
    // Broadcast to all peers
    size_t sent_count = peer_manager_->broadcastMessage(message_data);
    
    stats_.blocks_sent += sent_count;
    stats_.total_messages_sent += sent_count;
    stats_.total_bytes_sent += message_data.size() * sent_count;
    
    DEO_LOG_INFO(NETWORKING, "Broadcasted block to " + std::to_string(sent_count) + " peers");
    return sent_count;
}

size_t NetworkManager::broadcastTransaction(std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        return 0;
    }
    
    // Serialize transaction
    auto tx_data = transaction->serialize();
    if (tx_data.empty()) {
        DEO_LOG_ERROR(NETWORKING, "Failed to serialize transaction for broadcasting");
        return 0;
    }
    
    // Create TX message
    TxMessage tx_msg(tx_data);
    auto message_data = tx_msg.serialize();
    
    // Broadcast to all peers
    size_t sent_count = peer_manager_->broadcastMessage(message_data);
    
    stats_.transactions_sent += sent_count;
    stats_.total_messages_sent += sent_count;
    stats_.total_bytes_sent += message_data.size() * sent_count;
    
    DEO_LOG_INFO(NETWORKING, "Broadcasted transaction to " + std::to_string(sent_count) + " peers");
    return sent_count;
}

bool NetworkManager::sendBlockToPeer(const std::string& address, uint16_t port, 
                                    std::shared_ptr<core::Block> block) {
    if (!block) {
        return false;
    }
    
    // Serialize block
    auto block_data = block->serialize();
    if (block_data.empty()) {
        return false;
    }
    
    // Create BLOCK message
    BlockMessage block_msg(block_data);
    auto message_data = block_msg.serialize();
    
    // Send to peer
    bool success = peer_manager_->sendMessageToPeer(address, port, message_data);
    
    if (success) {
        stats_.blocks_sent++;
        stats_.total_messages_sent++;
        stats_.total_bytes_sent += message_data.size();
    }
    
    return success;
}

bool NetworkManager::sendTransactionToPeer(const std::string& address, uint16_t port,
                                          std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        return false;
    }
    
    // Serialize transaction
    auto tx_data = transaction->serialize();
    if (tx_data.empty()) {
        return false;
    }
    
    // Create TX message
    TxMessage tx_msg(tx_data);
    auto message_data = tx_msg.serialize();
    
    // Send to peer
    bool success = peer_manager_->sendMessageToPeer(address, port, message_data);
    
    if (success) {
        stats_.transactions_sent++;
        stats_.total_messages_sent++;
        stats_.total_bytes_sent += message_data.size();
    }
    
    return success;
}

bool NetworkManager::requestBlocks(const std::string& address, uint16_t port,
                                  const std::vector<std::string>& block_hashes) {
    std::vector<InventoryItem> items;
    for (const auto& hash : block_hashes) {
        items.emplace_back(2, hash); // 2 = block
    }
    
    GetDataMessage getdata_msg(items);
    auto message_data = getdata_msg.serialize();
    
    return peer_manager_->sendMessageToPeer(address, port, message_data);
}

bool NetworkManager::requestTransactions(const std::string& address, uint16_t port,
                                        const std::vector<std::string>& tx_hashes) {
    std::vector<InventoryItem> items;
    for (const auto& hash : tx_hashes) {
        items.emplace_back(1, hash); // 1 = transaction
    }
    
    GetDataMessage getdata_msg(items);
    auto message_data = getdata_msg.serialize();
    
    return peer_manager_->sendMessageToPeer(address, port, message_data);
}

bool NetworkManager::connectToPeer(const std::string& address, uint16_t port) {
    stats_.connection_attempts++;
    return peer_manager_->connectPeer(address, port);
}

bool NetworkManager::disconnectFromPeer(const std::string& address, uint16_t port) {
    return peer_manager_->disconnectPeer(address, port);
}

std::vector<PeerInfo> NetworkManager::getConnectedPeers() const {
    return peer_manager_->getConnectedPeers();
}

NetworkStats NetworkManager::getNetworkStats() const {
    return stats_;
}

size_t NetworkManager::getPeerCount() const {
    return peer_manager_->getPeerCount();
}

bool NetworkManager::isPeerConnected(const std::string& address, uint16_t port) const {
    auto peer = peer_manager_->getPeerConnection(address, port);
    return peer && peer->isConnected();
}

void NetworkManager::banPeer(const std::string& address, uint16_t port, const std::string& reason) {
    peer_manager_->banPeer(address, port, reason);
    stats_.peers_banned++;
}

void NetworkManager::setBlockHandler(std::function<void(std::shared_ptr<core::Block>)> handler) {
    block_handler_ = handler;
}

void NetworkManager::setTransactionHandler(std::function<void(std::shared_ptr<core::Transaction>)> handler) {
    transaction_handler_ = handler;
}

void NetworkManager::setPeerConnectionHandler(std::function<void(const std::string&, uint16_t)> handler) {
    peer_connection_handler_ = handler;
}

void NetworkManager::setPeerDisconnectionHandler(std::function<void(const std::string&, uint16_t)> handler) {
    peer_disconnection_handler_ = handler;
}

void NetworkManager::listenThreadFunction() {
    while (!should_stop_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(listen_socket_, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (!should_stop_) {
                DEO_LOG_ERROR(NETWORKING, "Failed to accept connection: " + std::string(strerror(errno)));
            }
            break;
        }
        
        // Get client address
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        uint16_t client_port = ntohs(client_addr.sin_port);
        
        DEO_LOG_INFO(NETWORKING, "Accepted connection from " + std::string(client_ip) + ":" + std::to_string(client_port));
        
        // Add peer
        peer_manager_->addPeer(client_ip, client_port);
        peer_manager_->connectPeer(client_ip, client_port);
        
        close(client_socket);
    }
}

void NetworkManager::messageThreadFunction() {
    while (!should_stop_) {
        std::unique_lock<std::mutex> lock(message_queue_mutex_);
        
        message_queue_cv_.wait(lock, [this] { return !message_queue_.empty() || should_stop_; });
        
        if (should_stop_) {
            break;
        }
        
        if (message_queue_.empty()) {
            continue;
        }
        
        auto message_pair = message_queue_.front();
        message_queue_.pop();
        lock.unlock();
        
        // Parse peer address and port
        size_t colon_pos = message_pair.first.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        
        std::string peer_address = message_pair.first.substr(0, colon_pos);
        uint16_t peer_port = static_cast<uint16_t>(std::stoi(message_pair.first.substr(colon_pos + 1)));
        
        handleMessage(peer_address, peer_port, message_pair.second);
    }
}

void NetworkManager::handleMessage(const std::string& peer_address, uint16_t peer_port,
                                  const std::vector<uint8_t>& message_data) {
    auto message = MessageFactory::createMessage(message_data);
    if (!message) {
        DEO_LOG_WARNING(NETWORKING, "Failed to parse message from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    if (!message->validate()) {
        DEO_LOG_WARNING(NETWORKING, "Invalid message from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    stats_.total_messages_received++;
    stats_.total_bytes_received += message_data.size();
    
    switch (message->getType()) {
        case MessageType::HELLO:
            handleHelloMessage(peer_address, peer_port, std::dynamic_pointer_cast<HelloMessage>(message));
            break;
        case MessageType::INV:
            handleInvMessage(peer_address, peer_port, std::dynamic_pointer_cast<InvMessage>(message));
            break;
        case MessageType::GETDATA:
            handleGetDataMessage(peer_address, peer_port, std::dynamic_pointer_cast<GetDataMessage>(message));
            break;
        case MessageType::BLOCK:
            handleBlockMessage(peer_address, peer_port, std::dynamic_pointer_cast<BlockMessage>(message));
            break;
        case MessageType::TX:
            handleTxMessage(peer_address, peer_port, std::dynamic_pointer_cast<TxMessage>(message));
            break;
        case MessageType::PING:
            handlePingMessage(peer_address, peer_port, std::dynamic_pointer_cast<PingMessage>(message));
            break;
        case MessageType::PONG:
            handlePongMessage(peer_address, peer_port, std::dynamic_pointer_cast<PongMessage>(message));
            break;
        default:
            DEO_LOG_WARNING(NETWORKING, "Unknown message type from " + peer_address + ":" + std::to_string(peer_port));
            break;
    }
}

void NetworkManager::handleHelloMessage(const std::string& peer_address, uint16_t peer_port,
                                       std::shared_ptr<HelloMessage> message) {
    if (!message) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Received HELLO from " + peer_address + ":" + std::to_string(peer_port) + 
                 " - " + message->toString());
    
    // Update peer info
    auto peer = peer_manager_->getPeerConnection(peer_address, peer_port);
    if (peer) {
        auto& info = peer->getInfo();
        info.version = message->getVersion();
        info.user_agent = message->getUserAgent();
        info.services = message->getServices();
        info.nonce = message->getNonce();
        info.state = PeerState::READY;
    }
}

void NetworkManager::handleInvMessage(const std::string& peer_address, uint16_t peer_port,
                                     std::shared_ptr<InvMessage> message) {
    if (!message) {
        return;
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Received INV from " + peer_address + ":" + std::to_string(peer_port) + 
                  " - " + message->toString());
    
    // Request items we don't have
    std::vector<InventoryItem> items_to_request;
    for (const auto& item : message->getItems()) {
        if (item.type == 1) { // Transaction
            // Check if we have this transaction
            // In a real implementation, you'd check the mempool
            items_to_request.push_back(item);
        } else if (item.type == 2) { // Block
            // Check if we have this block
            if (blockchain_ && !blockchain_->getBlock(item.hash)) {
                items_to_request.push_back(item);
            }
        }
    }
    
    if (!items_to_request.empty()) {
        GetDataMessage getdata_msg(items_to_request);
        auto message_data = getdata_msg.serialize();
        peer_manager_->sendMessageToPeer(peer_address, peer_port, message_data);
    }
}

void NetworkManager::handleGetDataMessage(const std::string& peer_address, uint16_t peer_port,
                                         std::shared_ptr<GetDataMessage> message) {
    if (!message) {
        return;
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Received GETDATA from " + peer_address + ":" + std::to_string(peer_port) + 
                  " - " + message->toString());
    
    // Send requested items
    for (const auto& item : message->getItems()) {
        if (item.type == 1) { // Transaction
            // Send transaction
            // In a real implementation, you'd look up the transaction
        } else if (item.type == 2) { // Block
            // Send block
            if (blockchain_) {
                auto block = blockchain_->getBlock(item.hash);
                if (block) {
                    sendBlockToPeer(peer_address, peer_port, block);
                }
            }
        }
    }
}

void NetworkManager::handleBlockMessage(const std::string& peer_address, uint16_t peer_port,
                                       std::shared_ptr<BlockMessage> message) {
    if (!message) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Received BLOCK from " + peer_address + ":" + std::to_string(peer_port) + 
                 " - " + message->toString());
    
    // Deserialize block
    auto block = std::make_shared<core::Block>();
    if (!block->deserialize(message->getBlockData())) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize block from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    // Validate block
    if (!validateIncomingBlock(block)) {
        DEO_LOG_WARNING(NETWORKING, "Invalid block from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    stats_.blocks_received++;
    
    // Add to blockchain
    if (blockchain_ && blockchain_->addBlock(block)) {
        DEO_LOG_INFO(NETWORKING, "Added block from " + peer_address + ":" + std::to_string(peer_port));
        
        // Broadcast to other peers
        broadcastBlock(block);
        
        if (block_handler_) {
            block_handler_(block);
        }
    }
}

void NetworkManager::handleTxMessage(const std::string& peer_address, uint16_t peer_port,
                                    std::shared_ptr<TxMessage> message) {
    if (!message) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Received TX from " + peer_address + ":" + std::to_string(peer_port) + 
                 " - " + message->toString());
    
    // Deserialize transaction
    auto transaction = std::make_shared<core::Transaction>();
    if (!transaction->deserialize(message->getTxData())) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize transaction from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    // Validate transaction
    if (!validateIncomingTransaction(transaction)) {
        DEO_LOG_WARNING(NETWORKING, "Invalid transaction from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    stats_.transactions_received++;
    
    // Add to mempool
    if (blockchain_ && blockchain_->addTransaction(transaction)) {
        DEO_LOG_INFO(NETWORKING, "Added transaction from " + peer_address + ":" + std::to_string(peer_port));
        
        // Broadcast to other peers
        broadcastTransaction(transaction);
        
        if (transaction_handler_) {
            transaction_handler_(transaction);
        }
    }
}

void NetworkManager::handlePingMessage(const std::string& peer_address, uint16_t peer_port,
                                      std::shared_ptr<PingMessage> message) {
    if (!message) {
        return;
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Received PING from " + peer_address + ":" + std::to_string(peer_port) + 
                  " - " + message->toString());
    
    // Send PONG response
    PongMessage pong_msg(message->getNonce());
    auto message_data = pong_msg.serialize();
    peer_manager_->sendMessageToPeer(peer_address, peer_port, message_data);
}

void NetworkManager::handlePongMessage(const std::string& peer_address, uint16_t peer_port,
                                      std::shared_ptr<PongMessage> message) {
    if (!message) {
        return;
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Received PONG from " + peer_address + ":" + std::to_string(peer_port) + 
                  " - " + message->toString());
    
    // Update peer last seen
    auto peer = peer_manager_->getPeerConnection(peer_address, peer_port);
    if (peer) {
        peer->updateLastSeen();
    }
}

bool NetworkManager::sendHelloMessage(const std::string& address, uint16_t port) {
    // Generate random nonce
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    HelloMessage hello_msg(1, "Deo/1.0.0", 1, dis(gen));
    auto message_data = hello_msg.serialize();
    
    return peer_manager_->sendMessageToPeer(address, port, message_data);
}

bool NetworkManager::sendPingMessage(const std::string& address, uint16_t port) {
    // Generate random nonce
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    PingMessage ping_msg(dis(gen));
    auto message_data = ping_msg.serialize();
    
    return peer_manager_->sendMessageToPeer(address, port, message_data);
}

bool NetworkManager::validateIncomingBlock(std::shared_ptr<core::Block> block) {
    if (!block) {
        return false;
    }
    
    // Basic validation
    if (!block->verify()) {
        return false;
    }
    
    // Consensus validation
    if (consensus_engine_ && !consensus_engine_->validateBlock(block)) {
        return false;
    }
    
    return true;
}

bool NetworkManager::validateIncomingTransaction(std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        return false;
    }
    
    // Basic validation
    if (!transaction->verify()) {
        return false;
    }
    
    // Check for double spending
    // In a real implementation, you'd check the UTXO set
    
    return true;
}

void NetworkManager::updateStats(uint64_t bytes_sent, uint64_t bytes_received,
                                uint64_t messages_sent, uint64_t messages_received) {
    stats_.total_bytes_sent += bytes_sent;
    stats_.total_bytes_received += bytes_received;
    stats_.total_messages_sent += messages_sent;
    stats_.total_messages_received += messages_received;
}

} // namespace network
} // namespace deo
