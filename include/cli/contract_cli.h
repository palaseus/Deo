/**
 * @file contract_cli.h
 * @brief CLI tools for contract deployment and debugging
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <cstdint>

#include "compiler/contract_compiler.h"
#include "vm/smart_contract_manager.h"
#include "core/blockchain.h"
#include "core/transaction.h"
#include "crypto/key_pair.h"

namespace deo {
namespace cli {

/**
 * @brief Contract deployment result
 */
struct ContractDeploymentResult {
    bool success = false;
    std::string contract_address;
    std::string transaction_hash;
    std::string bytecode_hash;
    uint64_t gas_used = 0;
    uint64_t gas_price = 0;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::string abi;
    std::string metadata;
};

/**
 * @brief Contract call result
 */
struct ContractCallResult {
    bool success = false;
    std::string return_data;
    uint64_t gas_used = 0;
    std::vector<std::string> errors;
    std::vector<std::string> logs;
    std::string transaction_hash;
};

/**
 * @brief Contract debugging information
 */
struct ContractDebugInfo {
    std::string contract_address;
    std::string source_code;
    std::string bytecode;
    std::string abi;
    std::vector<std::string> function_names;
    std::unordered_map<std::string, std::string> storage;
    uint64_t balance = 0;
    uint32_t nonce = 0;
    std::string creator;
    uint64_t creation_block = 0;
    std::string creation_tx_hash;
};

/**
 * @brief Contract CLI manager
 */
class ContractCLI {
public:
    explicit ContractCLI(std::shared_ptr<core::Blockchain> blockchain,
                        std::shared_ptr<vm::SmartContractManager> contract_manager);
    
    /**
     * @brief Compile contract from source file
     * @param source_file Path to source file
     * @param output_dir Output directory for compiled artifacts
     * @return Compilation result
     */
    compiler::CompilationResult compileContract(const std::string& source_file,
                                               const std::string& output_dir = "");
    
    /**
     * @brief Deploy contract to blockchain
     * @param bytecode_file Path to compiled bytecode file
     * @param abi_file Path to ABI file
     * @param deployer_key Deployer's private key
     * @param gas_limit Gas limit for deployment
     * @param gas_price Gas price for deployment
     * @return Deployment result
     */
    ContractDeploymentResult deployContract(const std::string& bytecode_file,
                                           const std::string& abi_file,
                                           const std::string& deployer_key,
                                           uint64_t gas_limit = 1000000,
                                           uint64_t gas_price = 20000000000);
    
    /**
     * @brief Call contract function
     * @param contract_address Contract address
     * @param function_name Function name to call
     * @param args Function arguments (JSON array)
     * @param caller_key Caller's private key
     * @param value ETH value to send (for payable functions)
     * @param gas_limit Gas limit for call
     * @param gas_price Gas price for call
     * @return Call result
     */
    ContractCallResult callContract(const std::string& contract_address,
                                   const std::string& function_name,
                                   const std::string& args,
                                   const std::string& caller_key,
                                   uint64_t value = 0,
                                   uint64_t gas_limit = 100000,
                                   uint64_t gas_price = 20000000000);
    
    /**
     * @brief Get contract debugging information
     * @param contract_address Contract address
     * @return Debug information
     */
    ContractDebugInfo getContractDebugInfo(const std::string& contract_address);
    
    /**
     * @brief List deployed contracts
     * @return List of contract addresses
     */
    std::vector<std::string> listContracts();
    
    /**
     * @brief Get contract ABI
     * @param contract_address Contract address
     * @return Contract ABI (JSON)
     */
    std::string getContractABI(const std::string& contract_address);
    
    /**
     * @brief Get contract bytecode
     * @param contract_address Contract address
     * @return Contract bytecode (hex)
     */
    std::string getContractBytecode(const std::string& contract_address);
    
    /**
     * @brief Get contract storage
     * @param contract_address Contract address
     * @param key Storage key
     * @return Storage value
     */
    std::string getContractStorage(const std::string& contract_address,
                                  const std::string& key);
    
    /**
     * @brief Set contract storage (for debugging)
     * @param contract_address Contract address
     * @param key Storage key
     * @param value Storage value
     * @return True if successful
     */
    bool setContractStorage(const std::string& contract_address,
                           const std::string& key,
                           const std::string& value);
    
    /**
     * @brief Estimate gas for contract call
     * @param contract_address Contract address
     * @param function_name Function name
     * @param args Function arguments
     * @param caller_address Caller address
     * @param value ETH value to send
     * @return Gas estimate
     */
    uint64_t estimateGas(const std::string& contract_address,
                        const std::string& function_name,
                        const std::string& args,
                        const std::string& caller_address,
                        uint64_t value = 0);
    
    /**
     * @brief Validate contract source code
     * @param source_code Source code to validate
     * @return True if valid
     */
    bool validateSourceCode(const std::string& source_code);
    
    /**
     * @brief Get contract compilation errors
     * @param source_code Source code to check
     * @return List of compilation errors
     */
    std::vector<std::string> getCompilationErrors(const std::string& source_code);
    
    /**
     * @brief Get supported contract features
     * @return List of supported features
     */
    std::vector<std::string> getSupportedFeatures();
    
    /**
     * @brief Get contract compiler version
     * @return Compiler version
     */
    std::string getCompilerVersion();
    
    /**
     * @brief Create contract template
     * @param template_name Template name
     * @param output_file Output file path
     * @return True if successful
     */
    bool createContractTemplate(const std::string& template_name,
                               const std::string& output_file);
    
    /**
     * @brief Get available contract templates
     * @return List of available templates
     */
    std::vector<std::string> getAvailableTemplates();
    
    /**
     * @brief Format contract source code
     * @param source_code Source code to format
     * @return Formatted source code
     */
    std::string formatSourceCode(const std::string& source_code);
    
    /**
     * @brief Lint contract source code
     * @param source_code Source code to lint
     * @return List of linting issues
     */
    std::vector<std::string> lintSourceCode(const std::string& source_code);
    
    /**
     * @brief Generate contract documentation
     * @param source_code Source code
     * @return Generated documentation (Markdown)
     */
    std::string generateDocumentation(const std::string& source_code);
    
    /**
     * @brief Get contract statistics
     * @param contract_address Contract address
     * @return Contract statistics (JSON)
     */
    std::string getContractStatistics(const std::string& contract_address);
    
    /**
     * @brief Monitor contract events
     * @param contract_address Contract address
     * @param event_name Event name (optional)
     * @param callback Event callback function
     * @return True if monitoring started
     */
    bool monitorContractEvents(const std::string& contract_address,
                              const std::string& event_name,
                              std::function<void(const std::string&)> callback);
    
    /**
     * @brief Stop monitoring contract events
     * @param contract_address Contract address
     * @return True if monitoring stopped
     */
    bool stopMonitoringEvents(const std::string& contract_address);
    
    /**
     * @brief Get contract transaction history
     * @param contract_address Contract address
     * @param limit Maximum number of transactions
     * @return Transaction history (JSON)
     */
    std::string getContractTransactionHistory(const std::string& contract_address,
                                             uint32_t limit = 100);
    
    /**
     * @brief Verify contract on blockchain
     * @param contract_address Contract address
     * @param source_code Source code
     * @param compiler_version Compiler version used
     * @return True if verification successful
     */
    bool verifyContract(const std::string& contract_address,
                       const std::string& source_code,
                       const std::string& compiler_version);

private:
    std::shared_ptr<core::Blockchain> blockchain_;
    std::shared_ptr<vm::SmartContractManager> contract_manager_;
    std::unique_ptr<compiler::ContractCompiler> compiler_;
    std::unordered_map<std::string, std::function<void(const std::string&)>> event_monitors_;
    
    /**
     * @brief Create deployment transaction
     * @param bytecode Contract bytecode
     * @param deployer_key Deployer's private key
     * @param gas_limit Gas limit
     * @param gas_price Gas price
     * @return Deployment transaction
     */
    std::shared_ptr<core::Transaction> createDeploymentTransaction(
        const std::vector<uint8_t>& bytecode,
        const std::string& deployer_key,
        uint64_t gas_limit,
        uint64_t gas_price);
    
    /**
     * @brief Create contract call transaction
     * @param contract_address Contract address
     * @param function_data Function call data
     * @param caller_key Caller's private key
     * @param value ETH value
     * @param gas_limit Gas limit
     * @param gas_price Gas price
     * @return Call transaction
     */
    std::shared_ptr<core::Transaction> createCallTransaction(
        const std::string& contract_address,
        const std::vector<uint8_t>& function_data,
        const std::string& caller_key,
        uint64_t value,
        uint64_t gas_limit,
        uint64_t gas_price);
    
    /**
     * @brief Encode function call data
     * @param function_name Function name
     * @param args Function arguments
     * @return Encoded function data
     */
    std::vector<uint8_t> encodeFunctionCall(const std::string& function_name,
                                           const std::string& args);
    
    /**
     * @brief Decode function return data
     * @param return_data Return data from contract
     * @param return_type Return type
     * @return Decoded return value
     */
    std::string decodeReturnData(const std::vector<uint8_t>& return_data,
                                const std::string& return_type);
    
    /**
     * @brief Generate contract address
     * @param deployer_address Deployer address
     * @param nonce Deployer nonce
     * @return Contract address
     */
    std::string generateContractAddress(const std::string& deployer_address,
                                       uint32_t nonce);
    
    /**
     * @brief Validate contract address
     * @param address Address to validate
     * @return True if valid
     */
    bool isValidContractAddress(const std::string& address);
    
    /**
     * @brief Get contract template content
     * @param template_name Template name
     * @return Template content
     */
    std::string getTemplateContent(const std::string& template_name);
    
    /**
     * @brief Parse function arguments
     * @param args JSON arguments string
     * @return Parsed arguments
     */
    std::vector<std::string> parseFunctionArguments(const std::string& args);
    
    /**
     * @brief Format function arguments
     * @param args Function arguments
     * @return Formatted arguments string
     */
    std::string formatFunctionArguments(const std::vector<std::string>& args);
    
    /**
     * @brief Calculate contract size
     * @param bytecode Contract bytecode
     * @return Contract size in bytes
     */
    uint32_t calculateContractSize(const std::vector<uint8_t>& bytecode);
    
    /**
     * @brief Estimate deployment gas
     * @param bytecode Contract bytecode
     * @return Gas estimate
     */
    uint64_t estimateDeploymentGas(const std::vector<uint8_t>& bytecode);
    
    /**
     * @brief Save compilation artifacts
     * @param result Compilation result
     * @param output_dir Output directory
     * @return True if successful
     */
    bool saveCompilationArtifacts(const compiler::CompilationResult& result,
                                 const std::string& output_dir);
    
    /**
     * @brief Load compilation artifacts
     * @param contract_address Contract address
     * @return Compilation artifacts
     */
    std::unordered_map<std::string, std::string> loadCompilationArtifacts(
        const std::string& contract_address);
};

} // namespace cli
} // namespace deo
