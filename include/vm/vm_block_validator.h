/**
 * @file vm_block_validator.h
 * @brief VM-integrated block validator for smart contract execution
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>

#include "core/block.h"
#include "core/transaction.h"
#include "vm/virtual_machine.h"
#include "vm/smart_contract_manager.h"
#include "vm/state_store.h"

namespace deo {
namespace vm {

/**
 * @brief Result of VM block validation
 */
struct VMBlockValidationResult {
    bool success;                           ///< Whether validation succeeded
    std::string error_message;              ///< Error message if validation failed
    uint64_t total_gas_used;                ///< Total gas used by all transactions
    std::map<std::string, uint64_t> gas_used_per_tx; ///< Gas used per transaction
    std::vector<std::string> executed_contracts;      ///< List of contracts executed
    std::map<std::string, std::string> state_changes; ///< State changes made
};

/**
 * @brief VM-integrated block validator
 * 
 * This class integrates the Virtual Machine with block validation,
 * ensuring that all smart contract transactions are executed deterministically
 * and that state changes are properly committed only if the block is valid.
 */
class VMBlockValidator {
public:
    /**
     * @brief Constructor
     * @param state_store Shared pointer to the state store
     */
    explicit VMBlockValidator(std::shared_ptr<StateStore> state_store);
    
    /**
     * @brief Destructor
     */
    ~VMBlockValidator();
    
    /**
     * @brief Validate a block with VM execution
     * @param block Block to validate
     * @param previous_state_hash Hash of the previous block's state (for determinism)
     * @return Validation result
     */
    VMBlockValidationResult validateBlock(
        std::shared_ptr<core::Block> block,
        const std::string& previous_state_hash = ""
    );
    
    /**
     * @brief Execute a single transaction in the VM
     * @param transaction Transaction to execute
     * @param block_context Block context (height, timestamp, etc.)
     * @return Execution result
     */
    ExecutionResult executeTransaction(
        std::shared_ptr<core::Transaction> transaction,
        const ExecutionContext& block_context
    );
    
    /**
     * @brief Check if a transaction is a smart contract transaction
     * @param transaction Transaction to check
     * @return True if it's a contract transaction
     */
    bool isContractTransaction(std::shared_ptr<core::Transaction> transaction) const;
    
    /**
     * @brief Get the smart contract manager
     * @return Reference to the contract manager
     */
    SmartContractManager& getContractManager();
    
    /**
     * @brief Get the state store
     * @return Shared pointer to the state store
     */
    std::shared_ptr<StateStore> getStateStore();
    
    /**
     * @brief Create a snapshot of the current state
     * @return State snapshot identifier
     */
    std::string createStateSnapshot();
    
    /**
     * @brief Restore state from a snapshot
     * @param snapshot_id Snapshot identifier
     * @return True if restoration was successful
     */
    bool restoreStateSnapshot(const std::string& snapshot_id);
    
    /**
     * @brief Get validation statistics
     * @return JSON string with statistics
     */
    std::string getValidationStatistics() const;

private:
    std::shared_ptr<StateStore> state_store_;           ///< State store for persistence
    std::unique_ptr<SmartContractManager> contract_manager_; ///< Contract manager
    std::unique_ptr<VirtualMachine> vm_;                ///< Virtual machine instance
    
    // Statistics
    mutable uint64_t total_blocks_validated_;           ///< Total blocks validated
    mutable uint64_t total_transactions_executed_;      ///< Total transactions executed
    mutable uint64_t total_gas_consumed_;               ///< Total gas consumed
    mutable uint64_t total_contract_deployments_;       ///< Total contract deployments
    mutable uint64_t total_contract_calls_;             ///< Total contract calls
    
    // State snapshots for rollback capability
    std::map<std::string, std::string> state_snapshots_; ///< State snapshots
    
    /**
     * @brief Initialize the validator
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Execute contract deployment transaction
     * @param transaction Transaction to execute
     * @param context Execution context
     * @return Execution result
     */
    ExecutionResult executeContractDeployment(
        std::shared_ptr<core::Transaction> transaction,
        const ExecutionContext& context
    );
    
    /**
     * @brief Execute contract call transaction
     * @param transaction Transaction to execute
     * @param context Execution context
     * @return Execution result
     */
    ExecutionResult executeContractCall(
        std::shared_ptr<core::Transaction> transaction,
        const ExecutionContext& context
    );
    
    /**
     * @brief Execute regular transaction (non-contract)
     * @param transaction Transaction to execute
     * @param context Execution context
     * @return Execution result
     */
    ExecutionResult executeRegularTransaction(
        std::shared_ptr<core::Transaction> transaction,
        const ExecutionContext& context
    );
    
    /**
     * @brief Create execution context from block
     * @param block Block to create context from
     * @return Execution context
     */
    ExecutionContext createBlockContext(std::shared_ptr<core::Block> block) const;
    
    /**
     * @brief Validate transaction signature and structure
     * @param transaction Transaction to validate
     * @return True if valid
     */
    bool validateTransactionStructure(std::shared_ptr<core::Transaction> transaction) const;
    
    /**
     * @brief Check if transaction has sufficient gas
     * @param transaction Transaction to check
     * @param estimated_gas Estimated gas requirement
     * @return True if sufficient gas
     */
    bool hasSufficientGas(std::shared_ptr<core::Transaction> transaction, uint64_t estimated_gas) const;
};

} // namespace vm
} // namespace deo

