/**
 * @file contract_cli.cpp
 * @brief CLI tools for contract deployment and debugging implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "cli/contract_cli.h"
#include "utils/logger.h"
#include "crypto/hash.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>

namespace deo {
namespace cli {

ContractCLI::ContractCLI(std::shared_ptr<core::Blockchain> blockchain,
                        std::shared_ptr<vm::SmartContractManager> contract_manager)
    : blockchain_(blockchain), contract_manager_(contract_manager) {
    compiler_ = std::make_unique<compiler::ContractCompiler>();
}

compiler::CompilationResult ContractCLI::compileContract(const std::string& source_file,
                                                        const std::string& output_dir) {
    try {
        // Read source file
        std::ifstream file(source_file);
        if (!file.is_open()) {
            compiler::CompilationResult result;
            result.errors.push_back("Cannot open source file: " + source_file);
            return result;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source_code = buffer.str();
        file.close();
        
        // Compile contract
        auto result = compiler_->compile(source_code);
        
        // Save artifacts if output directory specified
        if (!output_dir.empty() && result.success) {
            saveCompilationArtifacts(result, output_dir);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        compiler::CompilationResult result;
        result.errors.push_back("Compilation error: " + std::string(e.what()));
        return result;
    }
}

ContractDeploymentResult ContractCLI::deployContract(const std::string& bytecode_file,
                                                    const std::string& abi_file,
                                                    const std::string& deployer_key,
                                                    uint64_t gas_limit,
                                                    uint64_t gas_price) {
    ContractDeploymentResult result;
    
    try {
        // Read bytecode file
        std::ifstream bytecode_stream(bytecode_file, std::ios::binary);
        if (!bytecode_stream.is_open()) {
            result.errors.push_back("Cannot open bytecode file: " + bytecode_file);
            return result;
        }
        
        std::vector<uint8_t> bytecode((std::istreambuf_iterator<char>(bytecode_stream)),
                                     std::istreambuf_iterator<char>());
        bytecode_stream.close();
        
        // Read ABI file
        std::ifstream abi_stream(abi_file);
        if (!abi_stream.is_open()) {
            result.errors.push_back("Cannot open ABI file: " + abi_file);
            return result;
        }
        
        std::stringstream abi_buffer;
        abi_buffer << abi_stream.rdbuf();
        result.abi = abi_buffer.str();
        abi_stream.close();
        
        // Create deployment transaction
        auto transaction = createDeploymentTransaction(bytecode, deployer_key, gas_limit, gas_price);
        if (!transaction) {
            result.errors.push_back("Failed to create deployment transaction");
            return result;
        }
        
        // Submit transaction to blockchain
        if (!blockchain_->addTransaction(transaction)) {
            result.errors.push_back("Failed to submit deployment transaction");
            return result;
        }
        
        // Generate contract address
        crypto::KeyPair deployer_keypair(deployer_key);
        std::string deployer_address = deployer_keypair.getPublicKey();
        uint32_t nonce = 0; // Simplified for now
        result.contract_address = generateContractAddress(deployer_address, nonce);
        
        // Set transaction details
        result.transaction_hash = transaction->calculateHash();
        result.bytecode_hash = deo::crypto::Hash::sha256(std::string(bytecode.begin(), bytecode.end()));
        result.gas_used = gas_limit; // Simplified
        result.gas_price = gas_price;
        result.success = true;
        
        DEO_LOG_INFO(BLOCKCHAIN, "Contract deployed successfully: " + result.contract_address);
        
    } catch (const std::exception& e) {
        result.errors.push_back("Deployment error: " + std::string(e.what()));
    }
    
    return result;
}

ContractCallResult ContractCLI::callContract(const std::string& contract_address,
                                           const std::string& function_name,
                                           const std::string& args,
                                           const std::string& caller_key,
                                           uint64_t value,
                                           uint64_t gas_limit,
                                           uint64_t gas_price) {
    ContractCallResult result;
    
    try {
        // Validate contract address
        if (!isValidContractAddress(contract_address)) {
            result.errors.push_back("Invalid contract address: " + contract_address);
            return result;
        }
        
        // Encode function call data
        auto function_data = encodeFunctionCall(function_name, args);
        if (function_data.empty()) {
            result.errors.push_back("Failed to encode function call data");
            return result;
        }
        
        // Create call transaction
        auto transaction = createCallTransaction(contract_address, function_data, caller_key,
                                               value, gas_limit, gas_price);
        if (!transaction) {
            result.errors.push_back("Failed to create call transaction");
            return result;
        }
        
        // Submit transaction to blockchain
        if (!blockchain_->addTransaction(transaction)) {
            result.errors.push_back("Failed to submit call transaction");
            return result;
        }
        
        // Execute contract call (simplified for now)
        result.success = true;
        result.return_data = "0x42"; // Placeholder
        
        result.transaction_hash = transaction->calculateHash();
        result.gas_used = gas_limit; // Simplified
        
        DEO_LOG_INFO(BLOCKCHAIN, "Contract call executed: " + function_name + " on " + contract_address);
        
    } catch (const std::exception& e) {
        result.errors.push_back("Call error: " + std::string(e.what()));
    }
    
    return result;
}

ContractDebugInfo ContractCLI::getContractDebugInfo(const std::string& contract_address) {
    ContractDebugInfo info;
    info.contract_address = contract_address;
    
    try {
        // Get contract state (simplified for now)
        info.bytecode = "0x6001600355"; // Placeholder bytecode
        info.balance = 0;
        info.nonce = 0;
        info.creator = "0x0000000000000000000000000000000000000000";
        info.creation_block = 0;
        info.creation_tx_hash = "0x0000000000000000000000000000000000000000000000000000000000000000";
        
        // Load compilation artifacts
        auto artifacts = loadCompilationArtifacts(contract_address);
        if (!artifacts.empty()) {
            info.source_code = artifacts["source"];
            info.abi = artifacts["abi"];
        }
        
        // Parse ABI to get function names
        if (!info.abi.empty()) {
            try {
                auto abi_json = nlohmann::json::parse(info.abi);
                for (const auto& item : abi_json) {
                    if (item.contains("name") && item.contains("type") && 
                        item["type"] == "function") {
                        info.function_names.push_back(item["name"]);
                    }
                }
            } catch (const std::exception& e) {
                DEO_LOG_WARNING(BLOCKCHAIN, "Failed to parse ABI: " + std::string(e.what()));
            }
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get contract debug info: " + std::string(e.what()));
    }
    
    return info;
}

std::vector<std::string> ContractCLI::listContracts() {
    std::vector<std::string> contracts;
    
    try {
        // Get all deployed contracts from blockchain (simplified for now)
        contracts.push_back("0x1234567890123456789012345678901234567890");
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to list contracts: " + std::string(e.what()));
    }
    
    return contracts;
}

std::string ContractCLI::getContractABI(const std::string& contract_address) {
    try {
        auto artifacts = loadCompilationArtifacts(contract_address);
        return artifacts["abi"];
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get contract ABI: " + std::string(e.what()));
        return "";
    }
}

std::string ContractCLI::getContractBytecode(const std::string& contract_address) {
    try {
        auto contract_state = contract_manager_->getContractState(contract_address);
        if (contract_state) {
            // Convert bytecode to hex string
            std::stringstream hex_stream;
            hex_stream << std::hex << std::setfill('0');
            for (uint8_t byte : contract_state->bytecode) {
                hex_stream << std::setw(2) << static_cast<int>(byte);
            }
            return hex_stream.str();
        }
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get contract bytecode: " + std::string(e.what()));
    }
    
    return "";
}

std::string ContractCLI::getContractStorage([[maybe_unused]] const std::string& contract_address,
                                          [[maybe_unused]] const std::string& key) {
    try {
        // Simplified storage access for now
        return "0x0000000000000000000000000000000000000000000000000000000000000000";
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get contract storage: " + std::string(e.what()));
    }
    
    return "";
}

bool ContractCLI::setContractStorage([[maybe_unused]] const std::string& contract_address,
                                   [[maybe_unused]] const std::string& key,
                                   [[maybe_unused]] const std::string& value) {
    try {
        // Simplified storage setting for now
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to set contract storage: " + std::string(e.what()));
        return false;
    }
}

uint64_t ContractCLI::estimateGas([[maybe_unused]] const std::string& contract_address,
                                 const std::string& function_name,
                                 const std::string& args,
                                 [[maybe_unused]] const std::string& caller_address,
                                 [[maybe_unused]] uint64_t value) {
    try {
        // Encode function call data
        auto function_data = encodeFunctionCall(function_name, args);
        if (function_data.empty()) {
            return 0;
        }
        
        // Simplified gas estimation for now
        return 21000 + function_data.size() * 200;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to estimate gas: " + std::string(e.what()));
        return 0;
    }
}

bool ContractCLI::validateSourceCode(const std::string& source_code) {
    return compiler_->validateSource(source_code);
}

std::vector<std::string> ContractCLI::getCompilationErrors(const std::string& source_code) {
    std::vector<std::string> errors;
    
    try {
        auto result = compiler_->compile(source_code);
        errors = result.errors;
    } catch (const std::exception& e) {
        errors.push_back("Compilation error: " + std::string(e.what()));
    }
    
    return errors;
}

std::vector<std::string> ContractCLI::getSupportedFeatures() {
    return compiler_->getSupportedFeatures();
}

std::string ContractCLI::getCompilerVersion() {
    return compiler_->getVersion();
}

bool ContractCLI::createContractTemplate(const std::string& template_name,
                                        const std::string& output_file) {
    try {
        std::string template_content = getTemplateContent(template_name);
        if (template_content.empty()) {
            return false;
        }
        
        std::ofstream file(output_file);
        if (!file.is_open()) {
            return false;
        }
        
        file << template_content;
        file.close();
        
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to create contract template: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> ContractCLI::getAvailableTemplates() {
    return {
        "simple_storage",
        "token_contract",
        "voting_contract",
        "auction_contract",
        "multisig_wallet",
        "escrow_contract"
    };
}

std::string ContractCLI::formatSourceCode(const std::string& source_code) {
    // Simple formatting - in a real implementation, this would be more sophisticated
    std::string formatted = source_code;
    
    // Basic indentation
    int indent_level = 0;
    std::string result;
    std::istringstream stream(formatted);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Remove leading whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        
        // Adjust indentation based on braces
        if (line.find('}') != std::string::npos) {
            indent_level = std::max(0, indent_level - 1);
        }
        
        // Add indentation
        result += std::string(indent_level * 4, ' ') + line + '\n';
        
        // Increase indentation for opening braces
        if (line.find('{') != std::string::npos) {
            indent_level++;
        }
    }
    
    return result;
}

std::vector<std::string> ContractCLI::lintSourceCode(const std::string& source_code) {
    std::vector<std::string> issues;
    
    // Basic linting rules
    if (source_code.empty()) {
        issues.push_back("Source code is empty");
        return issues;
    }
    
    // Check for common issues
    if (source_code.find("contract") == std::string::npos) {
        issues.push_back("No contract declaration found");
    }
    
    if (source_code.find("function") == std::string::npos) {
        issues.push_back("No function declarations found");
    }
    
    // Check for missing semicolons
    std::istringstream stream(source_code);
    std::string line;
    int line_number = 0;
    
    while (std::getline(stream, line)) {
        line_number++;
        
        // Remove comments
        size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // Check for statements that should end with semicolon
        if (!line.empty() && line.find_first_not_of(" \t") != std::string::npos) {
            if (line.find("return") != std::string::npos ||
                line.find("var") != std::string::npos ||
                line.find("const") != std::string::npos) {
                if (line.back() != ';' && line.back() != '{' && line.back() != '}') {
                    issues.push_back("Line " + std::to_string(line_number) + ": Missing semicolon");
                }
            }
        }
    }
    
    return issues;
}

std::string ContractCLI::generateDocumentation(const std::string& source_code) {
    std::stringstream doc;
    
    doc << "# Contract Documentation\n\n";
    
    // Extract contract name
    size_t contract_pos = source_code.find("contract");
    if (contract_pos != std::string::npos) {
        size_t name_start = source_code.find_first_not_of(" \t", contract_pos + 8);
        size_t name_end = source_code.find_first_of(" \t{", name_start);
        if (name_start != std::string::npos && name_end != std::string::npos) {
            std::string contract_name = source_code.substr(name_start, name_end - name_start);
            doc << "## Contract: " << contract_name << "\n\n";
        }
    }
    
    // Extract functions
    std::istringstream stream(source_code);
    std::string line;
    int line_number = 0;
    
    while (std::getline(stream, line)) {
        line_number++;
        
        if (line.find("function") != std::string::npos) {
            doc << "### Function (Line " << line_number << ")\n";
            doc << "```solidity\n" << line << "\n```\n\n";
        }
    }
    
    return doc.str();
}

std::string ContractCLI::getContractStatistics(const std::string& contract_address) {
    nlohmann::json stats;
    
    try {
        auto debug_info = getContractDebugInfo(contract_address);
        
        stats["contract_address"] = debug_info.contract_address;
        stats["balance"] = debug_info.balance;
        stats["nonce"] = debug_info.nonce;
        stats["creator"] = debug_info.creator;
        stats["creation_block"] = debug_info.creation_block;
        stats["creation_tx_hash"] = debug_info.creation_tx_hash;
        stats["function_count"] = debug_info.function_names.size();
        stats["storage_entries"] = debug_info.storage.size();
        stats["bytecode_size"] = debug_info.bytecode.size();
        
        // Get transaction history
        auto tx_history = getContractTransactionHistory(contract_address, 10);
        stats["recent_transactions"] = nlohmann::json::parse(tx_history);
        
    } catch (const std::exception& e) {
        stats["error"] = e.what();
    }
    
    return stats.dump(2);
}

bool ContractCLI::monitorContractEvents(const std::string& contract_address,
                                       [[maybe_unused]] const std::string& event_name,
                                       std::function<void(const std::string&)> callback) {
    try {
        event_monitors_[contract_address] = callback;
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to start event monitoring: " + std::string(e.what()));
        return false;
    }
}

bool ContractCLI::stopMonitoringEvents(const std::string& contract_address) {
    try {
        auto it = event_monitors_.find(contract_address);
        if (it != event_monitors_.end()) {
            event_monitors_.erase(it);
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to stop event monitoring: " + std::string(e.what()));
        return false;
    }
}

std::string ContractCLI::getContractTransactionHistory([[maybe_unused]] const std::string& contract_address,
                                                      [[maybe_unused]] uint32_t limit) {
    nlohmann::json history = nlohmann::json::array();
    
    try {
        // Simplified transaction history for now
        nlohmann::json transactions = nlohmann::json::array();
        
        // No transactions for now
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get transaction history: " + std::string(e.what()));
    }
    
    return history.dump(2);
}

bool ContractCLI::verifyContract(const std::string& contract_address,
                                const std::string& source_code,
                                [[maybe_unused]] const std::string& compiler_version) {
    try {
        // Compile source code
        auto compilation_result = compiler_->compile(source_code);
        if (!compilation_result.success) {
            return false;
        }
        
        // Get deployed bytecode
        std::string deployed_bytecode = getContractBytecode(contract_address);
        if (deployed_bytecode.empty()) {
            return false;
        }
        
        // Convert compiled bytecode to hex
        std::stringstream hex_stream;
        hex_stream << std::hex << std::setfill('0');
        for (uint8_t byte : compilation_result.bytecode) {
            hex_stream << std::setw(2) << static_cast<int>(byte);
        }
        std::string compiled_bytecode = hex_stream.str();
        
        // Compare bytecodes
        return deployed_bytecode == compiled_bytecode;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to verify contract: " + std::string(e.what()));
        return false;
    }
}

// Private helper methods

std::shared_ptr<core::Transaction> ContractCLI::createDeploymentTransaction(
    const std::vector<uint8_t>& bytecode,
    const std::string& deployer_key,
    [[maybe_unused]] uint64_t gas_limit,
    [[maybe_unused]] uint64_t gas_price) {
    
    try {
        crypto::KeyPair deployer_keypair(deployer_key);
        std::string deployer_address = deployer_keypair.getPublicKey();
        
        // Create transaction inputs
        std::vector<core::TransactionInput> inputs;
        core::TransactionInput input(
            "0000000000000000000000000000000000000000000000000000000000000000",
            0,
            "",
            deployer_address,
            0xFFFFFFFF
        );
        inputs.push_back(input);
        
        // Create transaction outputs (contract deployment)
        std::vector<core::TransactionOutput> outputs;
        core::TransactionOutput output(
            0, // No value for contract deployment
            "", // Contract address will be generated
            std::string(bytecode.begin(), bytecode.end()), // Bytecode as string
            0 // No script
        );
        outputs.push_back(output);
        
        auto transaction = std::make_shared<core::Transaction>(inputs, outputs, core::Transaction::Type::CONTRACT);
        transaction->setVersion(1);
        transaction->sign(deployer_keypair.getPrivateKey());
        
        return transaction;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to create deployment transaction: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<core::Transaction> ContractCLI::createCallTransaction(
    const std::string& contract_address,
    const std::vector<uint8_t>& function_data,
    const std::string& caller_key,
    uint64_t value,
    [[maybe_unused]] uint64_t gas_limit,
    [[maybe_unused]] uint64_t gas_price) {
    
    try {
        crypto::KeyPair caller_keypair(caller_key);
        std::string caller_address = caller_keypair.getPublicKey();
        
        // Create transaction inputs
        std::vector<core::TransactionInput> inputs;
        core::TransactionInput input(
            "0000000000000000000000000000000000000000000000000000000000000000",
            0,
            "",
            caller_address,
            0xFFFFFFFF
        );
        inputs.push_back(input);
        
        // Create transaction outputs
        std::vector<core::TransactionOutput> outputs;
        core::TransactionOutput output(
            value,
            contract_address,
            std::string(function_data.begin(), function_data.end()), // Convert to string
            0 // No script
        );
        outputs.push_back(output);
        
        auto transaction = std::make_shared<core::Transaction>(inputs, outputs, core::Transaction::Type::CONTRACT);
        transaction->setVersion(1);
        transaction->sign(caller_keypair.getPrivateKey());
        
        return transaction;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to create call transaction: " + std::string(e.what()));
        return nullptr;
    }
}

std::vector<uint8_t> ContractCLI::encodeFunctionCall(const std::string& function_name,
                                                    const std::string& args) {
    try {
        // Simple function encoding - in a real implementation, this would use proper ABI encoding
        std::vector<uint8_t> encoded;
        
        // Add function selector (first 4 bytes of function hash)
        std::string function_signature = function_name + "(" + args + ")";
        std::string function_hash = deo::crypto::Hash::sha256(function_signature);
        
        // Take first 4 bytes
        for (int i = 0; i < 4; ++i) {
            encoded.push_back(static_cast<uint8_t>(function_hash[i]));
        }
        
        // Add encoded arguments (simplified)
        auto parsed_args = parseFunctionArguments(args);
        for (const auto& arg : parsed_args) {
            // Simple string encoding
            for (char c : arg) {
                encoded.push_back(static_cast<uint8_t>(c));
            }
        }
        
        return encoded;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to encode function call: " + std::string(e.what()));
        return {};
    }
}

std::string ContractCLI::decodeReturnData(const std::vector<uint8_t>& return_data,
                                         const std::string& return_type) {
    try {
        // Simple decoding - in a real implementation, this would use proper ABI decoding
        if (return_type == "string") {
            return std::string(return_data.begin(), return_data.end());
        } else if (return_type == "uint256") {
            // Convert bytes to uint256 string representation
            deo::vm::uint256_t value = deo::vm::uint256_t(0);
            for (size_t i = 0; i < return_data.size() && i < 32; ++i) {
                value = value * deo::vm::uint256_t(256) + deo::vm::uint256_t(return_data[i]);
            }
            return value.toString();
        }
        
        return std::string(return_data.begin(), return_data.end());
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to decode return data: " + std::string(e.what()));
        return "";
    }
}

std::string ContractCLI::generateContractAddress(const std::string& deployer_address,
                                                uint32_t nonce) {
    try {
        // Generate contract address using deployer address and nonce
        std::string data = deployer_address + std::to_string(nonce);
        std::string hash = deo::crypto::Hash::sha256(data);
        
        // Take first 20 bytes and format as address
        return "0x" + hash.substr(0, 40);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to generate contract address: " + std::string(e.what()));
        return "";
    }
}

bool ContractCLI::isValidContractAddress(const std::string& address) {
    // Basic address validation
    if (address.empty() || address.length() < 10) {
        return false;
    }
    
    // Check if it starts with 0x
    if (address.substr(0, 2) != "0x") {
        return false;
    }
    
    // Check if remaining characters are valid hex
    std::string hex_part = address.substr(2);
    for (char c : hex_part) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    
    return true;
}

std::string ContractCLI::getTemplateContent(const std::string& template_name) {
    static const std::unordered_map<std::string, std::string> templates = {
        {"simple_storage", R"(
contract SimpleStorage {
    var value = 0;
    
    function set(uint256 newValue) {
        value = newValue;
    }
    
    function get() {
        return value;
    }
}
)"},
        {"token_contract", R"(
contract Token {
    var totalSupply = 1000000;
    var balances = {};
    
    function transfer(address to, uint256 amount) {
        if (balances[msg.sender] >= amount) {
            balances[msg.sender] = balances[msg.sender] - amount;
            balances[to] = balances[to] + amount;
        }
    }
    
    function balanceOf(address account) {
        return balances[account];
    }
}
)"},
        {"voting_contract", R"(
contract Voting {
    var candidates = {};
    var votes = {};
    
    function addCandidate(string name) {
        candidates[name] = true;
    }
    
    function vote(string candidate) {
        if (candidates[candidate]) {
            votes[candidate] = votes[candidate] + 1;
        }
    }
    
    function getVotes(string candidate) {
        return votes[candidate];
    }
}
)"}
    };
    
    auto it = templates.find(template_name);
    if (it != templates.end()) {
        return it->second;
    }
    
    return "";
}

std::vector<std::string> ContractCLI::parseFunctionArguments(const std::string& args) {
    std::vector<std::string> parsed_args;
    
    try {
        if (args.empty() || args == "[]") {
            return parsed_args;
        }
        
        // Simple parsing - in a real implementation, this would use proper JSON parsing
        std::string clean_args = args;
        if (clean_args.front() == '[' && clean_args.back() == ']') {
            clean_args = clean_args.substr(1, clean_args.length() - 2);
        }
        
        // Split by comma
        std::stringstream ss(clean_args);
        std::string arg;
        while (std::getline(ss, arg, ',')) {
            // Remove quotes and whitespace
            arg.erase(std::remove(arg.begin(), arg.end(), '"'), arg.end());
            arg.erase(std::remove(arg.begin(), arg.end(), ' '), arg.end());
            if (!arg.empty()) {
                parsed_args.push_back(arg);
            }
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to parse function arguments: " + std::string(e.what()));
    }
    
    return parsed_args;
}

std::string ContractCLI::formatFunctionArguments(const std::vector<std::string>& args) {
    if (args.empty()) {
        return "[]";
    }
    
    std::string result = "[";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += "\"" + args[i] + "\"";
    }
    result += "]";
    
    return result;
}

uint32_t ContractCLI::calculateContractSize(const std::vector<uint8_t>& bytecode) {
    return static_cast<uint32_t>(bytecode.size());
}

uint64_t ContractCLI::estimateDeploymentGas(const std::vector<uint8_t>& bytecode) {
    // Simple gas estimation
    uint64_t base_gas = 21000;
    uint64_t bytecode_gas = bytecode.size() * 200;
    return base_gas + bytecode_gas;
}

bool ContractCLI::saveCompilationArtifacts(const compiler::CompilationResult& result,
                                          const std::string& output_dir) {
    try {
        if (!std::filesystem::exists(output_dir)) {
            std::filesystem::create_directories(output_dir);
        }
        
        // Save bytecode
        std::ofstream bytecode_file(output_dir + "/bytecode.bin", std::ios::binary);
        if (bytecode_file.is_open()) {
            bytecode_file.write(reinterpret_cast<const char*>(result.bytecode.data()), result.bytecode.size());
            bytecode_file.close();
        }
        
        // Save ABI
        std::ofstream abi_file(output_dir + "/abi.json");
        if (abi_file.is_open()) {
            abi_file << result.abi;
            abi_file.close();
        }
        
        // Save metadata
        std::ofstream metadata_file(output_dir + "/metadata.json");
        if (metadata_file.is_open()) {
            metadata_file << result.metadata;
            metadata_file.close();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to save compilation artifacts: " + std::string(e.what()));
        return false;
    }
}

std::unordered_map<std::string, std::string> ContractCLI::loadCompilationArtifacts(
    const std::string& contract_address) {
    std::unordered_map<std::string, std::string> artifacts;
    
    try {
        std::string artifacts_dir = "./contracts/" + contract_address;
        
        // Load ABI
        std::ifstream abi_file(artifacts_dir + "/abi.json");
        if (abi_file.is_open()) {
            std::stringstream abi_buffer;
            abi_buffer << abi_file.rdbuf();
            artifacts["abi"] = abi_buffer.str();
            abi_file.close();
        }
        
        // Load source code
        std::ifstream source_file(artifacts_dir + "/source.sol");
        if (source_file.is_open()) {
            std::stringstream source_buffer;
            source_buffer << source_file.rdbuf();
            artifacts["source"] = source_buffer.str();
            source_file.close();
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to load compilation artifacts: " + std::string(e.what()));
    }
    
    return artifacts;
}

} // namespace cli
} // namespace deo
