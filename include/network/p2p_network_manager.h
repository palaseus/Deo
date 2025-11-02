/**
 * @file p2p_network_manager.h
 * @brief Integrated P2P network manager for multi-node blockchain networking
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

#include "tcp_network.h"
#include "peer_manager.h"
#include "transaction_mempool.h"
#include "network_messages.h"
#include "core/blockchain.h"
#include "core/transaction.h"
#include "core/block.h"
#include "consensus/consensus_engine.h"
#include "utils/logger.h"

namespace deo {
namespace network {

/**
 * @brief Integrated P2P network manager for blockchain nodes
 */
class P2PNetworkManager {
public:
    P2PNetworkManager(uint16_t listen_port = 8333);
    ~P2PNetworkManager();
    
    // Initialization and lifecycle
    bool initialize(std::shared_ptr<core::Blockchain> blockchain,
                    std::shared_ptr<consensus::ConsensusEngine> consensus_engine);
    void shutdown();
    bool isRunning() const;
    
    // Peer management
    bool connectToPeer(const std::string& address, uint16_t port);
    void disconnectFromPeer(const std::string& address, uint16_t port);
    std::vector<std::string> getConnectedPeers() const;
    size_t getPeerCount() const;
    bool isPeerConnected(const std::string& address, uint16_t port) const;
    
    // Bootstrap and discovery
    void addBootstrapNode(const std::string& address, uint16_t port);
    void discoverPeers();
    
    // Transaction broadcasting
    void broadcastTransaction(std::shared_ptr<core::Transaction> transaction);
    void handleIncomingTransaction(std::shared_ptr<core::Transaction> transaction, const std::string& peer_address);
    
    // Block broadcasting
    void broadcastBlock(std::shared_ptr<core::Block> block);
    void handleIncomingBlock(std::shared_ptr<core::Block> block, const std::string& peer_address);
    
    // Mempool access
    std::shared_ptr<TransactionMempool> getTransactionMempool() const;
    std::shared_ptr<BlockMempool> getBlockMempool() const;
    
    // Peer manager access
    std::shared_ptr<PeerManager> getPeerManager() const;
    
    // Gossip protocol access
    std::shared_ptr<GossipProtocol> getGossipProtocol() const;
    
    // Network statistics
    struct NetworkStats {
        TcpNetworkManager::NetworkStats tcp_stats;
        PeerManager::PeerStats peer_stats;
        TransactionMempool::MempoolStats mempool_stats;
        BlockMempool::BlockMempoolStats block_mempool_stats;
        GossipProtocol::GossipStats gossip_stats;
    };
    
    NetworkStats getNetworkStats() const;
    
    // Event handlers
    void setBlockHandler(std::function<void(std::shared_ptr<core::Block>)> handler);
    void setTransactionHandler(std::function<void(std::shared_ptr<core::Transaction>)> handler);

private:
    uint16_t listen_port_;
    std::atomic<bool> running_;
    
    // Core components
    std::shared_ptr<TcpNetworkManager> tcp_manager_;
    std::shared_ptr<PeerManager> peer_manager_;
    std::shared_ptr<GossipProtocol> gossip_protocol_;
    std::shared_ptr<TransactionMempool> transaction_mempool_;
    std::shared_ptr<BlockMempool> block_mempool_;
    
    // Blockchain integration
    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<consensus::ConsensusEngine> consensus_engine_;
    
    // Event handlers
    std::function<void(std::shared_ptr<core::Block>)> block_handler_;
    std::function<void(std::shared_ptr<core::Transaction>)> transaction_handler_;
    
    // Message handlers
    void setupMessageHandlers();
    void handleBlockMessage(const NetworkMessage& message, const std::string& peer_address);
    void handleTxMessage(const NetworkMessage& message, const std::string& peer_address);
    void handleGetDataMessage(const NetworkMessage& message, const std::string& peer_address);
    void handleHelloMessage(const NetworkMessage& message, const std::string& peer_address);
    void handlePingMessage(const NetworkMessage& message, const std::string& peer_address);
    void handlePongMessage(const NetworkMessage& message, const std::string& peer_address);
    void handleGetAddrMessage(const NetworkMessage& message, const std::string& peer_address);
    void handleAddrMessage(const NetworkMessage& message, const std::string& peer_address);
    
    // Block synchronization
    void requestMissingBlocks();
    void handleBlockSynchronization();
    
    // Consensus integration
    void integrateWithConsensus();
    bool validateBlockForConsensus(std::shared_ptr<core::Block> block);
    bool validateTransactionForConsensus(std::shared_ptr<core::Transaction> transaction);
};

} // namespace network
} // namespace deo
