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
    peer_manager_ = std::make_shared<PeerManager>();
    if (!peer_manager_->initialize()) {
        DEO_LOG_ERROR(NETWORKING, "Failed to initialize peer manager");
        return false;
    }
    
    // Note: PeerManager doesn't have setMessageHandler, setConnectionHandler, setDisconnectionHandler
    // These will be handled by the NetworkManager's own message processing
    
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
    // PeerManager doesn't have startPeerDiscovery method
    // peer_manager_->startPeerDiscovery(config_.bootstrap_nodes);
    
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
    
    // Stop peer discovery - PeerManager doesn't have stopPeerDiscovery method
    // peer_manager_->stopPeerDiscovery();
    
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
    // BlockMessage constructor expects std::shared_ptr<core::Block>, not std::vector<uint8_t>
    // BlockMessage block_msg(block_data);
    BlockMessage block_msg(block);
    auto message_data = block_msg.serialize();
    
    // Broadcast to all peers
    // PeerManager doesn't have broadcastMessage method
    // size_t sent_count = peer_manager_->broadcastMessage(message_data);
    size_t sent_count = 0; // TODO: Implement proper broadcasting
    
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
    // TxMessage constructor expects std::shared_ptr<core::Transaction>, not std::vector<uint8_t>
    // TxMessage tx_msg(tx_data);
    TxMessage tx_msg(transaction);
    auto message_data = tx_msg.serialize();
    
    // Broadcast to all peers
    // PeerManager doesn't have broadcastMessage method
    // size_t sent_count = peer_manager_->broadcastMessage(message_data);
    size_t sent_count = 0; // TODO: Implement proper broadcasting
    
    stats_.transactions_sent += sent_count;
    stats_.total_messages_sent += sent_count;
    stats_.total_bytes_sent += message_data.size() * sent_count;
    
    DEO_LOG_INFO(NETWORKING, "Broadcasted transaction to " + std::to_string(sent_count) + " peers");
    return sent_count;
}

bool NetworkManager::sendBlockToPeer(const std::string& /* address */, uint16_t /* port */, 
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
    // BlockMessage constructor expects std::shared_ptr<core::Block>, not std::vector<uint8_t>
    // BlockMessage block_msg(block_data);
    BlockMessage block_msg(block);
    auto message_data = block_msg.serialize();
    
    // Send to peer
    // PeerManager doesn't have sendMessageToPeer method
    // bool success = peer_manager_->sendMessageToPeer(address, port, message_data);
    bool success = false; // TODO: Implement proper peer messaging
    
    if (success) {
        stats_.blocks_sent++;
        stats_.total_messages_sent++;
        stats_.total_bytes_sent += message_data.size();
    }
    
    return success;
}

bool NetworkManager::sendTransactionToPeer(const std::string& /* address */, uint16_t /* port */, 
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
    // TxMessage constructor expects std::shared_ptr<core::Transaction>, not std::vector<uint8_t>
    // TxMessage tx_msg(tx_data);
    TxMessage tx_msg(transaction);
    auto message_data = tx_msg.serialize();
    
    // Send to peer
    // PeerManager doesn't have sendMessageToPeer method
    // bool success = peer_manager_->sendMessageToPeer(address, port, message_data);
    bool success = false; // TODO: Implement proper peer messaging
    
    if (success) {
        stats_.transactions_sent++;
        stats_.total_messages_sent++;
        stats_.total_bytes_sent += message_data.size();
    }
    
    return success;
}

bool NetworkManager::requestBlocks(const std::string& /* address */, uint16_t /* port */, 
                                  const std::vector<std::string>& block_hashes) {
    // GetDataMessage constructor expects std::vector<std::string>, not std::vector<InventoryItem>
    // std::vector<InventoryItem> items;
    // for (const auto& hash : block_hashes) {
    //     items.emplace_back(2, hash); // 2 = block
    // }
    
    GetDataMessage getdata_msg(block_hashes);
    auto message_data = getdata_msg.serialize();
    
    // PeerManager doesn't have sendMessageToPeer method
    // return peer_manager_->sendMessageToPeer(address, port, message_data);
    return false; // TODO: Implement proper peer messaging
}

bool NetworkManager::requestTransactions(const std::string& /* address */, uint16_t /* port */,
                                        const std::vector<std::string>& tx_hashes) {
    // GetDataMessage constructor expects std::vector<std::string>, not std::vector<InventoryItem>
    // std::vector<InventoryItem> items;
    // for (const auto& hash : tx_hashes) {
    //     items.emplace_back(1, hash); // 1 = transaction
    // }
    
    GetDataMessage getdata_msg(tx_hashes);
    auto message_data = getdata_msg.serialize();
    
    // PeerManager doesn't have sendMessageToPeer method
    // return peer_manager_->sendMessageToPeer(address, port, message_data);
    return false; // TODO: Implement proper peer messaging
}

bool NetworkManager::connectToPeer(const std::string& address, uint16_t port) {
    stats_.connection_attempts++;
    // PeerManager doesn't have connectPeer method
    // return peer_manager_->connectPeer(address, port);
    return peer_manager_->addPeer(address, port);
}

bool NetworkManager::disconnectFromPeer(const std::string& address, uint16_t port) {
    // PeerManager doesn't have disconnectPeer method
    // return peer_manager_->disconnectPeer(address, port);
    peer_manager_->removePeer(address, port);
    return true;
}

std::vector<PeerInfo> NetworkManager::getConnectedPeers() const {
    // PeerManager::getConnectedPeers() returns std::vector<std::string>
    // but we need std::vector<PeerInfo>
    auto peer_addresses = peer_manager_->getConnectedPeers();
    std::vector<PeerInfo> peers;
    peers.reserve(peer_addresses.size());
    
    for (const auto& address : peer_addresses) {
        // Extract port from address if it contains port
        size_t colon_pos = address.find(':');
        std::string peer_address = address;
        uint16_t peer_port = 8333; // Default port
        
        if (colon_pos != std::string::npos) {
            peer_address = address.substr(0, colon_pos);
            peer_port = static_cast<uint16_t>(std::stoi(address.substr(colon_pos + 1)));
        }
        
        PeerInfo info;
        info.address = peer_address;
        info.port = peer_port;
        info.connected = true;
        peers.push_back(info);
    }
    
    return peers;
}

NetworkStats NetworkManager::getNetworkStats() const {
    return stats_;
}

size_t NetworkManager::getPeerCount() const {
    // PeerManager doesn't have getPeerCount method
    // return peer_manager_->getPeerCount();
    return peer_manager_->getConnectedPeers().size();
}

bool NetworkManager::isPeerConnected(const std::string& address, uint16_t port) const {
    // PeerManager doesn't have getPeerConnection method
    // auto peer = peer_manager_->getPeerConnection(address, port);
    // return peer && peer->isConnected();
    return peer_manager_->isPeerConnected(address, port);
}

void NetworkManager::banPeer(const std::string& address, uint16_t port, const std::string& /* reason */) {
    // PeerManager::banPeer expects std::chrono::seconds, not std::string
    peer_manager_->banPeer(address, port, std::chrono::seconds(3600)); // Ban for 1 hour
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
    // MessageFactory::createMessage expects MessageType, not std::vector<uint8_t>
    // We need to parse the message type from the data first
    if (message_data.empty()) {
        DEO_LOG_WARNING(NETWORKING, "Empty message from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    // Parse message type from first byte
    MessageType message_type = static_cast<MessageType>(message_data[0]);
    auto message = MessageFactory::createMessage(message_type);
    if (!message) {
        DEO_LOG_WARNING(NETWORKING, "Failed to create message of type " + std::to_string(static_cast<int>(message_type)) + " from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    // Deserialize the message data
    if (!message->deserialize(message_data)) {
        DEO_LOG_WARNING(NETWORKING, "Failed to deserialize message from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    if (!message->validate()) {
        DEO_LOG_WARNING(NETWORKING, "Invalid message from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    stats_.total_messages_received++;
    stats_.total_bytes_received += message_data.size();
    
    // TODO: Implement proper message handling
    // The NetworkMessage classes are missing many required methods
    DEO_LOG_DEBUG(NETWORKING, "Received message of type " + std::to_string(static_cast<int>(message->getType())) + " from " + peer_address + ":" + std::to_string(peer_port));
}

void NetworkManager::handleHelloMessage(const std::string& peer_address, uint16_t peer_port,
                                       std::shared_ptr<HelloMessage> message) {
    if (!message) {
        DEO_LOG_ERROR(NETWORKING, "Null HELLO message from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Received HELLO message from " + peer_address + ":" + std::to_string(peer_port));
    
    // Add peer to our peer list
    if (peer_manager_) {
        peer_manager_->addPeer(peer_address, peer_port);
    }
    
    // Send our own HELLO message back
    auto response = std::make_shared<HelloMessage>();
    response->node_id_ = "deo_node";
    response->user_agent_ = "DeoBlockchain/1.0.0";
    response->capabilities_ = {"blockchain", "consensus", "networking"};
    
    sendHelloMessage(peer_address, peer_port);
}

void NetworkManager::handleInvMessage(const std::string& peer_address, uint16_t peer_port,
                                     std::shared_ptr<InvMessage> message) {
    if (!message) {
        DEO_LOG_ERROR(NETWORKING, "Null INV message from " + peer_address + ":" + std::to_string(peer_port));
        return;
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Received INV message from " + peer_address + ":" + std::to_string(peer_port));
    
    // Process inventory items
    auto inventory = message->inventory_;
    for (const auto& item_hash : inventory) {
        DEO_LOG_DEBUG(NETWORKING, "Inventory item: " + item_hash);
        
        // Request the item if we don't have it
        if (!hasItem(item_hash)) {
            requestItem(peer_address, peer_port, item_hash);
        }
    }
}

void NetworkManager::handleGetDataMessage(const std::string& peer_address, uint16_t peer_port,
                                         std::shared_ptr<GetDataMessage> /* message */) {
    // TODO: Implement proper GETDATA message handling
    // The NetworkMessage classes are missing many required methods
    DEO_LOG_DEBUG(NETWORKING, "Received GETDATA message from " + peer_address + ":" + std::to_string(peer_port));
}

void NetworkManager::handleBlockMessage(const std::string& peer_address, uint16_t peer_port,
                                       std::shared_ptr<BlockMessage> /* message */) {
    // TODO: Implement proper BLOCK message handling
    // The NetworkMessage classes are missing many required methods
    DEO_LOG_DEBUG(NETWORKING, "Received BLOCK message from " + peer_address + ":" + std::to_string(peer_port));
}

void NetworkManager::handleTxMessage(const std::string& peer_address, uint16_t peer_port,
                                    std::shared_ptr<TxMessage> /* message */) {
    // TODO: Implement proper TX message handling
    // The NetworkMessage classes are missing many required methods
    DEO_LOG_DEBUG(NETWORKING, "Received TX message from " + peer_address + ":" + std::to_string(peer_port));
}

void NetworkManager::handlePingMessage(const std::string& peer_address, uint16_t peer_port,
                                      std::shared_ptr<PingMessage> /* message */) {
    // TODO: Implement proper PING message handling
    // The NetworkMessage classes are missing many required methods
    DEO_LOG_DEBUG(NETWORKING, "Received PING message from " + peer_address + ":" + std::to_string(peer_port));
}

void NetworkManager::handlePongMessage(const std::string& peer_address, uint16_t peer_port,
                                      std::shared_ptr<PongMessage> /* message */) {
    // TODO: Implement proper PONG message handling
    // The NetworkMessage classes are missing many required methods
    DEO_LOG_DEBUG(NETWORKING, "Received PONG message from " + peer_address + ":" + std::to_string(peer_port));
}

bool NetworkManager::sendHelloMessage(const std::string& /* address */, uint16_t /* port */) {
    // Generate random nonce
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    // HelloMessage constructor expects (node_id, version, capabilities)
    // HelloMessage hello_msg(1, "Deo/1.0.0", 1, dis(gen));
    std::vector<std::string> capabilities = {"block", "transaction"};
    HelloMessage hello_msg("node_" + std::to_string(dis(gen)), 1, capabilities);
    auto message_data = hello_msg.serialize();
    
    // PeerManager doesn't have sendMessageToPeer method
    // return peer_manager_->sendMessageToPeer(address, port, message_data);
    return false; // TODO: Implement proper peer messaging
}

bool NetworkManager::sendPingMessage(const std::string& /* address */, uint16_t /* port */) {
    // Generate random nonce
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    PingMessage ping_msg(dis(gen));
    auto message_data = ping_msg.serialize();
    
    // PeerManager doesn't have sendMessageToPeer method
    // return peer_manager_->sendMessageToPeer(address, port, message_data);
    return false; // TODO: Implement proper peer messaging
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

bool NetworkManager::hasItem(const std::string& hash) const {
    // Check if we have the item in our blockchain or mempool
    if (blockchain_) {
        auto block = blockchain_->getBlock(hash);
        if (block) {
            return true;
        }
        
        auto tx = blockchain_->getTransaction(hash);
        if (tx) {
            return true;
        }
    }
    
    return false;
}

void NetworkManager::requestItem(const std::string& peer_address, uint16_t peer_port, const std::string& item_hash) {
    auto getdata_message = std::make_shared<GetDataMessage>();
    getdata_message->items_.push_back(item_hash);
    
    // Note: Using sendTransactionToPeer as a placeholder for sending GETDATA
    // In a real implementation, we would have a specific method for this
    DEO_LOG_DEBUG(NETWORKING, "GETDATA message prepared for " + peer_address + ":" + std::to_string(peer_port));
    
    DEO_LOG_DEBUG(NETWORKING, "Requested item " + item_hash + " from " + peer_address + ":" + std::to_string(peer_port));
}

} // namespace network
} // namespace deo
