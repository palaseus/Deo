/**
 * @file network_messages.h
 * @brief Network message structures for peer-to-peer communication
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "core/block.h"
#include "core/transaction.h"

namespace deo {
namespace network {

/**
 * @brief Network message types
 */
enum class MessageType : uint8_t {
    HELLO = 0x01,           ///< Initial handshake message
    INV = 0x02,             ///< Inventory announcement
    GETDATA = 0x03,         ///< Request for data
    BLOCK = 0x04,           ///< Block data
    TX = 0x05,              ///< Transaction data
    PING = 0x06,            ///< Ping message
    PONG = 0x07,            ///< Pong response
    GETBLOCKS = 0x08,       ///< Request for blocks
    GETHEADERS = 0x09,      ///< Request for block headers
    HEADERS = 0x0A,         ///< Block headers
    REJECT = 0x0B,          ///< Rejection message
    MEMPOOL = 0x0C,         ///< Mempool request
    FILTERLOAD = 0x0D,      ///< Bloom filter load
    FILTERADD = 0x0E,       ///< Bloom filter add
    FILTERCLEAR = 0x0F,     ///< Bloom filter clear
    MERKLEBLOCK = 0x10,     ///< Merkle block
    ALERT = 0x11,           ///< Alert message
    VERSION = 0x12,         ///< Version message
    VERACK = 0x13,          ///< Version acknowledgment
    ADDR = 0x14,            ///< Address message
    GETADDR = 0x15,         ///< Get addresses
    NOTFOUND = 0x16         ///< Data not found
};

/**
 * @brief Base class for all network messages
 */
class NetworkMessage {
public:
    /**
     * @brief Constructor
     * @param type Message type
     */
    explicit NetworkMessage(MessageType type);
    
    /**
     * @brief Destructor
     */
    virtual ~NetworkMessage() = default;
    
    /**
     * @brief Get message type
     * @return Message type
     */
    MessageType getType() const { return type_; }
    
    /**
     * @brief Get message size
     * @return Message size in bytes
     */
    virtual size_t getSize() const = 0;
    
    /**
     * @brief Serialize message to binary
     * @return Binary data
     */
    virtual std::vector<uint8_t> serialize() const = 0;
    
    /**
     * @brief Deserialize message from binary
     * @param data Binary data
     * @return True if successful
     */
    virtual bool deserialize(const std::vector<uint8_t>& data) = 0;
    
    /**
     * @brief Serialize message to JSON
     * @return JSON representation
     */
    virtual nlohmann::json toJson() const = 0;
    
    /**
     * @brief Deserialize message from JSON
     * @param json JSON representation
     * @return True if successful
     */
    virtual bool fromJson(const nlohmann::json& json) = 0;
    
    /**
     * @brief Validate message
     * @return True if valid
     */
    virtual bool validate() const = 0;

protected:
    MessageType type_;      ///< Message type
    uint32_t version_;      ///< Protocol version
    uint64_t timestamp_;    ///< Message timestamp
};

/**
 * @brief Hello message for initial handshake
 */
class HelloMessage : public NetworkMessage {
public:
    /**
     * @brief Constructor
     */
    HelloMessage();
    
    /**
     * @brief Constructor with parameters
     * @param node_id Node identifier
     * @param version Protocol version
     * @param capabilities Node capabilities
     */
    HelloMessage(const std::string& node_id, uint32_t version, const std::vector<std::string>& capabilities);
    
    std::string node_id_;                   ///< Node identifier
    std::vector<std::string> capabilities_; ///< Node capabilities
    std::string user_agent_;                ///< User agent string
    
    size_t getSize() const override;
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
    bool validate() const override;
};

/**
 * @brief Inventory message for announcing available data
 */
class InvMessage : public NetworkMessage {
public:
    /**
     * @brief Constructor
     */
    InvMessage();
    
    /**
     * @brief Constructor with parameters
     * @param inventory Inventory items
     */
    explicit InvMessage(const std::vector<std::string>& inventory);
    
    std::vector<std::string> inventory_;    ///< Inventory items (hashes)
    
    size_t getSize() const override;
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
    bool validate() const override;
};

/**
 * @brief GetData message for requesting specific data
 */
class GetDataMessage : public NetworkMessage {
public:
    /**
     * @brief Constructor
     */
    GetDataMessage();
    
    /**
     * @brief Constructor with parameters
     * @param items Items to request
     */
    explicit GetDataMessage(const std::vector<std::string>& items);
    
    std::vector<std::string> items_;        ///< Items to request
    
    size_t getSize() const override;
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
    bool validate() const override;
};

/**
 * @brief Block message for sending block data
 */
class BlockMessage : public NetworkMessage {
public:
    /**
     * @brief Constructor
     */
    BlockMessage();
    
    /**
     * @brief Constructor with block
     * @param block Block to send
     */
    explicit BlockMessage(std::shared_ptr<core::Block> block);
    
    std::shared_ptr<core::Block> block_;    ///< Block data
    
    size_t getSize() const override;
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
    bool validate() const override;
};

/**
 * @brief Transaction message for sending transaction data
 */
class TxMessage : public NetworkMessage {
public:
    /**
     * @brief Constructor
     */
    TxMessage();
    
    /**
     * @brief Constructor with transaction
     * @param transaction Transaction to send
     */
    explicit TxMessage(std::shared_ptr<core::Transaction> transaction);
    
    std::shared_ptr<core::Transaction> transaction_; ///< Transaction data
    
    size_t getSize() const override;
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
    bool validate() const override;
};

/**
 * @brief Ping message for connection health check
 */
class PingMessage : public NetworkMessage {
public:
    /**
     * @brief Constructor
     */
    PingMessage();
    
    /**
     * @brief Constructor with nonce
     * @param nonce Random nonce
     */
    explicit PingMessage(uint64_t nonce);
    
    uint64_t nonce_;                        ///< Random nonce
    
    size_t getSize() const override;
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
    bool validate() const override;
};

/**
 * @brief Pong message for ping response
 */
class PongMessage : public NetworkMessage {
public:
    /**
     * @brief Constructor
     */
    PongMessage();
    
    /**
     * @brief Constructor with nonce
     * @param nonce Nonce from ping message
     */
    explicit PongMessage(uint64_t nonce);
    
    uint64_t nonce_;                        ///< Nonce from ping message
    
    size_t getSize() const override;
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
    bool validate() const override;
};

/**
 * @brief Reject message for rejecting invalid data
 */
class RejectMessage : public NetworkMessage {
public:
    /**
     * @brief Constructor
     */
    RejectMessage();
    
    /**
     * @brief Constructor with parameters
     * @param reason Rejection reason
     * @param data_hash Hash of rejected data
     */
    RejectMessage(const std::string& reason, const std::string& data_hash);
    
    std::string reason_;                    ///< Rejection reason
    std::string data_hash_;                 ///< Hash of rejected data
    
    size_t getSize() const override;
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
    bool validate() const override;
};

/**
 * @brief Message factory for creating message instances
 */
class MessageFactory {
public:
    /**
     * @brief Create message from type
     * @param type Message type
     * @return Message pointer or nullptr if invalid type
     */
    static std::unique_ptr<NetworkMessage> createMessage(MessageType type) {
        switch (type) {
            case MessageType::HELLO:
                return std::make_unique<HelloMessage>();
            case MessageType::INV:
                return std::make_unique<InvMessage>();
            case MessageType::GETDATA:
                return std::make_unique<GetDataMessage>();
            case MessageType::BLOCK:
                return std::make_unique<BlockMessage>();
            case MessageType::TX:
                return std::make_unique<TxMessage>();
            case MessageType::PING:
                return std::make_unique<PingMessage>();
            case MessageType::PONG:
                return std::make_unique<PongMessage>();
            case MessageType::GETBLOCKS:
                return std::make_unique<GetDataMessage>(); // Use GetDataMessage for block requests
            case MessageType::GETHEADERS:
                return std::make_unique<GetDataMessage>(); // Use GetDataMessage for header requests
            case MessageType::HEADERS:
                return std::make_unique<InvMessage>(); // Use InvMessage for headers response
            default:
                return nullptr;
        }
    }
    
    /**
     * @brief Create message from binary data
     * @param data Binary data
     * @return Message pointer or nullptr if invalid data
     */
    static std::unique_ptr<NetworkMessage> createFromBinary(const std::vector<uint8_t>& data);
    
    /**
     * @brief Create message from JSON
     * @param json JSON data
     * @return Message pointer or nullptr if invalid JSON
     */
    static std::unique_ptr<NetworkMessage> createFromJson(const nlohmann::json& json);
    
    /**
     * @brief Get message type name
     * @param type Message type
     * @return Type name string
     */
    static std::string getTypeName(MessageType type);
};


} // namespace network
} // namespace deo
