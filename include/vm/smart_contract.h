/**
 * @file smart_contract.h
 * @brief Smart contract system for the Deo blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#ifndef DEO_VM_SMART_CONTRACT_H
#define DEO_VM_SMART_CONTRACT_H

#include "vm/virtual_machine.h"
#include "vm/uint256.h"
#include "core/transaction.h"
#include "core/block.h"

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace deo {
namespace vm {

/**
 * @brief Smart contract deployment transaction
 */
class ContractDeploymentTransaction {
public:
    /**
     * @brief Constructor
     * @param deployer_address Address of the deployer
     * @param bytecode Contract bytecode
     * @param gas_limit Gas limit for deployment
     * @param gas_price Gas price
     * @param value Value to send with deployment
     */
    ContractDeploymentTransaction(
        const std::string& deployer_address,
        const std::vector<uint8_t>& bytecode,
        uint64_t gas_limit,
        uint64_t gas_price,
        uint64_t value = 0
    );
    
    /**
     * @brief Get contract bytecode
     * @return Contract bytecode
     */
    const std::vector<uint8_t>& getBytecode() const { return bytecode_; }
    
    /**
     * @brief Get contract address (calculated from deployer + nonce)
     * @return Contract address
     */
    std::string getContractAddress() const;
    
    /**
     * @brief Get deployer address
     * @return Deployer address
     */
    const std::string& getFromAddress() const { return from_address_; }
    
    /**
     * @brief Get value
     * @return Value
     */
    uint64_t getValue() const { return value_; }
    
    /**
     * @brief Get gas limit
     * @return Gas limit
     */
    uint64_t getGasLimit() const { return gas_limit_; }
    
    /**
     * @brief Get gas price
     * @return Gas price
     */
    uint64_t getGasPrice() const { return gas_price_; }
    
    /**
     * @brief Serialize transaction
     * @return Serialized data
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief Deserialize transaction
     * @param data Serialized data
     */
    bool deserialize(const std::vector<uint8_t>& data);
    
    /**
     * @brief Validate transaction
     * @return True if valid
     */
    bool validate() const;

private:
    std::string from_address_;      ///< Deployer address
    std::vector<uint8_t> bytecode_; ///< Contract bytecode
    uint64_t value_;                ///< Value to send
    uint64_t gas_limit_;            ///< Gas limit
    uint64_t gas_price_;            ///< Gas price
    uint64_t nonce_;                ///< Nonce
};

/**
 * @brief Smart contract call transaction
 */
class ContractCallTransaction {
public:
    /**
     * @brief Constructor
     * @param caller_address Address of the caller
     * @param contract_address Address of the contract
     * @param input_data Input data for contract call
     * @param gas_limit Gas limit for execution
     * @param gas_price Gas price
     * @param value Value to send with call
     */
    ContractCallTransaction(
        const std::string& caller_address,
        const std::string& contract_address,
        const std::vector<uint8_t>& input_data,
        uint64_t gas_limit,
        uint64_t gas_price,
        uint64_t value = 0
    );
    
    /**
     * @brief Get contract address
     * @return Contract address
     */
    const std::string& getContractAddress() const { return contract_address_; }
    
    /**
     * @brief Get input data
     * @return Input data
     */
    const std::vector<uint8_t>& getInputData() const { return input_data_; }
    
    /**
     * @brief Get caller address
     * @return Caller address
     */
    const std::string& getFromAddress() const { return from_address_; }
    
    /**
     * @brief Get value
     * @return Value
     */
    uint64_t getValue() const { return value_; }
    
    /**
     * @brief Get gas limit
     * @return Gas limit
     */
    uint64_t getGasLimit() const { return gas_limit_; }
    
    /**
     * @brief Get gas price
     * @return Gas price
     */
    uint64_t getGasPrice() const { return gas_price_; }
    
    /**
     * @brief Serialize transaction
     * @return Serialized data
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief Deserialize transaction
     * @param data Serialized data
     * @return True if successful
     */
    bool deserialize(const std::vector<uint8_t>& data);
    
    /**
     * @brief Validate transaction
     * @return True if valid
     */
    bool validate() const;

private:
    std::string from_address_;          ///< Caller address
    std::string contract_address_;      ///< Contract address
    std::vector<uint8_t> input_data_;   ///< Input data
    uint64_t value_;                    ///< Value to send
    uint64_t gas_limit_;                ///< Gas limit
    uint64_t gas_price_;                ///< Gas price
};

// ContractState is now defined in state_store.h

// SmartContractManager is now defined in smart_contract_manager.h

} // namespace vm
} // namespace deo

#endif // DEO_VM_SMART_CONTRACT_H