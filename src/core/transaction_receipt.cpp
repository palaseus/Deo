/**
 * @file transaction_receipt.cpp
 * @brief Transaction receipt implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "core/transaction_receipt.h"
#include "crypto/hash.h"
#include "utils/logger.h"

#include <chrono>
#include <sstream>

namespace deo {
namespace core {

nlohmann::json TransactionReceipt::toJson() const {
    nlohmann::json json;
    
    json["transaction_hash"] = transaction_hash;
    json["block_hash"] = block_hash;
    json["block_number"] = block_number;
    json["transaction_index"] = transaction_index;
    json["from_address"] = from_address;
    json["to_address"] = to_address;
    json["gas_used"] = gas_used;
    json["gas_price"] = gas_price;
    json["cumulative_gas_used"] = cumulative_gas_used;
    json["success"] = success;
    json["error_message"] = error_message;
    json["return_data"] = nlohmann::json::binary(return_data);
    json["contract_address"] = contract_address;
    json["logs"] = logs;
    json["timestamp"] = timestamp;
    
    return json;
}

bool TransactionReceipt::fromJson(const nlohmann::json& json) {
    try {
        transaction_hash = json["transaction_hash"];
        block_hash = json["block_hash"];
        block_number = json["block_number"];
        transaction_index = json["transaction_index"];
        from_address = json["from_address"];
        to_address = json["to_address"];
        gas_used = json["gas_used"];
        gas_price = json["gas_price"];
        cumulative_gas_used = json["cumulative_gas_used"];
        success = json["success"];
        error_message = json["error_message"];
        
        if (json.contains("return_data") && json["return_data"].is_binary()) {
            return_data = json["return_data"].get_binary();
        }
        
        contract_address = json["contract_address"];
        
        if (json.contains("logs") && json["logs"].is_array()) {
            logs = json["logs"].get<std::vector<std::string>>();
        }
        
        timestamp = json["timestamp"];
        
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to deserialize transaction receipt: " + std::string(e.what()));
        return false;
    }
}

bool TransactionReceipt::validate() const {
    if (transaction_hash.empty() || block_hash.empty()) {
        return false;
    }
    
    if (from_address.empty()) {
        return false;
    }
    
    if (gas_used == 0 && success) {
        // Successful transactions should use some gas
        return false;
    }
    
    return true;
}

std::string TransactionReceipt::getHash() const {
    std::stringstream ss;
    ss << transaction_hash << block_hash << block_number << transaction_index
       << from_address << to_address << gas_used << gas_price << success;
    
    return deo::crypto::Hash::sha256(ss.str());
}

void BlockReceipt::addReceipt(const TransactionReceipt& receipt) {
    receipts.push_back(receipt);
    total_gas_used += receipt.gas_used;
}

const TransactionReceipt* BlockReceipt::getReceipt(const std::string& tx_hash) const {
    for (const auto& receipt : receipts) {
        if (receipt.transaction_hash == tx_hash) {
            return &receipt;
        }
    }
    return nullptr;
}

nlohmann::json BlockReceipt::toJson() const {
    nlohmann::json json;
    
    json["block_hash"] = block_hash;
    json["block_number"] = block_number;
    json["total_gas_used"] = total_gas_used;
    json["transaction_count"] = transaction_count;
    json["state_root"] = state_root;
    json["timestamp"] = timestamp;
    
    nlohmann::json receipts_json = nlohmann::json::array();
    for (const auto& receipt : receipts) {
        receipts_json.push_back(receipt.toJson());
    }
    json["receipts"] = receipts_json;
    
    return json;
}

bool BlockReceipt::fromJson(const nlohmann::json& json) {
    try {
        block_hash = json["block_hash"];
        block_number = json["block_number"];
        total_gas_used = json["total_gas_used"];
        transaction_count = json["transaction_count"];
        state_root = json["state_root"];
        timestamp = json["timestamp"];
        
        if (json.contains("receipts") && json["receipts"].is_array()) {
            receipts.clear();
            for (const auto& receipt_json : json["receipts"]) {
                TransactionReceipt receipt;
                if (receipt.fromJson(receipt_json)) {
                    receipts.push_back(receipt);
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to deserialize block receipt: " + std::string(e.what()));
        return false;
    }
}

bool BlockReceipt::validate() const {
    if (block_hash.empty()) {
        return false;
    }
    
    if (receipts.size() != transaction_count) {
        return false;
    }
    
    uint64_t calculated_gas = 0;
    for (const auto& receipt : receipts) {
        if (!receipt.validate()) {
            return false;
        }
        calculated_gas += receipt.gas_used;
    }
    
    if (calculated_gas != total_gas_used) {
        return false;
    }
    
    return true;
}

} // namespace core
} // namespace deo
