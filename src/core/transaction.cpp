/**
 * @file transaction.cpp
 * @brief Transaction implementation for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "core/transaction.h"
#include "crypto/hash.h"
#include "crypto/signature.h"
#include "utils/logger.h"
#include "utils/error_handler.h"

#include <sstream>
#include <iomanip>
#include <algorithm>

namespace deo {
namespace core {

// TransactionInput implementation

nlohmann::json TransactionInput::toJson() const {
    nlohmann::json json;
    json["previous_tx_hash"] = previous_tx_hash;
    json["output_index"] = output_index;
    json["signature"] = signature;
    json["public_key"] = public_key;
    json["sequence"] = sequence;
    return json;
}

bool TransactionInput::fromJson(const nlohmann::json& json) {
    try {
        previous_tx_hash = json.at("previous_tx_hash").get<std::string>();
        output_index = json.at("output_index").get<uint32_t>();
        signature = json.at("signature").get<std::string>();
        public_key = json.at("public_key").get<std::string>();
        sequence = json.at("sequence").get<uint64_t>();
        return true;
    } catch (const std::exception& e) {
        DEO_ERROR(VALIDATION, "Failed to deserialize TransactionInput from JSON: " + std::string(e.what()));
        return false;
    }
}

std::string TransactionInput::getSerializedData() const {
    std::stringstream ss;
    ss << previous_tx_hash
       << std::setfill('0') << std::setw(8) << std::hex << output_index
       << signature
       << public_key
       << std::setfill('0') << std::setw(16) << std::hex << sequence;
    return ss.str();
}

// TransactionOutput implementation

nlohmann::json TransactionOutput::toJson() const {
    nlohmann::json json;
    json["value"] = value;
    json["recipient_address"] = recipient_address;
    json["script_pubkey"] = script_pubkey;
    json["output_index"] = output_index;
    return json;
}

bool TransactionOutput::fromJson(const nlohmann::json& json) {
    try {
        value = json.at("value").get<uint64_t>();
        recipient_address = json.at("recipient_address").get<std::string>();
        script_pubkey = json.at("script_pubkey").get<std::string>();
        output_index = json.at("output_index").get<uint32_t>();
        return true;
    } catch (const std::exception& e) {
        DEO_ERROR(VALIDATION, "Failed to deserialize TransactionOutput from JSON: " + std::string(e.what()));
        return false;
    }
}

std::string TransactionOutput::getSerializedData() const {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(16) << std::hex << value
       << recipient_address
       << script_pubkey
       << std::setfill('0') << std::setw(8) << std::hex << output_index;
    return ss.str();
}

// Transaction implementation

Transaction::Transaction()
    : version_(1)
    , lock_time_(0)
    , type_(Type::REGULAR)
    , timestamp_(std::chrono::system_clock::now())
    , hash_cached_(false) {
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction created");
}

Transaction::Transaction(const std::vector<TransactionInput>& inputs,
                       const std::vector<TransactionOutput>& outputs,
                       Type type)
    : version_(1)
    , inputs_(inputs)
    , outputs_(outputs)
    , lock_time_(0)
    , type_(type)
    , timestamp_(std::chrono::system_clock::now())
    , hash_cached_(false) {
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction created with " + std::to_string(inputs.size()) + 
                  " inputs and " + std::to_string(outputs.size()) + " outputs");
}

std::string Transaction::getId() const {
    if (!hash_cached_) {
        updateHash();
    }
    return cached_hash_;
}

std::string Transaction::calculateHash() const {
    if (!hash_cached_) {
        updateHash();
    }
    return cached_hash_;
}

bool Transaction::addInput(const TransactionInput& input) {
    if (!validateInput(input)) {
        DEO_ERROR(VALIDATION, "Invalid transaction input");
        return false;
    }
    
    inputs_.push_back(input);
    hash_cached_ = false; // Invalidate cached hash
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Added input to transaction");
    return true;
}

bool Transaction::addOutput(const TransactionOutput& output) {
    if (!validateOutput(output)) {
        DEO_ERROR(VALIDATION, "Invalid transaction output");
        return false;
    }
    
    outputs_.push_back(output);
    hash_cached_ = false; // Invalidate cached hash
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Added output to transaction");
    return true;
}

bool Transaction::sign(const std::string& private_key) {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Signing transaction");
    
    if (private_key.empty()) {
        DEO_ERROR(CRYPTOGRAPHY, "Private key is empty");
        return false;
    }
    
    if (inputs_.empty()) {
        DEO_ERROR(VALIDATION, "No inputs to sign");
        return false;
    }
    
    // Get transaction data for signing
    std::string tx_data = getSerializedData();
    
    // Sign each input
    for (auto& input : inputs_) {
        if (input.signature.empty()) {
            input.signature = crypto::Signature::sign(tx_data, private_key);
            if (input.signature.empty()) {
                DEO_ERROR(CRYPTOGRAPHY, "Failed to sign transaction input");
                return false;
            }
        }
    }
    
    hash_cached_ = false; // Invalidate cached hash
    
    DEO_LOG_INFO(BLOCKCHAIN, "Transaction signed successfully");
    return true;
}

bool Transaction::verify() const {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Verifying transaction signatures");
    
    // Coinbase transactions don't need input verification
    if (type_ == Type::COINBASE) {
        return true;
    }
    
    if (inputs_.empty()) {
        DEO_ERROR(VALIDATION, "No inputs to verify");
        return false;
    }
    
    // Get transaction data for verification
    std::string tx_data = getSerializedData();
    
    // Verify each input signature
    for (const auto& input : inputs_) {
        if (input.signature.empty() || input.public_key.empty()) {
            DEO_ERROR(VALIDATION, "Missing signature or public key");
            return false;
        }
        
        if (!crypto::Signature::verify(tx_data, input.signature, input.public_key)) {
            DEO_ERROR(CRYPTOGRAPHY, "Invalid signature for input");
            return false;
        }
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction verification successful");
    return true;
}

bool Transaction::validate() const {
    DEO_LOG_DEBUG(BLOCKCHAIN, "Validating transaction structure");
    
    // Validate version
    if (version_ == 0) {
        DEO_ERROR(VALIDATION, "Invalid transaction version");
        return false;
    }
    
    // Validate inputs
    if (inputs_.empty() && type_ != Type::COINBASE) {
        DEO_ERROR(VALIDATION, "No inputs in non-coinbase transaction");
        return false;
    }
    
    for (const auto& input : inputs_) {
        if (!validateInput(input)) {
            return false;
        }
    }
    
    // Validate outputs
    if (outputs_.empty()) {
        DEO_ERROR(VALIDATION, "No outputs in transaction");
        return false;
    }
    
    for (const auto& output : outputs_) {
        if (!validateOutput(output)) {
            return false;
        }
    }
    
    // Validate coinbase transaction
    if (type_ == Type::COINBASE) {
        if (inputs_.size() != 1 || inputs_[0].previous_tx_hash != "0000000000000000000000000000000000000000000000000000000000000000") {
            DEO_ERROR(VALIDATION, "Invalid coinbase transaction");
            return false;
        }
    }
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction validation successful");
    return true;
}

nlohmann::json Transaction::toJson() const {
    nlohmann::json json;
    json["version"] = version_;
    json["lock_time"] = lock_time_;
    json["type"] = static_cast<int>(type_);
    json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_.time_since_epoch()).count();
    
    // Serialize inputs
    nlohmann::json inputs_json = nlohmann::json::array();
    for (const auto& input : inputs_) {
        inputs_json.push_back(input.toJson());
    }
    json["inputs"] = inputs_json;
    
    // Serialize outputs
    nlohmann::json outputs_json = nlohmann::json::array();
    for (const auto& output : outputs_) {
        outputs_json.push_back(output.toJson());
    }
    json["outputs"] = outputs_json;
    
    json["hash"] = calculateHash();
    
    return json;
}

bool Transaction::fromJson(const nlohmann::json& json) {
    try {
        version_ = json.at("version").get<uint32_t>();
        lock_time_ = json.at("lock_time").get<uint32_t>();
        type_ = static_cast<Type>(json.at("type").get<int>());
        
        auto timestamp_ms = json.at("timestamp").get<uint64_t>();
        timestamp_ = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp_ms));
        
        // Deserialize inputs
        inputs_.clear();
        const auto& inputs_json = json.at("inputs");
        for (const auto& input_json : inputs_json) {
            TransactionInput input;
            if (!input.fromJson(input_json)) {
                return false;
            }
            inputs_.push_back(input);
        }
        
        // Deserialize outputs
        outputs_.clear();
        const auto& outputs_json = json.at("outputs");
        for (const auto& output_json : outputs_json) {
            TransactionOutput output;
            if (!output.fromJson(output_json)) {
                return false;
            }
            outputs_.push_back(output);
        }
        
        hash_cached_ = false; // Invalidate cached hash
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction deserialized from JSON");
        return true;
        
    } catch (const std::exception& e) {
        DEO_ERROR(VALIDATION, "Failed to deserialize Transaction from JSON: " + std::string(e.what()));
        return false;
    }
}

std::vector<uint8_t> Transaction::serialize() const {
    std::vector<uint8_t> data;
    
    // Serialize version (4 bytes)
    uint32_t version = version_;
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&version), 
                reinterpret_cast<uint8_t*>(&version) + sizeof(version));
    
    // Serialize input count (4 bytes)
    uint32_t input_count = static_cast<uint32_t>(inputs_.size());
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&input_count), 
                reinterpret_cast<uint8_t*>(&input_count) + sizeof(input_count));
    
    // Serialize inputs
    for (const auto& input : inputs_) {
        // Previous tx hash (32 bytes)
        std::vector<uint8_t> prev_hash = crypto::Hash::hexToBytes(input.previous_tx_hash);
        data.insert(data.end(), prev_hash.begin(), prev_hash.end());
        
        // Output index (4 bytes)
        uint32_t output_index = input.output_index;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&output_index), 
                    reinterpret_cast<uint8_t*>(&output_index) + sizeof(output_index));
        
        // Signature length and data
        uint32_t sig_length = static_cast<uint32_t>(input.signature.length());
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&sig_length), 
                    reinterpret_cast<uint8_t*>(&sig_length) + sizeof(sig_length));
        data.insert(data.end(), input.signature.begin(), input.signature.end());
        
        // Public key length and data
        uint32_t pubkey_length = static_cast<uint32_t>(input.public_key.length());
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&pubkey_length), 
                    reinterpret_cast<uint8_t*>(&pubkey_length) + sizeof(pubkey_length));
        data.insert(data.end(), input.public_key.begin(), input.public_key.end());
        
        // Sequence (8 bytes)
        uint64_t sequence = input.sequence;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&sequence), 
                    reinterpret_cast<uint8_t*>(&sequence) + sizeof(sequence));
    }
    
    // Serialize output count (4 bytes)
    uint32_t output_count = static_cast<uint32_t>(outputs_.size());
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&output_count), 
                reinterpret_cast<uint8_t*>(&output_count) + sizeof(output_count));
    
    // Serialize outputs
    for (const auto& output : outputs_) {
        // Value (8 bytes)
        uint64_t value = output.value;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&value), 
                    reinterpret_cast<uint8_t*>(&value) + sizeof(value));
        
        // Recipient address length and data
        uint32_t addr_length = static_cast<uint32_t>(output.recipient_address.length());
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&addr_length), 
                    reinterpret_cast<uint8_t*>(&addr_length) + sizeof(addr_length));
        data.insert(data.end(), output.recipient_address.begin(), output.recipient_address.end());
        
        // Script pubkey length and data
        uint32_t script_length = static_cast<uint32_t>(output.script_pubkey.length());
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&script_length), 
                    reinterpret_cast<uint8_t*>(&script_length) + sizeof(script_length));
        data.insert(data.end(), output.script_pubkey.begin(), output.script_pubkey.end());
        
        // Output index (4 bytes)
        uint32_t output_index = output.output_index;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&output_index), 
                    reinterpret_cast<uint8_t*>(&output_index) + sizeof(output_index));
    }
    
    // Serialize lock time (4 bytes)
    uint32_t lock_time = lock_time_;
    data.insert(data.end(), reinterpret_cast<uint8_t*>(&lock_time), 
                reinterpret_cast<uint8_t*>(&lock_time) + sizeof(lock_time));
    
    return data;
}

bool Transaction::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 16) { // Minimum size check
        DEO_ERROR(VALIDATION, "Transaction data too small");
        return false;
    }
    
    size_t offset = 0;
    
    // Deserialize version
    version_ = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += sizeof(uint32_t);
    
    // Deserialize input count
    uint32_t input_count = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += sizeof(uint32_t);
    
    // Deserialize inputs
    inputs_.clear();
    for (uint32_t i = 0; i < input_count; ++i) {
        if (offset + 32 > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for input");
            return false;
        }
        
        TransactionInput input;
        
        // Previous tx hash
        input.previous_tx_hash = crypto::Hash::bytesToHex(&data[offset], 32);
        offset += 32;
        
        // Output index
        if (offset + sizeof(uint32_t) > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for output index");
            return false;
        }
        input.output_index = *reinterpret_cast<const uint32_t*>(&data[offset]);
        offset += sizeof(uint32_t);
        
        // Signature
        if (offset + sizeof(uint32_t) > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for signature length");
            return false;
        }
        uint32_t sig_length = *reinterpret_cast<const uint32_t*>(&data[offset]);
        offset += sizeof(uint32_t);
        
        if (offset + sig_length > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for signature");
            return false;
        }
        input.signature = std::string(reinterpret_cast<const char*>(&data[offset]), sig_length);
        offset += sig_length;
        
        // Public key
        if (offset + sizeof(uint32_t) > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for public key length");
            return false;
        }
        uint32_t pubkey_length = *reinterpret_cast<const uint32_t*>(&data[offset]);
        offset += sizeof(uint32_t);
        
        if (offset + pubkey_length > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for public key");
            return false;
        }
        input.public_key = std::string(reinterpret_cast<const char*>(&data[offset]), pubkey_length);
        offset += pubkey_length;
        
        // Sequence
        if (offset + sizeof(uint64_t) > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for sequence");
            return false;
        }
        input.sequence = *reinterpret_cast<const uint64_t*>(&data[offset]);
        offset += sizeof(uint64_t);
        
        inputs_.push_back(input);
    }
    
    // Deserialize output count
    if (offset + sizeof(uint32_t) > data.size()) {
        DEO_ERROR(VALIDATION, "Insufficient data for output count");
        return false;
    }
    uint32_t output_count = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += sizeof(uint32_t);
    
    // Deserialize outputs
    outputs_.clear();
    for (uint32_t i = 0; i < output_count; ++i) {
        TransactionOutput output;
        
        // Value
        if (offset + sizeof(uint64_t) > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for value");
            return false;
        }
        output.value = *reinterpret_cast<const uint64_t*>(&data[offset]);
        offset += sizeof(uint64_t);
        
        // Recipient address
        if (offset + sizeof(uint32_t) > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for address length");
            return false;
        }
        uint32_t addr_length = *reinterpret_cast<const uint32_t*>(&data[offset]);
        offset += sizeof(uint32_t);
        
        if (offset + addr_length > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for address");
            return false;
        }
        output.recipient_address = std::string(reinterpret_cast<const char*>(&data[offset]), addr_length);
        offset += addr_length;
        
        // Script pubkey
        if (offset + sizeof(uint32_t) > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for script length");
            return false;
        }
        uint32_t script_length = *reinterpret_cast<const uint32_t*>(&data[offset]);
        offset += sizeof(uint32_t);
        
        if (offset + script_length > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for script");
            return false;
        }
        output.script_pubkey = std::string(reinterpret_cast<const char*>(&data[offset]), script_length);
        offset += script_length;
        
        // Output index
        if (offset + sizeof(uint32_t) > data.size()) {
            DEO_ERROR(VALIDATION, "Insufficient data for output index");
            return false;
        }
        output.output_index = *reinterpret_cast<const uint32_t*>(&data[offset]);
        offset += sizeof(uint32_t);
        
        outputs_.push_back(output);
    }
    
    // Deserialize lock time
    if (offset + sizeof(uint32_t) > data.size()) {
        DEO_ERROR(VALIDATION, "Insufficient data for lock time");
        return false;
    }
    lock_time_ = *reinterpret_cast<const uint32_t*>(&data[offset]);
    
    hash_cached_ = false; // Invalidate cached hash
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Transaction deserialized from binary data");
    return true;
}

size_t Transaction::getSize() const {
    return serialize().size();
}

uint64_t Transaction::getFee() const {
    if (type_ == Type::COINBASE) {
        return 0; // Coinbase transactions have no fee
    }
    
    uint64_t input_total = 0;
    uint64_t output_total = 0;
    
    // Calculate total input value (simplified - in reality would need UTXO lookup)
    for (const auto& input : inputs_) {
        (void)input; // Suppress unused variable warning
        // Note: In a real implementation, we would look up the previous output value
        // For now, we'll assume a standard input value
        input_total += 1000000; // 0.01 BTC in satoshis
    }
    
    // Calculate total output value
    for (const auto& output : outputs_) {
        output_total += output.value;
    }
    
    return (input_total > output_total) ? (input_total - output_total) : 0;
}

bool Transaction::isCoinbase() const {
    return type_ == Type::COINBASE;
}

std::string Transaction::getSerializedData() const {
    std::stringstream ss;
    
    // Serialize in deterministic order for hashing
    ss << std::setfill('0') << std::setw(8) << std::hex << version_;
    
    // Serialize inputs
    ss << std::setfill('0') << std::setw(8) << std::hex << inputs_.size();
    for (const auto& input : inputs_) {
        ss << input.getSerializedData();
    }
    
    // Serialize outputs
    ss << std::setfill('0') << std::setw(8) << std::hex << outputs_.size();
    for (const auto& output : outputs_) {
        ss << output.getSerializedData();
    }
    
    ss << std::setfill('0') << std::setw(8) << std::hex << lock_time_;
    ss << static_cast<int>(type_);
    
    return ss.str();
}

void Transaction::updateHash() const {
    std::string serialized_data = getSerializedData();
    cached_hash_ = crypto::Hash::sha256(serialized_data);
    hash_cached_ = true;
}

bool Transaction::validateInput(const TransactionInput& input) const {
    if (input.previous_tx_hash.empty()) {
        DEO_ERROR(VALIDATION, "Empty previous transaction hash");
        return false;
    }
    
    if (input.previous_tx_hash.length() != 64) { // 32 bytes = 64 hex chars
        DEO_ERROR(VALIDATION, "Invalid previous transaction hash length");
        return false;
    }
    
    // For coinbase transactions, public key can be empty
    if (input.public_key.empty() && type_ != Type::COINBASE) {
        DEO_ERROR(VALIDATION, "Empty public key");
        return false;
    }
    
    return true;
}

bool Transaction::validateOutput(const TransactionOutput& output) const {
    if (output.value == 0) {
        DEO_ERROR(VALIDATION, "Output value cannot be zero");
        return false;
    }
    
    if (output.recipient_address.empty()) {
        DEO_ERROR(VALIDATION, "Empty recipient address");
        return false;
    }
    
    return true;
}

} // namespace core
} // namespace deo