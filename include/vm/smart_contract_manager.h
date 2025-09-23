/**
 * @file smart_contract_manager.h
 * @brief Smart contract management system
 * @author Deo Blockchain Team
 * @date 2024
 */

#pragma once

#include "vm/smart_contract.h"
#include "vm/virtual_machine.h"
#include "vm/state_store.h"
#include <map>
#include <memory>
#include <mutex>

namespace deo {
namespace vm {

/**
 * @brief Smart contract manager for deploying and calling contracts
 */
class SmartContractManager {
public:
    /**
     * @brief Constructor
     * @param state_store Persistent state store
     */
    explicit SmartContractManager(std::shared_ptr<StateStore> state_store);
    
    /**
     * @brief Destructor
     */
    ~SmartContractManager();
    
    /**
     * @brief Deploy a smart contract
     * @param deployment_tx Deployment transaction
     * @param vm Virtual machine instance
     * @return Contract address if successful, empty string if failed
     */
    std::string deployContract(const ContractDeploymentTransaction& deployment_tx, VirtualMachine& vm);
    
    /**
     * @brief Call a smart contract
     * @param call_tx Call transaction
     * @param vm Virtual machine instance
     * @return Execution result
     */
    ExecutionResult callContract(const ContractCallTransaction& call_tx, VirtualMachine& vm);
    
    /**
     * @brief Get contract state
     * @param address Contract address
     * @return Contract state or nullptr if not found
     */
    std::shared_ptr<ContractState> getContractState(const std::string& address);
    
    /**
     * @brief Check if contract exists
     * @param address Contract address
     * @return True if contract exists
     */
    bool contractExists(const std::string& address);
    
    /**
     * @brief Get account balance
     * @param address Account address
     * @return Account balance
     */
    uint256_t getBalance(const std::string& address);
    
    /**
     * @brief Set account balance
     * @param address Account address
     * @param balance New balance
     * @return True if successful
     */
    bool setBalance(const std::string& address, const uint256_t& balance);
    
    /**
     * @brief Increment account nonce
     * @param address Account address
     * @return New nonce value
     */
    uint64_t incrementNonce(const std::string& address);
    
    /**
     * @brief Validate bytecode
     * @param bytecode Contract bytecode
     * @return True if valid
     */
    bool validateBytecode(const std::vector<uint8_t>& bytecode) const;
    
    /**
     * @brief Get contract statistics
     * @return Statistics string
     */
    std::string getStatistics() const;

private:
    std::shared_ptr<StateStore> state_store_;    ///< Persistent state store
    mutable std::mutex mutex_;                   ///< Thread safety mutex
    
    // Statistics
    mutable uint64_t total_deployments_;         ///< Total contract deployments
    mutable uint64_t total_calls_;               ///< Total contract calls
    mutable uint64_t total_gas_used_;            ///< Total gas used
    
    /**
     * @brief Generate contract address
     * @param deployer_address Deployer address
     * @param nonce Deployer nonce
     * @return Contract address
     */
    std::string generateContractAddress(const std::string& deployer_address, uint64_t nonce);
    
    /**
     * @brief Deduct gas cost from account
     * @param address Account address
     * @param gas_used Gas used
     * @param gas_price Gas price
     * @return True if successful
     */
    bool deductGasCost(const std::string& address, uint64_t gas_used, uint64_t gas_price);
    
    /**
     * @brief Transfer value between accounts
     * @param from_address From account
     * @param to_address To account
     * @param value Amount to transfer
     * @return True if successful
     */
    bool transferValue(const std::string& from_address, const std::string& to_address, const uint256_t& value);
};

} // namespace vm
} // namespace deo
