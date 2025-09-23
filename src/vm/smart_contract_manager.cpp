/**
 * @file smart_contract_manager.cpp
 * @brief Smart Contract Manager implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "vm/smart_contract_manager.h"
#include "utils/logger.h"
#include "crypto/hash.h"

#include <sstream>
#include <iomanip>

namespace deo {
namespace vm {

SmartContractManager::SmartContractManager(std::shared_ptr<StateStore> state_store) 
    : state_store_(state_store)
    , total_deployments_(0)
    , total_calls_(0)
    , total_gas_used_(0) {
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Smart Contract Manager initialized with persistent state store");
}

SmartContractManager::~SmartContractManager() {
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Smart Contract Manager destroyed");
}

std::string SmartContractManager::deployContract(
    const ContractDeploymentTransaction& deployment_tx,
    VirtualMachine& /* vm */
) {
    std::lock_guard<std::mutex> lock(mutex_);
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Deploying contract from: " + deployment_tx.getFromAddress());
    
    try {
        // Validate deployment transaction
        if (!deployment_tx.validate()) {
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Invalid contract deployment transaction");
            return "";
        }
        
        // Get deployer nonce and increment it
        uint64_t nonce = state_store_->incrementNonce(deployment_tx.getFromAddress());
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Deployer nonce incremented to: " + std::to_string(nonce));
        
        // Generate contract address
        std::string contract_address = generateContractAddress(deployment_tx.getFromAddress(), nonce);
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Generated contract address: " + contract_address);
        
        // Check if contract already exists
        if (state_store_->contractExists(contract_address)) {
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Contract already exists at address: " + contract_address);
            return "";
        }
        
        // Validate bytecode
        if (!validateBytecode(deployment_tx.getBytecode())) {
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Invalid contract bytecode");
            return "";
        }
        
        // Deduct gas cost from deployer
        uint64_t gas_used = deployment_tx.getGasLimit(); // Simplified gas calculation
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Attempting to deduct gas cost: " + std::to_string(gas_used) + " * " + std::to_string(deployment_tx.getGasPrice()));
        if (!deductGasCost(deployment_tx.getFromAddress(), gas_used, deployment_tx.getGasPrice())) {
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Insufficient balance for gas payment");
            return "";
        }
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Gas cost deducted successfully");
        
        // Transfer value if any
        if (deployment_tx.getValue() > 0) {
            if (!transferValue(deployment_tx.getFromAddress(), contract_address, uint256_t(deployment_tx.getValue()))) {
                DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to transfer value to contract");
                return "";
            }
        }
        
        // Deploy contract to persistent storage
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Attempting to deploy contract to address: " + contract_address);
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Bytecode size: " + std::to_string(deployment_tx.getBytecode().size()));
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Deployer address: " + deployment_tx.getFromAddress());
        
        if (!state_store_->deployContract(contract_address, 
                                        deployment_tx.getBytecode(),
                                        deployment_tx.getFromAddress(),
                                        0)) { // TODO: Get current block number
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to deploy contract to persistent storage");
            return "";
        }
        
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Contract successfully deployed to persistent storage");
        
        // Update statistics
        total_deployments_++;
        total_gas_used_ += gas_used;
        
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Contract deployed successfully at address: " + contract_address);
        return contract_address;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to deploy contract: " + std::string(e.what()));
        return "";
    }
}

ExecutionResult SmartContractManager::callContract(
    const ContractCallTransaction& call_tx,
    VirtualMachine& vm
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ExecutionResult result;
    result.success = false;
    result.gas_used = 0;
    result.return_data.clear();
    result.error_message = "";
    
    try {
        // Validate call transaction
        if (!call_tx.validate()) {
            result.error_message = "Invalid contract call transaction";
            return result;
        }
        
        // Check if contract exists
        if (!state_store_->contractExists(call_tx.getContractAddress())) {
            result.error_message = "Contract not found";
            return result;
        }
        
        // Get contract state
        auto contract_state = state_store_->getContractState(call_tx.getContractAddress());
        if (!contract_state) {
            result.error_message = "Failed to get contract state";
            return result;
        }
        
        // Deduct gas cost from caller
        uint64_t gas_used = call_tx.getGasLimit(); // Simplified gas calculation
        if (!deductGasCost(call_tx.getFromAddress(), gas_used, call_tx.getGasPrice())) {
            result.error_message = "Insufficient balance for gas payment";
            return result;
        }
        
        // Transfer value if any
        if (call_tx.getValue() > 0) {
            if (!transferValue(call_tx.getFromAddress(), call_tx.getContractAddress(), uint256_t(call_tx.getValue()))) {
                result.error_message = "Failed to transfer value to contract";
                return result;
            }
        }
        
        // Create execution context
        ExecutionContext context;
        context.code = contract_state->bytecode;
        context.gas_limit = call_tx.getGasLimit();
        context.contract_address = call_tx.getContractAddress();
        context.caller_address = call_tx.getFromAddress();
        context.value = call_tx.getValue();
        context.input_data = call_tx.getInputData();
        context.gas_price = call_tx.getGasPrice();
        context.block_number = 0; // TODO: Get current block number
        context.block_timestamp = 0; // TODO: Get current block timestamp
        context.block_coinbase = "0x0000000000000000000000000000000000000000"; // TODO: Get current block coinbase
        
        // Execute contract
        ExecutionResult vm_result = vm.execute(context);
        
        // Update contract storage if execution was successful
        if (vm_result.success) {
            // TODO: Update contract storage from VM state
            // This would require the VM to return storage changes
        }
        
        // Update statistics
        total_calls_++;
        total_gas_used_ += gas_used;
        
        result = vm_result;
        result.gas_used = gas_used;
        
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Contract call executed successfully");
        return result;
        
    } catch (const std::exception& e) {
        result.error_message = "Contract call failed: " + std::string(e.what());
        DEO_LOG_ERROR(VIRTUAL_MACHINE, result.error_message);
        return result;
    }
}

std::shared_ptr<ContractState> SmartContractManager::getContractState(const std::string& address) {
    return state_store_->getContractState(address);
}

bool SmartContractManager::contractExists(const std::string& address) {
    return state_store_->contractExists(address);
}

uint256_t SmartContractManager::getBalance(const std::string& address) {
    return state_store_->getBalance(address);
}

bool SmartContractManager::setBalance(const std::string& address, const uint256_t& balance) {
    return state_store_->setBalance(address, balance);
}

uint64_t SmartContractManager::incrementNonce(const std::string& address) {
    return state_store_->incrementNonce(address);
}

bool SmartContractManager::validateBytecode(const std::vector<uint8_t>& bytecode) const {
    if (bytecode.empty()) {
        return false;
    }
    
    // Basic validation - check for valid opcodes
    for (size_t i = 0; i < bytecode.size(); i++) {
        uint8_t opcode = bytecode[i];
        
        // Check if opcode is valid (basic range check)
        // Opcode validation - all uint8_t values are valid
        (void)opcode; // Suppress unused variable warning
        
        // Skip PUSH data
        if (opcode >= 0x60 && opcode <= 0x7F) {
            uint8_t push_size = opcode - 0x5F;
            i += push_size; // Skip the data bytes
            if (i >= bytecode.size()) {
                return false; // Incomplete PUSH instruction
            }
        }
    }
    
    return true;
}

std::string SmartContractManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"total_deployments\": " << total_deployments_ << ",\n";
    ss << "  \"total_calls\": " << total_calls_ << ",\n";
    ss << "  \"total_gas_used\": " << total_gas_used_ << ",\n";
    ss << "  \"contracts_deployed\": " << state_store_->getContractCount() << ",\n";
    ss << "  \"accounts\": " << state_store_->getAccountCount() << ",\n";
    ss << "  \"storage_entries\": " << state_store_->getStorageEntryCount() << "\n";
    ss << "}";
    
    return ss.str();
}

// Private helper methods

std::string SmartContractManager::generateContractAddress(const std::string& deployer_address, uint64_t nonce) {
    std::stringstream ss;
    ss << "0x";
    ss << std::hex << std::setfill('0');
    
    // Use deployer address and nonce to generate contract address
    std::string input = deployer_address + std::to_string(nonce);
    std::string hash = deo::crypto::Hash::sha256(input);
    
    // Take first 20 bytes (40 hex characters) for address
    ss << hash.substr(0, 40);
    
    return ss.str();
}

bool SmartContractManager::deductGasCost(const std::string& address, uint64_t gas_used, uint64_t gas_price) {
    uint256_t current_balance = state_store_->getBalance(address);
    uint256_t gas_cost = uint256_t(gas_used) * uint256_t(gas_price);
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "deductGasCost: address=" + address + ", current_balance=" + current_balance.toString() + ", gas_cost=" + gas_cost.toString());
    
    if (current_balance < gas_cost) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Insufficient balance: " + current_balance.toString() + " < " + gas_cost.toString());
        return false; // Insufficient balance
    }
    
    uint256_t new_balance = current_balance - gas_cost;
    bool result = state_store_->setBalance(address, new_balance);
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "deductGasCost result: " + std::string(result ? "true" : "false") + ", new_balance=" + new_balance.toString());
    return result;
}

bool SmartContractManager::transferValue(const std::string& from_address, const std::string& to_address, const uint256_t& value) {
    uint256_t from_balance = state_store_->getBalance(from_address);
    
    if (from_balance < value) {
        return false; // Insufficient balance
    }
    
    // Deduct from sender
    uint256_t new_from_balance = from_balance - value;
    if (!state_store_->setBalance(from_address, new_from_balance)) {
        return false;
    }
    
    // Add to recipient
    uint256_t to_balance = state_store_->getBalance(to_address);
    uint256_t new_to_balance = to_balance + value;
    return state_store_->setBalance(to_address, new_to_balance);
}

} // namespace vm
} // namespace deo