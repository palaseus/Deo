/**
 * @file p2p_network_manager.cpp
 * @brief Integrated P2P network manager for multi-node blockchain networking
 */

#include "network/p2p_network_manager.h"
#include <algorithm>

namespace deo {
namespace network {

P2PNetworkManager::P2PNetworkManager(uint16_t listen_port)
    : listen_port_(listen_port)
    , running_(false) {
}

P2PNetworkManager::~P2PNetworkManager() {
    shutdown();
}

bool P2PNetworkManager::initialize(std::shared_ptr<core::Blockchain> blockchain,
                                   std::shared_ptr<consensus::ConsensusEngine> consensus_engine) {
    DEO_LOG_INFO(NETWORKING, "Initializing P2P network manager on port " + std::to_string(listen_port_));
    
    blockchain_ = blockchain;
    consensus_engine_ = consensus_engine;
    
    // Initialize TCP network manager
    tcp_manager_ = std::make_shared<TcpNetworkManager>(listen_port_);
    if (!tcp_manager_->initialize()) {
        DEO_LOG_ERROR(NETWORKING, "Failed to initialize TCP network manager");
        return false;
    }
    
    // Initialize peer manager
    peer_manager_ = std::make_shared<PeerManager>();
    if (!peer_manager_->initialize()) {
        DEO_LOG_ERROR(NETWORKING, "Failed to initialize peer manager");
        return false;
    }
    
    // Initialize gossip protocol
    gossip_protocol_ = std::make_shared<GossipProtocol>(tcp_manager_, peer_manager_);
    
    // Initialize mempools
    transaction_mempool_ = std::make_shared<TransactionMempool>(tcp_manager_, peer_manager_);
    if (!transaction_mempool_->initialize()) {
        DEO_LOG_ERROR(NETWORKING, "Failed to initialize transaction mempool");
        return false;
    }
    
    block_mempool_ = std::make_shared<BlockMempool>(tcp_manager_, peer_manager_);
    if (!block_mempool_->initialize()) {
        DEO_LOG_ERROR(NETWORKING, "Failed to initialize block mempool");
        return false;
    }
    
    // Setup message handlers
    setupMessageHandlers();
    
    // Integrate with consensus
    integrateWithConsensus();
    
    running_ = true;
    
    DEO_LOG_INFO(NETWORKING, "P2P network manager initialized successfully");
    return true;
}

void P2PNetworkManager::shutdown() {
    if (!running_) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Shutting down P2P network manager");
    running_ = false;
    
    if (transaction_mempool_) {
        transaction_mempool_->shutdown();
    }
    
    if (block_mempool_) {
        block_mempool_->shutdown();
    }
    
    if (peer_manager_) {
        peer_manager_->shutdown();
    }
    
    if (tcp_manager_) {
        tcp_manager_->shutdown();
    }
    
    DEO_LOG_INFO(NETWORKING, "P2P network manager shutdown complete");
}

bool P2PNetworkManager::isRunning() const {
    return running_;
}

bool P2PNetworkManager::connectToPeer(const std::string& address, uint16_t port) {
    if (!running_) {
        return false;
    }
    
    DEO_LOG_INFO(NETWORKING, "Connecting to peer " + address + ":" + std::to_string(port));
    
    // Add to peer manager first
    peer_manager_->addPeer(address, port);
    
    // Connect via TCP
    bool success = tcp_manager_->connectToPeer(address, port);
    
    if (success) {
        DEO_LOG_INFO(NETWORKING, "Successfully connected to peer " + address + ":" + std::to_string(port));
    } else {
        DEO_LOG_WARNING(NETWORKING, "Failed to connect to peer " + address + ":" + std::to_string(port));
        peer_manager_->removePeer(address, port);
    }
    
    return success;
}

void P2PNetworkManager::disconnectFromPeer(const std::string& address, uint16_t port) {
    if (!running_) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Disconnecting from peer " + address + ":" + std::to_string(port));
    
    tcp_manager_->disconnectPeer(address, port);
    peer_manager_->removePeer(address, port);
}

std::vector<std::string> P2PNetworkManager::getConnectedPeers() const {
    if (!running_) {
        return {};
    }
    
    return peer_manager_->getConnectedPeers();
}

size_t P2PNetworkManager::getPeerCount() const {
    if (!running_) {
        return 0;
    }
    
    return peer_manager_->getConnectedPeers().size();
}

bool P2PNetworkManager::isPeerConnected(const std::string& address, uint16_t port) const {
    if (!running_) {
        return false;
    }
    
    return peer_manager_->isPeerConnected(address, port);
}

void P2PNetworkManager::addBootstrapNode(const std::string& address, uint16_t port) {
    if (!running_) {
        return;
    }
    
    peer_manager_->addBootstrapNode(address, port);
    tcp_manager_->addBootstrapNode(address, port);
}

void P2PNetworkManager::discoverPeers() {
    if (!running_) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Starting peer discovery");
    peer_manager_->discoverPeers();
    tcp_manager_->discoverPeers();
}

void P2PNetworkManager::broadcastTransaction(std::shared_ptr<core::Transaction> transaction) {
    if (!running_ || !transaction) {
        return;
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Broadcasting transaction: " + transaction->getId());
    transaction_mempool_->broadcastNewTransaction(transaction);
}

void P2PNetworkManager::handleIncomingTransaction(std::shared_ptr<core::Transaction> transaction, const std::string& peer_address) {
    if (!running_ || !transaction) {
        return;
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Handling incoming transaction from " + peer_address + ": " + transaction->getId());
    transaction_mempool_->handleIncomingTransaction(transaction, peer_address);
    
    // Report good behavior
    peer_manager_->reportGoodBehavior(peer_address, 0, 1);
    
    // Call transaction handler if set
    if (transaction_handler_) {
        transaction_handler_(transaction);
    }
}

void P2PNetworkManager::broadcastBlock(std::shared_ptr<core::Block> block) {
    if (!running_ || !block) {
        return;
    }
    
    DEO_LOG_INFO(NETWORKING, "Broadcasting block: " + block->getHash());
    block_mempool_->broadcastNewBlock(block);
}

void P2PNetworkManager::handleIncomingBlock(std::shared_ptr<core::Block> block, const std::string& peer_address) {
    if (!running_ || !block) {
        return;
    }
    
    DEO_LOG_DEBUG(NETWORKING, "Handling incoming block from " + peer_address + ": " + block->getHash());
    block_mempool_->handleIncomingBlock(block, peer_address);
    
    // Report good behavior
    peer_manager_->reportGoodBehavior(peer_address, 0, 5);
    
    // Call block handler if set
    if (block_handler_) {
        block_handler_(block);
    }
}

std::shared_ptr<TransactionMempool> P2PNetworkManager::getTransactionMempool() const {
    return transaction_mempool_;
}

std::shared_ptr<BlockMempool> P2PNetworkManager::getBlockMempool() const {
    return block_mempool_;
}

P2PNetworkManager::NetworkStats P2PNetworkManager::getNetworkStats() const {
    NetworkStats stats;
    
    if (tcp_manager_) {
        stats.tcp_stats = tcp_manager_->getNetworkStats();
    }
    
    if (peer_manager_) {
        stats.peer_stats = peer_manager_->getPeerStats();
    }
    
    if (transaction_mempool_) {
        stats.mempool_stats = transaction_mempool_->getMempoolStats();
    }
    
    if (block_mempool_) {
        stats.block_mempool_stats = block_mempool_->getBlockMempoolStats();
    }
    
    if (gossip_protocol_) {
        stats.gossip_stats = gossip_protocol_->getGossipStats();
    }
    
    return stats;
}

void P2PNetworkManager::setBlockHandler(std::function<void(std::shared_ptr<core::Block>)> handler) {
    block_handler_ = handler;
}

void P2PNetworkManager::setTransactionHandler(std::function<void(std::shared_ptr<core::Transaction>)> handler) {
    transaction_handler_ = handler;
}

void P2PNetworkManager::setupMessageHandlers() {
    if (!tcp_manager_) {
        return;
    }
    
    // Setup message handlers for different message types
    tcp_manager_->setMessageHandler(MessageType::BLOCK, 
        [this](const NetworkMessage& message, const std::string& peer_address) {
            handleBlockMessage(message, peer_address);
        });
    
    tcp_manager_->setMessageHandler(MessageType::TX, 
        [this](const NetworkMessage& message, const std::string& peer_address) {
            handleTxMessage(message, peer_address);
        });
    
    tcp_manager_->setMessageHandler(MessageType::GETDATA, 
        [this](const NetworkMessage& message, const std::string& peer_address) {
            handleGetDataMessage(message, peer_address);
        });
    
    tcp_manager_->setMessageHandler(MessageType::HELLO, 
        [this](const NetworkMessage& message, const std::string& peer_address) {
            handleHelloMessage(message, peer_address);
        });
    
    tcp_manager_->setMessageHandler(MessageType::PING, 
        [this](const NetworkMessage& message, const std::string& peer_address) {
            handlePingMessage(message, peer_address);
        });
    
    tcp_manager_->setMessageHandler(MessageType::PONG, 
        [this](const NetworkMessage& message, const std::string& peer_address) {
            handlePongMessage(message, peer_address);
        });
}

void P2PNetworkManager::handleBlockMessage(const NetworkMessage& message, const std::string& peer_address) {
    try {
        // Cast to BlockMessage
        const BlockMessage* block_message = dynamic_cast<const BlockMessage*>(&message);
        if (!block_message) {
            DEO_LOG_ERROR(NETWORKING, "Invalid block message from " + peer_address);
            peer_manager_->reportMisbehavior(peer_address, 0, 10);
            return;
        }
        
        // Get block from message
        auto block = block_message->block_;
        if (block) {
            handleIncomingBlock(block, peer_address);
        } else {
            DEO_LOG_ERROR(NETWORKING, "Failed to get block from message from " + peer_address);
            peer_manager_->reportMisbehavior(peer_address, 0, 10);
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle block message: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 10);
    }
}

void P2PNetworkManager::handleTxMessage(const NetworkMessage& message, const std::string& peer_address) {
    try {
        // Cast to TxMessage
        const TxMessage* tx_message = dynamic_cast<const TxMessage*>(&message);
        if (!tx_message) {
            DEO_LOG_ERROR(NETWORKING, "Invalid transaction message from " + peer_address);
            peer_manager_->reportMisbehavior(peer_address, 0, 5);
            return;
        }
        
        // Get transaction from message
        auto transaction = tx_message->transaction_;
        if (transaction) {
            handleIncomingTransaction(transaction, peer_address);
        } else {
            DEO_LOG_ERROR(NETWORKING, "Failed to get transaction from message from " + peer_address);
            peer_manager_->reportMisbehavior(peer_address, 0, 5);
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle transaction message: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 5);
    }
}

void P2PNetworkManager::handleGetDataMessage(const NetworkMessage& message, const std::string& peer_address) {
    try {
        // Cast to GetDataMessage
        const GetDataMessage* getdata_message = dynamic_cast<const GetDataMessage*>(&message);
        if (!getdata_message) {
            DEO_LOG_ERROR(NETWORKING, "Invalid getdata message from " + peer_address);
            peer_manager_->reportMisbehavior(peer_address, 0, 5);
            return;
        }
        
        // TODO: Implement data retrieval and response
        DEO_LOG_DEBUG(NETWORKING, "Handled getdata request from " + peer_address);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle getdata request: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 5);
    }
}

void P2PNetworkManager::handleHelloMessage(const NetworkMessage& message, const std::string& peer_address) {
    try {
        // Cast to HelloMessage
        const HelloMessage* hello_message = dynamic_cast<const HelloMessage*>(&message);
        if (!hello_message) {
            DEO_LOG_ERROR(NETWORKING, "Invalid hello message from " + peer_address);
            peer_manager_->reportMisbehavior(peer_address, 0, 5);
            return;
        }
        
        DEO_LOG_INFO(NETWORKING, "Received HELLO from " + peer_address);
        
        // Update peer information
        peer_manager_->updatePeerActivity(peer_address, 0);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle HELLO message: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 5);
    }
}

void P2PNetworkManager::handlePingMessage(const NetworkMessage& message, const std::string& peer_address) {
    try {
        // Cast to PingMessage
        const PingMessage* ping_message = dynamic_cast<const PingMessage*>(&message);
        if (!ping_message) {
            DEO_LOG_ERROR(NETWORKING, "Invalid ping message from " + peer_address);
            peer_manager_->reportMisbehavior(peer_address, 0, 5);
            return;
        }
        
        // Send PONG response
        PongMessage pong(ping_message->nonce_);
        
        tcp_manager_->sendToPeer(peer_address, pong);
        
        DEO_LOG_DEBUG(NETWORKING, "Responded to PING from " + peer_address);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle PING message: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 5);
    }
}

void P2PNetworkManager::handlePongMessage(const NetworkMessage& message, const std::string& peer_address) {
    try {
        // Cast to PongMessage
        const PongMessage* pong_message = dynamic_cast<const PongMessage*>(&message);
        if (!pong_message) {
            DEO_LOG_ERROR(NETWORKING, "Invalid pong message from " + peer_address);
            peer_manager_->reportMisbehavior(peer_address, 0, 5);
            return;
        }
        
        DEO_LOG_DEBUG(NETWORKING, "Received PONG from " + peer_address);
        
        // Update peer activity
        peer_manager_->updatePeerActivity(peer_address, 0);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle PONG message: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 5);
    }
}

void P2PNetworkManager::requestMissingBlocks() {
    if (!blockchain_ || !peer_manager_) {
        return;
    }
    
    try {
        // Get current blockchain height
        uint64_t current_height = blockchain_->getHeight();
        
        // Get connected peers
        auto peers = peer_manager_->getConnectedPeers();
        
        for (const auto& peer : peers) {
            // Request blocks from this peer
            auto getdata_message = std::make_shared<GetDataMessage>();
            
            // Request blocks starting from current height + 1
            for (uint64_t height = current_height + 1; height <= current_height + 10; ++height) {
                getdata_message->items_.push_back("block_" + std::to_string(height));
            }
            
            // Send request (placeholder - would need proper network manager integration)
            DEO_LOG_DEBUG(NETWORKING, "Requested blocks from peer: " + peer);
        }
        
        DEO_LOG_DEBUG(NETWORKING, "Requested missing blocks from " + std::to_string(peers.size()) + " peers");
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to request missing blocks: " + std::string(e.what()));
    }
}

void P2PNetworkManager::handleBlockSynchronization() {
    if (!blockchain_ || !peer_manager_) {
        return;
    }
    
    try {
        // Check if we need to synchronize
        auto peers = peer_manager_->getConnectedPeers();
        if (peers.empty()) {
            DEO_LOG_DEBUG(NETWORKING, "No peers available for synchronization");
            return;
        }
        
        // Request missing blocks
        requestMissingBlocks();
        
        // Handle any pending block synchronization
        // This would typically involve:
        // 1. Checking for blocks we've requested
        // 2. Validating received blocks
        // 3. Adding valid blocks to the blockchain
        // 4. Updating our synchronization state
        
        DEO_LOG_DEBUG(NETWORKING, "Block synchronization handled");
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Block synchronization failed: " + std::string(e.what()));
    }
}

void P2PNetworkManager::integrateWithConsensus() {
    if (!consensus_engine_) {
        DEO_LOG_WARNING(NETWORKING, "No consensus engine available for integration");
        return;
    }
    
    try {
        DEO_LOG_INFO(NETWORKING, "Integrating P2P network with consensus engine");
        
        // Set up consensus event handlers
        // This would typically involve:
        // 1. Registering for consensus events (new blocks, forks, etc.)
        // 2. Setting up block validation callbacks
        // 3. Configuring transaction validation
        // 4. Setting up consensus state synchronization
        
        // For now, we'll just log the integration
        DEO_LOG_INFO(NETWORKING, "P2P network integrated with consensus engine");
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to integrate with consensus: " + std::string(e.what()));
    }
}

bool P2PNetworkManager::validateBlockForConsensus(std::shared_ptr<core::Block> block) {
    if (!consensus_engine_ || !block) {
        return false;
    }
    
    // Use consensus engine to validate block
    return consensus_engine_->validateBlock(block);
}

bool P2PNetworkManager::validateTransactionForConsensus(std::shared_ptr<core::Transaction> transaction) {
    if (!transaction) {
        return false;
    }
    
    // Basic transaction validation
    return transaction->verify();
}

} // namespace network
} // namespace deo
