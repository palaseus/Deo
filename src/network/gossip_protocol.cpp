/**
 * @file gossip_protocol.cpp
 * @brief Gossip protocol implementation for P2P message propagation
 */

#include "network/peer_manager.h"
#include <algorithm>

namespace deo {
namespace network {

GossipProtocol::GossipProtocol(std::shared_ptr<TcpNetworkManager> network_manager,
                               std::shared_ptr<PeerManager> peer_manager)
    : network_manager_(network_manager)
    , peer_manager_(peer_manager) {
    stats_.transactions_broadcasted = 0;
    stats_.blocks_broadcasted = 0;
    stats_.messages_propagated = 0;
    stats_.duplicate_messages_filtered = 0;
    stats_.start_time = std::chrono::steady_clock::now();
}

GossipProtocol::~GossipProtocol() {
}

void GossipProtocol::broadcastTransaction(const std::string& /* transaction_data */) {
    // Create transaction message using the correct message type
    auto tx_message = std::make_unique<TxMessage>();
    // Note: We would need to parse transaction_data and create a Transaction object
    // For now, we'll just log the broadcast attempt
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.transactions_broadcasted++;
    
    DEO_LOG_DEBUG(NETWORKING, "Transaction broadcast requested (implementation pending)");
}

void GossipProtocol::broadcastBlock(const std::string& /* block_data */) {
    // Create block message using the correct message type
    auto block_message = std::make_unique<BlockMessage>();
    // Note: We would need to parse block_data and create a Block object
    // For now, we'll just log the broadcast attempt
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.blocks_broadcasted++;
    
    DEO_LOG_DEBUG(NETWORKING, "Block broadcast requested (implementation pending)");
}

void GossipProtocol::broadcastMessage(const NetworkMessage& message) {
    std::string message_hash = calculateMessageHash(message);
    
    auto best_peers = peer_manager_->getBestPeers(8);
    
    for (const auto& peer_key : best_peers) {
        if (shouldPropagateMessage(message_hash, peer_key)) {
            size_t colon_pos = peer_key.find(':');
            if (colon_pos != std::string::npos) {
                std::string address = peer_key.substr(0, colon_pos);
                network_manager_->sendToPeer(address, message);
                recordMessagePropagation(message_hash, peer_key);
            }
        }
    }
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_propagated++;
}

void GossipProtocol::handleNewTransaction(const NetworkMessage& message, const std::string& peer_address) {
    try {
        // Cast to TxMessage to access transaction data
        const TxMessage* tx_message = dynamic_cast<const TxMessage*>(&message);
        if (tx_message) {
            std::string message_hash = calculateMessageHash(message);
            if (shouldPropagateMessage(message_hash, peer_address)) {
                broadcastMessage(message);
                recordMessagePropagation(message_hash, peer_address);
            }
        }
        
        DEO_LOG_DEBUG(NETWORKING, "Handled new transaction from " + peer_address);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle new transaction: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 10);
    }
}

void GossipProtocol::handleNewBlock(const NetworkMessage& message, const std::string& peer_address) {
    try {
        // Cast to BlockMessage to access block data
        const BlockMessage* block_message = dynamic_cast<const BlockMessage*>(&message);
        if (block_message) {
            std::string message_hash = calculateMessageHash(message);
            if (shouldPropagateMessage(message_hash, peer_address)) {
                broadcastMessage(message);
                recordMessagePropagation(message_hash, peer_address);
            }
        }
        
        DEO_LOG_DEBUG(NETWORKING, "Handled new block from " + peer_address);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle new block: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 20);
    }
}

void GossipProtocol::handleRequestBlock(const NetworkMessage& /* message */, const std::string& peer_address) {
    try {
        // Cast to GetDataMessage to access request data
        // const GetDataMessage* request = dynamic_cast<const GetDataMessage*>(&message);
        
        DEO_LOG_DEBUG(NETWORKING, "Handled block request from " + peer_address);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle block request: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 5);
    }
}

void GossipProtocol::handleResponseBlock(const NetworkMessage& /* message */, const std::string& peer_address) {
    try {
        // Cast to BlockMessage to access response data
        // const BlockMessage* response = dynamic_cast<const BlockMessage*>(&message);
        
        DEO_LOG_DEBUG(NETWORKING, "Handled block response from " + peer_address);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to handle block response: " + std::string(e.what()));
        peer_manager_->reportMisbehavior(peer_address, 0, 5);
    }
}

bool GossipProtocol::shouldPropagateMessage(const std::string& message_hash, const std::string& peer_address) {
    std::lock_guard<std::mutex> lock(seen_messages_mutex_);
    
    auto it = seen_messages_.find(message_hash);
    if (it != seen_messages_.end()) {
        if (it->second.find(peer_address) != it->second.end()) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.duplicate_messages_filtered++;
            return false;
        }
    }
    
    return true;
}

void GossipProtocol::recordMessagePropagation(const std::string& message_hash, const std::string& peer_address) {
    std::lock_guard<std::mutex> lock(seen_messages_mutex_);
    
    seen_messages_[message_hash].insert(peer_address);
    message_timestamps_[message_hash] = std::chrono::steady_clock::now();
    
    if (seen_messages_.size() > 10000) {
        cleanupOldMessages();
    }
}

GossipProtocol::GossipStats GossipProtocol::getGossipStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void GossipProtocol::cleanupOldMessages() {
    auto now = std::chrono::steady_clock::now();
    auto cleanup_threshold = std::chrono::hours(1);
    
    auto it = message_timestamps_.begin();
    while (it != message_timestamps_.end()) {
        if (now - it->second > cleanup_threshold) {
            seen_messages_.erase(it->first);
            it = message_timestamps_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string GossipProtocol::calculateMessageHash(const NetworkMessage& message) const {
    // Use a simple hash based on message type and current time
    std::string data = std::to_string(static_cast<int>(message.getType())) + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return std::to_string(std::hash<std::string>{}(data));
}

} // namespace network
} // namespace deo
