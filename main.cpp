/**
 * @file main.cpp
 * @brief Main entry point for the Deo Blockchain implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "cli/command_parser.h"
#include "cli/commands.h"
#include "utils/logger.h"
#include "utils/config.h"
#include "utils/error_handler.h"

using namespace deo;

/**
 * @brief Main entry point for the Deo Blockchain CLI
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code
 */
int main(int argc, char* argv[]) {
    try {
        // Initialize error handling system
        deo::utils::ErrorHandler::initialize();
        
        // Initialize logger
        deo::utils::Logger::initialize(deo::utils::LogLevel::INFO);
        DEO_LOG_INFO(GENERAL, "Deo Blockchain v1.0.0 starting...");
        
        // Load configuration
        auto config = std::make_unique<deo::utils::Config>();
        if (!config->load("config.json")) {
            DEO_LOG_WARNING(CONFIGURATION, "Could not load config.json, using defaults");
        }
        
        // Parse command line arguments
        auto parser = std::make_unique<deo::cli::CommandParser>(argc, argv);
        auto commands = std::make_unique<deo::cli::Commands>();
        
        // Initialize commands
        commands->initialize();
        
        // Register commands with parser
        parser->registerCommand("create-tx", "Create a new transaction", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "create-tx";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("create-block", "Create a new block", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "create-block";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("mine-block", "Mine a block using Proof of Work", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "mine-block";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("validate-block", "Validate a block", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "validate-block";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("generate-keypair", "Generate a new key pair", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "generate-keypair";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("test-crypto", "Test cryptographic functions", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "test-crypto";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("test-merkle", "Test Merkle tree implementation", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "test-merkle";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("demo-end-to-end", "Demonstrate end-to-end blockchain flow", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "demo-end-to-end";
                result.args = args;
                return commands->execute(result);
            });
        
        // Networking commands
        parser->registerCommand("start-node", "Start a full node with P2P networking", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "start-node";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("connect-peer", "Connect to a peer", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "connect-peer";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("show-peers", "Show connected peers", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "show-peers";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("status", "Show node status", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "status";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("test-networking", "Test P2P networking components", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "test-networking";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("deploy-contract", "Deploy a smart contract", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "deploy-contract";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("call-contract", "Call a smart contract", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "call-contract";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("get-contract", "Get contract information", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "get-contract";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("test-vm", "Test Virtual Machine functionality", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "test-vm";
                result.args = args;
                return commands->execute(result);
            });
        
        // Node Runtime Commands
        parser->registerCommand("start-node", "Start the node runtime", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "start-node";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("stop-node", "Stop the node runtime", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "stop-node";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("node-status", "Get node status", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "node-status";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("replay-block", "Replay a block for debugging", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "replay-block";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("json-rpc-stats", "Get JSON-RPC server statistics", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "json-rpc-stats";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("test-determinism", "Run determinism tests for VM execution", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "test-determinism";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("new-block", "Create a new block from transaction pool", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "new-block";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("show-chain", "Show blockchain state and latest block", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "show-chain";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("tx-pool", "Show transaction pool", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "tx-pool";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("add-tx", "Add transaction to pool", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "add-tx";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("show-block", "Show block by hash or height", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "show-block";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("show-stats", "Show blockchain statistics", 
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "show-stats";
                result.args = args;
                return commands->execute(result);
            });
        
        // Multi-node P2P commands
        parser->registerCommand("broadcast-tx", "Broadcast transaction to network",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "broadcast-tx";
                result.args = args;
                return commands->execute(result);
            });
        parser->registerCommand("broadcast-block", "Broadcast block to network",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "broadcast-block";
                result.args = args;
                return commands->execute(result);
            });
        parser->registerCommand("sync-chain", "Synchronize blockchain with peers",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "sync-chain";
                result.args = args;
                return commands->execute(result);
            });
        
        // Contract CLI Commands
        parser->registerCommand("compile-contract", "Compile contract from source file",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "compile-contract";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("deploy-contract", "Deploy contract to blockchain",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "deploy-contract";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("call-contract", "Call contract function",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "call-contract";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("get-contract-info", "Get contract information",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "get-contract-info";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("list-contracts", "List deployed contracts",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "list-contracts";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("get-contract-abi", "Get contract ABI",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "get-contract-abi";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("get-contract-bytecode", "Get contract bytecode",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "get-contract-bytecode";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("get-contract-storage", "Get contract storage",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "get-contract-storage";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("set-contract-storage", "Set contract storage (debugging)",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "set-contract-storage";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("estimate-gas", "Estimate gas for contract call",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "estimate-gas";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("validate-source", "Validate contract source code",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "validate-source";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("create-template", "Create contract template",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "create-template";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("list-templates", "List available contract templates",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "list-templates";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("format-source", "Format contract source code",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "format-source";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("lint-source", "Lint contract source code",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "lint-source";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("generate-docs", "Generate contract documentation",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "generate-docs";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("get-contract-stats", "Get contract statistics",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "get-contract-stats";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("monitor-events", "Monitor contract events",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "monitor-events";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("get-contract-history", "Get contract transaction history",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "get-contract-history";
                result.args = args;
                return commands->execute(result);
            });
        
        parser->registerCommand("verify-contract", "Verify contract on blockchain",
            [&commands](const std::map<std::string, std::string>& args) -> int {
                deo::cli::CommandResult result;
                result.command = "verify-contract";
                result.args = args;
                return commands->execute(result);
            });
        
        // Execute the parsed command
        int result = parser->execute();
        
        // Shutdown commands
        commands->shutdown();
        
        DEO_LOG_INFO(GENERAL, "Deo Blockchain shutting down gracefully");
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        DEO_LOG_ERROR(GENERAL, "Fatal error: " + std::string(e.what()));
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        DEO_LOG_ERROR(GENERAL, "Unknown fatal error occurred");
        return 1;
    }
}
