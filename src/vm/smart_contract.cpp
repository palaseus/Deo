/**
 * @file smart_contract.cpp
 * @brief Smart contract system implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "vm/smart_contract.h"
#include "vm/virtual_machine.h"
#include "utils/logger.h"
#include "crypto/hash.h"

#include <sstream>
#include <iomanip>

namespace deo {
namespace vm {

// ContractDeploymentTransaction implementation
ContractDeploymentTransaction::ContractDeploymentTransaction(
    const std::string& deployer_address,
    const std::vector<uint8_t>& bytecode,
                                              uint64_t gas_limit,
    uint64_t gas_price,
    uint64_t value
) : from_address_(deployer_address)
  , bytecode_(bytecode)
  , value_(value)
  , gas_limit_(gas_limit)
  , gas_price_(gas_price)
  , nonce_(0) {
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Contract deployment transaction created for deployer: " + deployer_address);
}

std::string ContractDeploymentTransaction::getContractAddress() const {
    // Calculate contract address from deployer address and nonce
    std::stringstream ss;
    ss << "0x";
    ss << std::hex << std::setfill('0');
    
    // Use deployer address and nonce to generate contract address
    std::string input = from_address_ + std::to_string(nonce_);
    std::string hash = deo::crypto::Hash::sha256(input);
    
    // Take first 20 bytes (40 hex characters) for address
    ss << hash.substr(0, 40);
    
    return ss.str();
}

std::vector<uint8_t> ContractDeploymentTransaction::serialize() const {
    std::vector<uint8_t> data;
    
    // Serialize deployer address
    data.insert(data.end(), from_address_.begin(), from_address_.end());
    data.push_back(0); // Null terminator
    
    // Serialize bytecode length
    uint32_t bytecode_length = static_cast<uint32_t>(bytecode_.size());
    data.push_back((bytecode_length >> 24) & 0xFF);
    data.push_back((bytecode_length >> 16) & 0xFF);
    data.push_back((bytecode_length >> 8) & 0xFF);
    data.push_back(bytecode_length & 0xFF);
    
    // Serialize bytecode
    data.insert(data.end(), bytecode_.begin(), bytecode_.end());
    
    return data;
}

bool ContractDeploymentTransaction::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    
    // Find deployer address (null-terminated string)
    size_t addr_end = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == 0) {
            addr_end = i;
            break;
        }
    }
    
    if (addr_end == 0) return false;
    
    // Deserialize deployer address
    from_address_ = std::string(data.begin(), data.begin() + addr_end);
    
    // Deserialize bytecode length
    if (data.size() < addr_end + 1 + 4) return false;
    
    uint32_t bytecode_length = (static_cast<uint32_t>(data[addr_end + 1]) << 24) |
                               (static_cast<uint32_t>(data[addr_end + 2]) << 16) |
                               (static_cast<uint32_t>(data[addr_end + 3]) << 8) |
                               static_cast<uint32_t>(data[addr_end + 4]);
    
    if (data.size() < addr_end + 5 + bytecode_length) return false;
    
    // Deserialize bytecode
    bytecode_.assign(data.begin() + addr_end + 5, data.begin() + addr_end + 5 + bytecode_length);
    
    return true;
}

bool ContractDeploymentTransaction::validate() const {
    
    // Validate bytecode
    if (bytecode_.empty()) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Contract deployment transaction has empty bytecode");
        return false;
    }
    
    // Check bytecode size limit
    if (bytecode_.size() > 24576) { // 24KB limit
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Contract bytecode exceeds size limit");
        return false;
    }
    
    return true;
}

// ContractCallTransaction implementation
ContractCallTransaction::ContractCallTransaction(
    const std::string& caller_address,
    const std::string& contract_address,
    const std::vector<uint8_t>& input_data,
    uint64_t gas_limit,
    uint64_t gas_price,
    uint64_t value
) : from_address_(caller_address)
  , contract_address_(contract_address)
  , input_data_(input_data)
  , value_(value)
  , gas_limit_(gas_limit)
  , gas_price_(gas_price) {
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Contract call transaction created from " + caller_address + " to " + contract_address);
}

std::vector<uint8_t> ContractCallTransaction::serialize() const {
    std::vector<uint8_t> data;
    
    // Serialize caller address
    data.insert(data.end(), from_address_.begin(), from_address_.end());
    data.push_back(0); // Null terminator
    
    // Serialize contract address
    data.insert(data.end(), contract_address_.begin(), contract_address_.end());
    data.push_back(0); // Null terminator
    
    // Serialize input data length
    uint32_t input_length = static_cast<uint32_t>(input_data_.size());
    data.push_back((input_length >> 24) & 0xFF);
    data.push_back((input_length >> 16) & 0xFF);
    data.push_back((input_length >> 8) & 0xFF);
    data.push_back(input_length & 0xFF);
    
    // Serialize input data
    data.insert(data.end(), input_data_.begin(), input_data_.end());
    
    return data;
}

bool ContractCallTransaction::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    
    // Find caller address (null-terminated string)
    size_t caller_end = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == 0) {
            caller_end = i;
            break;
        }
    }
    
    if (caller_end == 0) return false;
    
    // Deserialize caller address
    from_address_ = std::string(data.begin(), data.begin() + caller_end);
    
    // Find contract address (null-terminated string)
    size_t contract_end = 0;
    for (size_t i = caller_end + 1; i < data.size(); ++i) {
        if (data[i] == 0) {
            contract_end = i;
            break;
        }
    }
    
    if (contract_end == 0) return false;
    
    // Deserialize contract address
    contract_address_ = std::string(data.begin() + caller_end + 1, data.begin() + contract_end);
    
    // Deserialize input data length
    if (data.size() < contract_end + 1 + 4) return false;
    
    uint32_t input_length = (static_cast<uint32_t>(data[contract_end + 1]) << 24) |
                            (static_cast<uint32_t>(data[contract_end + 2]) << 16) |
                            (static_cast<uint32_t>(data[contract_end + 3]) << 8) |
                            static_cast<uint32_t>(data[contract_end + 4]);
    
    if (data.size() < contract_end + 5 + input_length) return false;
    
    // Deserialize input data
    input_data_.assign(data.begin() + contract_end + 5, data.begin() + contract_end + 5 + input_length);
    
    return true;
}

bool ContractCallTransaction::validate() const {
    
    // Validate contract address
    if (contract_address_.empty()) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Contract call transaction has empty contract address");
        return false;
    }
    
    // Check input data size limit
    if (input_data_.size() > 1024 * 1024) { // 1MB limit
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Contract call input data exceeds size limit");
            return false;
    }
    
    return true;
}

} // namespace vm
} // namespace deo