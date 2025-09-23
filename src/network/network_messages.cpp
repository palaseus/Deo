/**
 * @file network_messages.cpp
 * @brief Implementation of network message classes
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "network/network_messages.h"
#include "utils/logger.h"

namespace deo {
namespace network {

// NetworkMessage implementation
NetworkMessage::NetworkMessage(MessageType type) : type_(type) {
    timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

size_t NetworkMessage::getSize() const {
    return serialize().size();
}

std::string MessageFactory::getTypeName(MessageType type) {
    switch (type) {
        case MessageType::HELLO: return "HELLO";
        case MessageType::INV: return "INV";
        case MessageType::GETDATA: return "GETDATA";
        case MessageType::BLOCK: return "BLOCK";
        case MessageType::TX: return "TX";
        case MessageType::PING: return "PING";
        case MessageType::PONG: return "PONG";
        case MessageType::GETBLOCKS: return "GETBLOCKS";
        case MessageType::GETHEADERS: return "GETHEADERS";
        case MessageType::HEADERS: return "HEADERS";
        case MessageType::REJECT: return "REJECT";
        default: return "UNKNOWN";
    }
}

// HelloMessage implementation
HelloMessage::HelloMessage() : NetworkMessage(MessageType::HELLO) {
    version_ = 1;
}

HelloMessage::HelloMessage(const std::string& node_id, uint32_t /* version */, const std::vector<std::string>& capabilities)
    : NetworkMessage(MessageType::HELLO), node_id_(node_id), capabilities_(capabilities) {
}

size_t HelloMessage::getSize() const {
    size_t size = sizeof(type_) + sizeof(version_) + sizeof(timestamp_);
    size += node_id_.size() + user_agent_.size();
    for (const auto& cap : capabilities_) {
        size += cap.size();
    }
    return size;
}

std::vector<uint8_t> HelloMessage::serialize() const {
    std::vector<uint8_t> data;
    // Simplified serialization - just return basic data
    data.push_back(static_cast<uint8_t>(type_));
    return data;
}

bool HelloMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    return data[0] == static_cast<uint8_t>(type_);
}

nlohmann::json HelloMessage::toJson() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type_);
    json["node_id"] = node_id_;
    json["version"] = version_;
    json["capabilities"] = capabilities_;
    json["user_agent"] = user_agent_;
    return json;
}

bool HelloMessage::fromJson(const nlohmann::json& json) {
    try {
        node_id_ = json.at("node_id").get<std::string>();
        version_ = json.at("version").get<uint32_t>();
        capabilities_ = json.at("capabilities").get<std::vector<std::string>>();
        user_agent_ = json.at("user_agent").get<std::string>();
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize HelloMessage: " + std::string(e.what()));
        return false;
    }
}

bool HelloMessage::validate() const {
    return !node_id_.empty() && version_ > 0;
}

// InvMessage implementation
InvMessage::InvMessage() : NetworkMessage(MessageType::INV) {
}

InvMessage::InvMessage(const std::vector<std::string>& inventory)
    : NetworkMessage(MessageType::INV), inventory_(inventory) {
}

size_t InvMessage::getSize() const {
    size_t size = sizeof(type_) + sizeof(timestamp_);
    for (const auto& item : inventory_) {
        size += item.size();
    }
    return size;
}

std::vector<uint8_t> InvMessage::serialize() const {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(type_));
    return data;
}

bool InvMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    return data[0] == static_cast<uint8_t>(type_);
}

nlohmann::json InvMessage::toJson() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type_);
    json["inventory"] = inventory_;
    return json;
}

bool InvMessage::fromJson(const nlohmann::json& json) {
    try {
        inventory_ = json.at("inventory").get<std::vector<std::string>>();
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize InvMessage: " + std::string(e.what()));
        return false;
    }
}

bool InvMessage::validate() const {
    return true; // Inventory can be empty
}

// GetDataMessage implementation
GetDataMessage::GetDataMessage() : NetworkMessage(MessageType::GETDATA) {
}

GetDataMessage::GetDataMessage(const std::vector<std::string>& items)
    : NetworkMessage(MessageType::GETDATA), items_(items) {
}

size_t GetDataMessage::getSize() const {
    size_t size = sizeof(type_) + sizeof(timestamp_);
    for (const auto& item : items_) {
        size += item.size();
    }
    return size;
}

std::vector<uint8_t> GetDataMessage::serialize() const {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(type_));
    return data;
}

bool GetDataMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    return data[0] == static_cast<uint8_t>(type_);
}

nlohmann::json GetDataMessage::toJson() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type_);
    json["items"] = items_;
    return json;
}

bool GetDataMessage::fromJson(const nlohmann::json& json) {
    try {
        items_ = json.at("items").get<std::vector<std::string>>();
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize GetDataMessage: " + std::string(e.what()));
        return false;
    }
}

bool GetDataMessage::validate() const {
    return true; // Items can be empty
}

// BlockMessage implementation
BlockMessage::BlockMessage() : NetworkMessage(MessageType::BLOCK) {
}

BlockMessage::BlockMessage(std::shared_ptr<core::Block> block)
    : NetworkMessage(MessageType::BLOCK), block_(block) {
}

size_t BlockMessage::getSize() const {
    size_t size = sizeof(type_) + sizeof(timestamp_);
    if (block_) {
        size += block_->getSize();
    }
    return size;
}

std::vector<uint8_t> BlockMessage::serialize() const {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(type_));
    return data;
}

bool BlockMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    return data[0] == static_cast<uint8_t>(type_);
}

nlohmann::json BlockMessage::toJson() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type_);
    if (block_) {
        json["block"] = block_->toJson();
    }
    return json;
}

bool BlockMessage::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("block")) {
            block_ = std::make_shared<core::Block>();
            return block_->fromJson(json.at("block"));
        }
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize BlockMessage: " + std::string(e.what()));
        return false;
    }
}

bool BlockMessage::validate() const {
    return block_ != nullptr;
}

// TxMessage implementation
TxMessage::TxMessage() : NetworkMessage(MessageType::TX) {
}

TxMessage::TxMessage(std::shared_ptr<core::Transaction> transaction)
    : NetworkMessage(MessageType::TX), transaction_(transaction) {
}

size_t TxMessage::getSize() const {
    size_t size = sizeof(type_) + sizeof(timestamp_);
    if (transaction_) {
        size += transaction_->getSize();
    }
    return size;
}

std::vector<uint8_t> TxMessage::serialize() const {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(type_));
    return data;
}

bool TxMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    return data[0] == static_cast<uint8_t>(type_);
}

nlohmann::json TxMessage::toJson() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type_);
    if (transaction_) {
        json["transaction"] = transaction_->toJson();
    }
    return json;
}

bool TxMessage::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("transaction")) {
            transaction_ = std::make_shared<core::Transaction>();
            return transaction_->fromJson(json.at("transaction"));
        }
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize TxMessage: " + std::string(e.what()));
        return false;
    }
}

bool TxMessage::validate() const {
    return transaction_ != nullptr;
}

// PingMessage implementation
PingMessage::PingMessage() : NetworkMessage(MessageType::PING) {
    nonce_ = 0;
}

PingMessage::PingMessage(uint64_t nonce) : NetworkMessage(MessageType::PING), nonce_(nonce) {
}

size_t PingMessage::getSize() const {
    return sizeof(type_) + sizeof(timestamp_) + sizeof(nonce_);
}

std::vector<uint8_t> PingMessage::serialize() const {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(type_));
    return data;
}

bool PingMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    return data[0] == static_cast<uint8_t>(type_);
}

nlohmann::json PingMessage::toJson() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type_);
    json["nonce"] = nonce_;
    return json;
}

bool PingMessage::fromJson(const nlohmann::json& json) {
    try {
        nonce_ = json.at("nonce").get<uint64_t>();
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize PingMessage: " + std::string(e.what()));
        return false;
    }
}

bool PingMessage::validate() const {
    return true; // Nonce can be 0
}

// PongMessage implementation
PongMessage::PongMessage() : NetworkMessage(MessageType::PONG) {
    nonce_ = 0;
}

PongMessage::PongMessage(uint64_t nonce) : NetworkMessage(MessageType::PONG), nonce_(nonce) {
}

size_t PongMessage::getSize() const {
    return sizeof(type_) + sizeof(timestamp_) + sizeof(nonce_);
}

std::vector<uint8_t> PongMessage::serialize() const {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(type_));
    return data;
}

bool PongMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    return data[0] == static_cast<uint8_t>(type_);
}

nlohmann::json PongMessage::toJson() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type_);
    json["nonce"] = nonce_;
    return json;
}

bool PongMessage::fromJson(const nlohmann::json& json) {
    try {
        nonce_ = json.at("nonce").get<uint64_t>();
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize PongMessage: " + std::string(e.what()));
        return false;
    }
}

bool PongMessage::validate() const {
    return true; // Nonce can be 0
}

// RejectMessage implementation
RejectMessage::RejectMessage() : NetworkMessage(MessageType::REJECT) {
}

RejectMessage::RejectMessage(const std::string& reason, const std::string& data_hash)
    : NetworkMessage(MessageType::REJECT), reason_(reason), data_hash_(data_hash) {
}

size_t RejectMessage::getSize() const {
    return sizeof(type_) + sizeof(timestamp_) + reason_.size() + data_hash_.size();
}

std::vector<uint8_t> RejectMessage::serialize() const {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(type_));
    return data;
}

bool RejectMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;
    return data[0] == static_cast<uint8_t>(type_);
}

nlohmann::json RejectMessage::toJson() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type_);
    json["reason"] = reason_;
    json["data_hash"] = data_hash_;
    return json;
}

bool RejectMessage::fromJson(const nlohmann::json& json) {
    try {
        reason_ = json.at("reason").get<std::string>();
        data_hash_ = json.at("data_hash").get<std::string>();
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(NETWORKING, "Failed to deserialize RejectMessage: " + std::string(e.what()));
        return false;
    }
}

bool RejectMessage::validate() const {
    return !reason_.empty() && !data_hash_.empty();
}

} // namespace network
} // namespace deo
