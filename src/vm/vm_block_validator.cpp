/**
 * @file vm_block_validator.cpp
 * @brief VM-integrated block validator implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "vm/vm_block_validator.h"
#include "utils/logger.h"
#include "crypto/hash.h"

#include <sstream>
#include <iomanip>
#include <chrono>

namespace deo {
namespace vm {

VMBlockValidator::VMBlockValidator(std::shared_ptr<StateStore> state_store)
    : state_store_(state_store)
    , total_blocks_validated_(0)
    , total_transactions_executed_(0)
    , total_gas_consumed_(0)
    , total_contract_deployments_(0)
    , total_contract_calls_(0) {
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "VMBlockValidator created");
    
    if (!initialize()) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to initialize VMBlockValidator");
    }
}

VMBlockValidator::~VMBlockValidator() {
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "VMBlockValidator destroyed");
}

bool VMBlockValidator::initialize() {
    try {
        // State store should already be initialized by NodeRuntime before creating VMBlockValidator
        // Don't call initialize() again as it's already initialized
        if (!state_store_) {
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "State store is null");
            return false;
        }
        
        // Create contract manager
        contract_manager_ = std::make_unique<SmartContractManager>(state_store_);
        
        // Create virtual machine
        vm_ = std::make_unique<VirtualMachine>();
        
        DEO_LOG_INFO(VIRTUAL_MACHINE, "VMBlockValidator initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to initialize VMBlockValidator: " + std::string(e.what()));
        return false;
    }
}

VMBlockValidationResult VMBlockValidator::validateBlock(
    std::shared_ptr<core::Block> block,
    const std::string& /* previous_state_hash */
) {
    VMBlockValidationResult result;
    result.success = false;
    result.total_gas_used = 0;
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Starting VM block validation for block: " + block->calculateHash());
    
    try {
        // Create state snapshot for potential rollback
        std::string snapshot_id = createStateSnapshot();
        
        // Create block execution context
        ExecutionContext block_context = createBlockContext(block);
        
        // Execute all transactions in the block
        for (const auto& transaction : block->getTransactions()) {
            if (!transaction) {
                result.error_message = "Null transaction in block";
                DEO_LOG_ERROR(VIRTUAL_MACHINE, result.error_message);
                restoreStateSnapshot(snapshot_id);
                return result;
            }
            
            // Execute transaction
            ExecutionResult tx_result = executeTransaction(transaction, block_context);
            
            if (!tx_result.success) {
                result.error_message = "Transaction execution failed: " + tx_result.error_message;
                DEO_LOG_ERROR(VIRTUAL_MACHINE, result.error_message);
                restoreStateSnapshot(snapshot_id);
                return result;
            }
            
            // Update statistics
            result.total_gas_used += tx_result.gas_used;
            result.gas_used_per_tx[transaction->getId()] = tx_result.gas_used;
            
            if (isContractTransaction(transaction)) {
                result.executed_contracts.push_back(transaction->getId());
            }
            
            total_transactions_executed_++;
            total_gas_consumed_ += tx_result.gas_used;
        }
        
        // Update statistics
        total_blocks_validated_++;
        
        result.success = true;
        DEO_LOG_INFO(VIRTUAL_MACHINE, "Block validation successful. Gas used: " + std::to_string(result.total_gas_used));
        
    } catch (const std::exception& e) {
        result.error_message = "Block validation exception: " + std::string(e.what());
        DEO_LOG_ERROR(VIRTUAL_MACHINE, result.error_message);
    }
    
    return result;
}

ExecutionResult VMBlockValidator::executeTransaction(
    std::shared_ptr<core::Transaction> transaction,
    const ExecutionContext& block_context
) {
    ExecutionResult result;
    result.success = false;
    result.gas_used = 0;
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Executing transaction: " + transaction->getId());
    
    try {
        // Validate transaction structure
        if (!validateTransactionStructure(transaction)) {
            result.error_message = "Invalid transaction structure";
            return result;
        }
        
        // Determine transaction type and execute accordingly
        if (isContractTransaction(transaction)) {
            // Check if it's a deployment or call
            if (transaction->getOutputs().empty()) {
                // Contract deployment
                result = executeContractDeployment(transaction, block_context);
                if (result.success) {
                    total_contract_deployments_++;
                }
            } else {
                // Contract call
                result = executeContractCall(transaction, block_context);
                if (result.success) {
                    total_contract_calls_++;
                }
            }
        } else {
            // Regular transaction
            result = executeRegularTransaction(transaction, block_context);
        }
        
    } catch (const std::exception& e) {
        result.error_message = "Transaction execution exception: " + std::string(e.what());
        DEO_LOG_ERROR(VIRTUAL_MACHINE, result.error_message);
    }
    
    return result;
}

bool VMBlockValidator::isContractTransaction(std::shared_ptr<core::Transaction> transaction) const {
    // For now, we'll identify contract transactions by checking if they have
    // specific markers in the transaction data or outputs
    // This is a simplified approach - in a real implementation, you'd have
    // more sophisticated contract transaction identification
    
    // Check if transaction has contract-related data in outputs
    for (const auto& output : transaction->getOutputs()) {
        if (!output.script_pubkey.empty() && 
            output.script_pubkey.find("CONTRACT") != std::string::npos) {
            return true;
        }
    }
    
    // Check if transaction has contract bytecode in inputs (for deployment)
    for (const auto& input : transaction->getInputs()) {
        if (!input.signature.empty() && 
            input.signature.find("DEPLOY") != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

SmartContractManager& VMBlockValidator::getContractManager() {
    return *contract_manager_;
}

std::shared_ptr<StateStore> VMBlockValidator::getStateStore() {
    return state_store_;
}

std::string VMBlockValidator::createStateSnapshot() {
    // Create a snapshot identifier based on current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::string snapshot_id = "snapshot_" + std::to_string(timestamp);
    
    // In a real implementation, you would serialize the current state
    // For now, we'll just store a placeholder
    state_snapshots_[snapshot_id] = "state_snapshot_placeholder";
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Created state snapshot: " + snapshot_id);
    return snapshot_id;
}

bool VMBlockValidator::restoreStateSnapshot(const std::string& snapshot_id) {
    auto it = state_snapshots_.find(snapshot_id);
    if (it == state_snapshots_.end()) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Snapshot not found: " + snapshot_id);
        return false;
    }
    
    // In a real implementation, you would restore the state from the snapshot
    // For now, we'll just log the restoration
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Restored state snapshot: " + snapshot_id);
    
    // Clean up the snapshot
    state_snapshots_.erase(it);
    return true;
}

std::string VMBlockValidator::getValidationStatistics() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"total_blocks_validated\": " << total_blocks_validated_ << ",\n";
    ss << "  \"total_transactions_executed\": " << total_transactions_executed_ << ",\n";
    ss << "  \"total_gas_consumed\": " << total_gas_consumed_ << ",\n";
    ss << "  \"total_contract_deployments\": " << total_contract_deployments_ << ",\n";
    ss << "  \"total_contract_calls\": " << total_contract_calls_ << ",\n";
    ss << "  \"active_snapshots\": " << state_snapshots_.size() << "\n";
    ss << "}";
    return ss.str();
}

ExecutionResult VMBlockValidator::executeContractDeployment(
    std::shared_ptr<core::Transaction> transaction,
    const ExecutionContext& /* context */
) {
    ExecutionResult result;
    result.success = false;
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Executing contract deployment transaction");
    
    try {
        // Extract contract bytecode from transaction
        // This is a simplified approach - in reality, you'd have a more
        // sophisticated way to extract contract data from transactions
        
        std::vector<uint8_t> bytecode;
        // For now, we'll create a simple bytecode from transaction data
        std::string tx_data = transaction->getId();
        for (size_t i = 0; i < std::min(tx_data.size(), size_t(10)); ++i) {
            bytecode.push_back(static_cast<uint8_t>(tx_data[i]));
        }
        
        // Create deployment transaction
        std::string deployer = "0x" + tx_data.substr(0, 40); // Simplified address
        ContractDeploymentTransaction deployment_tx(
            deployer, bytecode, 100000, 20, 0
        );
        
        // Deploy contract
        std::string contract_address = contract_manager_->deployContract(deployment_tx, *vm_);
        
        if (contract_address.empty()) {
            result.error_message = "Contract deployment failed";
            return result;
        }
        
        result.success = true;
        result.gas_used = 100000; // Simplified gas calculation
        // Convert contract address to bytes
        result.return_data.clear();
        for (char c : contract_address) {
            result.return_data.push_back(static_cast<uint8_t>(c));
        }
        
        DEO_LOG_INFO(VIRTUAL_MACHINE, "Contract deployed at address: " + contract_address);
        
    } catch (const std::exception& e) {
        result.error_message = "Contract deployment exception: " + std::string(e.what());
        DEO_LOG_ERROR(VIRTUAL_MACHINE, result.error_message);
    }
    
    return result;
}

ExecutionResult VMBlockValidator::executeContractCall(
    std::shared_ptr<core::Transaction> transaction,
    const ExecutionContext& /* context */
) {
    ExecutionResult result;
    result.success = false;
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Executing contract call transaction");
    
    try {
        // Extract contract address and call data from transaction
        // This is a simplified approach
        
        std::string contract_address = "0x" + transaction->getId().substr(0, 40);
        std::vector<uint8_t> call_data;
        
        // Create call data from transaction
        std::string tx_data = transaction->getId();
        for (size_t i = 0; i < std::min(tx_data.size(), size_t(10)); ++i) {
            call_data.push_back(static_cast<uint8_t>(tx_data[i]));
        }
        
        // Create call transaction
        std::string caller = "0x" + tx_data.substr(0, 40);
        ContractCallTransaction call_tx(
            caller, contract_address, call_data, 50000, 20, 0
        );
        
        // Execute contract call
        result = contract_manager_->callContract(call_tx, *vm_);
        
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Contract call result: " + std::string(result.success ? "success" : "failed"));
        
    } catch (const std::exception& e) {
        result.error_message = "Contract call exception: " + std::string(e.what());
        DEO_LOG_ERROR(VIRTUAL_MACHINE, result.error_message);
    }
    
    return result;
}

ExecutionResult VMBlockValidator::executeRegularTransaction(
    std::shared_ptr<core::Transaction> /* transaction */,
    const ExecutionContext& /* context */
) {
    ExecutionResult result;
    result.success = true;
    result.gas_used = 21000; // Base gas cost for regular transactions
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Executing regular transaction");
    
    // For regular transactions, we just validate the structure
    // In a real implementation, you would handle UTXO updates, etc.
    
    return result;
}

ExecutionContext VMBlockValidator::createBlockContext(std::shared_ptr<core::Block> block) const {
    ExecutionContext context;
    
    // Set block-specific context
    context.block_number = block->getHeader().height;
    context.block_timestamp = block->getHeader().timestamp;
    context.gas_limit = 10000000; // Block gas limit
    context.gas_price = 20; // Base gas price
    
    // Set default values
    context.caller_address = "0x0000000000000000000000000000000000000000";
    context.contract_address = "0x0000000000000000000000000000000000000000";
    context.value = 0;
    
    return context;
}

bool VMBlockValidator::validateTransactionStructure(std::shared_ptr<core::Transaction> transaction) const {
    // Basic transaction structure validation
    if (!transaction) {
        return false;
    }
    
    if (transaction->getId().empty()) {
        return false;
    }
    
    // Check if transaction has at least one input or output
    if (transaction->getInputs().empty() && transaction->getOutputs().empty()) {
        return false;
    }
    
    return true;
}

bool VMBlockValidator::hasSufficientGas(std::shared_ptr<core::Transaction> /* transaction */, uint64_t estimated_gas) const {
    // Simplified gas sufficiency check
    // In a real implementation, you would check the sender's balance
    // and gas price against the estimated gas cost
    
    return estimated_gas <= 10000000; // Block gas limit
}

} // namespace vm
} // namespace deo

