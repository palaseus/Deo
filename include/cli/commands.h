/**
 * @file commands.h
 * @brief Command implementations for the Deo Blockchain CLI
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <map>
#include <functional>
#include <memory>
#include "core/blockchain.h"
#include "node/node_runtime.h"
#include "cli/contract_cli.h"
#include "wallet/wallet.h"

namespace deo {
namespace core {
    class Blockchain;
}

namespace cli {

/**
 * @brief Command result structure
 */
struct CommandResult {
    std::string command;                    ///< Command name
    std::map<std::string, std::string> args; ///< Command arguments
    std::vector<std::string> flags;         ///< Command flags
    std::string subcommand;                 ///< Subcommand name
};

/**
 * @brief Command handler function type
 */
using CommandHandler = std::function<int(const std::map<std::string, std::string>&)>;

/**
 * @brief Command implementations for the Deo Blockchain CLI
 * 
 * This class provides implementations for all CLI commands including
 * blockchain management, transaction handling, and system operations.
 */
class Commands {
public:
    /**
     * @brief Constructor
     */
    Commands();
    
    /**
     * @brief Destructor
     */
    ~Commands() = default;
    
    /**
     * @brief Initialize the commands system
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the commands system
     */
    void shutdown();
    
    /**
     * @brief Execute a command
     * @param result Command result to execute
     * @return Exit code
     */
    int execute(const CommandResult& result);
    
    /**
     * @brief Get help text for all commands
     * @return Help text string
     */
    std::string getHelp() const;
    
    // Disable copy and move semantics
    Commands(const Commands&) = delete;
    Commands& operator=(const Commands&) = delete;
    Commands(Commands&&) = delete;
    Commands& operator=(Commands&&) = delete;

    /**
     * @brief Initialize the blockchain
     * @param args Command arguments
     * @return Exit code
     */
    int init(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Start the blockchain node
     * @param args Command arguments
     * @return Exit code
     */
    int start(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Stop the blockchain node
     * @param args Command arguments
     * @return Exit code
     */
    int stop(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get blockchain status
     * @param args Command arguments
     * @return Exit code
     */
    int status(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get blockchain information
     * @param args Command arguments
     * @return Exit code
     */
    int info(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Create a new transaction
     * @param result Command result
     * @return Exit code
     */
    int createTransaction(const CommandResult& result);
    
    /**
     * @brief Send a transaction
     * @param args Command arguments
     * @return Exit code
     */
    int sendTransaction(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get transaction information
     * @param args Command arguments
     * @return Exit code
     */
    int getTransaction(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get block information
     * @param args Command arguments
     * @return Exit code
     */
    int getBlock(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get blockchain height
     * @param args Command arguments
     * @return Exit code
     */
    int getHeight(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get account balance
     * @param args Command arguments
     * @return Exit code
     */
    int getBalance(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Generate a new key pair
     * @param result Command result
     * @return Exit code
     */
    int generateKeyPair(const CommandResult& result);
    
    /**
     * @brief Create a new block
     * @param result Command result
     * @return Exit code
     */
    int createBlock(const CommandResult& result);
    
    /**
     * @brief Mine a block using Proof of Work
     * @param result Command result
     * @return Exit code
     */
    int mineBlock(const CommandResult& result);
    
    /**
     * @brief Validate a block
     * @param result Command result
     * @return Exit code
     */
    int validateBlock(const CommandResult& result);
    
    /**
     * @brief Test cryptographic functions
     * @param result Command result
     * @return Exit code
     */
    int testCrypto(const CommandResult& result);
    
    /**
     * @brief Test Merkle tree implementation
     * @param result Command result
     * @return Exit code
     */
    int testMerkleTree(const CommandResult& result);
    
    /**
     * @brief Demonstrate end-to-end blockchain flow
     * @param result Command result
     * @return Exit code
     */
    int demoEndToEnd(const CommandResult& result);
    
    /**
     * @brief Start a full node with networking
     * @param result Command result
     * @return Exit code
     */
    int startNode(const CommandResult& result);
    
    /**
     * @brief Connect to a peer
     * @param result Command result
     * @return Exit code
     */
    int connectPeer(const CommandResult& result);
    
    /**
     * @brief Show connected peers
     * @param result Command result
     * @return Exit code
     */
    int showPeers(const CommandResult& result);
    
    /**
     * @brief Show node status
     * @param result Command result
     * @return Exit code
     */
    int showStatus(const CommandResult& result);
    
    /**
     * @brief Test P2P networking
     * @param result Command result
     * @return Exit code
     */
    int testNetworking(const CommandResult& result);
    
    /**
     * @brief Get contract information (legacy)
     * @param result Command result
     * @return Exit code
     */
    int getContract(const CommandResult& result);
    
    /**
     * @brief Test VM functionality
     * @param result Command result
     * @return Exit code
     */
    int testVM(const CommandResult& result);
    
    
    /**
     * @brief Stop the node runtime
     * @param result Command result
     * @return Exit code
     */
    int stopNode(const CommandResult& result);
    
    /**
     * @brief Get node status
     * @param result Command result
     * @return Exit code
     */
    int nodeStatus(const CommandResult& result);
    
    /**
     * @brief Replay a block for debugging
     * @param result Command result
     * @return Exit code
     */
    int replayBlock(const CommandResult& result);
    
    /**
     * @brief Get JSON-RPC server statistics
     * @param result Command result
     * @return Exit code
     */
    int getJsonRpcStats(const CommandResult& result);
    
    /**
     * @brief Run determinism tests
     * @param result Command result
     * @return Exit code
     */
    int runDeterminismTests(const CommandResult& result);
    
    /**
     * @brief Create a new block from transaction pool
     * @param result Command result
     * @return Exit code
     */
    int newBlock(const CommandResult& result);
    
    /**
     * @brief Show blockchain state and latest block
     * @param result Command result
     * @return Exit code
     */
    int showChain(const CommandResult& result);
    
    /**
     * @brief Show transaction pool
     * @param result Command result
     * @return Exit code
     */
    int showTxPool(const CommandResult& result);
    
    /**
     * @brief Add transaction to pool
     * @param result Command result
     * @return Exit code
     */
    int addTransaction(const CommandResult& result);
    
    /**
     * @brief Show block by hash or height
     * @param result Command result
     * @return Exit code
     */
    int showBlock(const CommandResult& result);
    
    /**
     * @brief Show blockchain statistics
     * @param result Command result
     * @return Exit code
     */
    int showStats(const CommandResult& result);
    
    // Multi-node P2P commands
    int broadcastTransaction(const CommandResult& result);
    int broadcastBlock(const CommandResult& result);
    int syncChain(const CommandResult& result);
    
    
    /**
     * @brief Import a private key
     * @param args Command arguments
     * @return Exit code
     */
    int importKey(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Export a private key
     * @param args Command arguments
     * @return Exit code
     */
    int exportKey(const std::map<std::string, std::string>& args);
    
    /**
     * @brief List all accounts
     * @param args Command arguments
     * @return Exit code
     */
    int listAccounts(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Start mining
     * @param args Command arguments
     * @return Exit code
     */
    int startMining(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Stop mining
     * @param args Command arguments
     * @return Exit code
     */
    int stopMining(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get mining status
     * @param args Command arguments
     * @return Exit code
     */
    int getMiningStatus(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Connect to a peer
     * @param args Command arguments
     * @return Exit code
     */
    int connectPeer(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Disconnect from a peer
     * @param args Command arguments
     * @return Exit code
     */
    int disconnectPeer(const std::map<std::string, std::string>& args);
    
    /**
     * @brief List connected peers
     * @param args Command arguments
     * @return Exit code
     */
    int listPeers(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get network information
     * @param args Command arguments
     * @return Exit code
     */
    int getNetworkInfo(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Sync with the network
     * @param args Command arguments
     * @return Exit code
     */
    int sync(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Validate the blockchain
     * @param args Command arguments
     * @return Exit code
     */
    int validate(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Export blockchain data
     * @param args Command arguments
     * @return Exit code
     */
    int exportBlockchain(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Import blockchain data
     * @param args Command arguments
     * @return Exit code
     */
    int importBlockchain(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Reset the blockchain
     * @param args Command arguments
     * @return Exit code
     */
    int reset(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Show configuration
     * @param args Command arguments
     * @return Exit code
     */
    int showConfig(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Set configuration value
     * @param args Command arguments
     * @return Exit code
     */
    int setConfig(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get logs
     * @param args Command arguments
     * @return Exit code
     */
    int getLogs(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Clear logs
     * @param args Command arguments
     * @return Exit code
     */
    int clearLogs(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get system information
     * @param args Command arguments
     * @return Exit code
     */
    int getSystemInfo(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Get performance statistics
     * @param args Command arguments
     * @return Exit code
     */
    int getPerformanceStats(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Run diagnostics
     * @param args Command arguments
     * @return Exit code
     */
    int runDiagnostics(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Show help for a specific command
     * @param args Command arguments
     * @return Exit code
     */
    int showHelp(const std::map<std::string, std::string>& args);
    
    /**
     * @brief Show version information
     * @param args Command arguments
     * @return Exit code
     */
    int showVersion(const std::map<std::string, std::string>& args);
    
    // Contract CLI Commands
    
    /**
     * @brief Compile contract from source file
     * @param result Command result
     * @return Exit code
     */
    int compileContract(const CommandResult& result);
    
    /**
     * @brief Deploy contract to blockchain
     * @param result Command result
     * @return Exit code
     */
    int deployContract(const CommandResult& result);
    
    /**
     * @brief Call contract function
     * @param result Command result
     * @return Exit code
     */
    int callContract(const CommandResult& result);
    
    /**
     * @brief Get contract information
     * @param result Command result
     * @return Exit code
     */
    int getContractInfo(const CommandResult& result);
    
    /**
     * @brief List deployed contracts
     * @param result Command result
     * @return Exit code
     */
    int listContracts(const CommandResult& result);
    
    /**
     * @brief Get contract ABI
     * @param result Command result
     * @return Exit code
     */
    int getContractABI(const CommandResult& result);
    
    /**
     * @brief Get contract bytecode
     * @param result Command result
     * @return Exit code
     */
    int getContractBytecode(const CommandResult& result);
    
    /**
     * @brief Get contract storage
     * @param result Command result
     * @return Exit code
     */
    int getContractStorage(const CommandResult& result);
    
    /**
     * @brief Set contract storage (debugging)
     * @param result Command result
     * @return Exit code
     */
    int setContractStorage(const CommandResult& result);
    
    /**
     * @brief Estimate gas for contract call
     * @param result Command result
     * @return Exit code
     */
    int estimateGas(const CommandResult& result);
    
    /**
     * @brief Validate contract source code
     * @param result Command result
     * @return Exit code
     */
    int validateSourceCode(const CommandResult& result);
    
    /**
     * @brief Create contract template
     * @param result Command result
     * @return Exit code
     */
    int createContractTemplate(const CommandResult& result);
    
    /**
     * @brief List available contract templates
     * @param result Command result
     * @return Exit code
     */
    int listContractTemplates(const CommandResult& result);
    
    /**
     * @brief Format contract source code
     * @param result Command result
     * @return Exit code
     */
    int formatSourceCode(const CommandResult& result);
    
    /**
     * @brief Lint contract source code
     * @param result Command result
     * @return Exit code
     */
    int lintSourceCode(const CommandResult& result);
    
    /**
     * @brief Generate contract documentation
     * @param result Command result
     * @return Exit code
     */
    int generateDocumentation(const CommandResult& result);
    
    /**
     * @brief Get contract statistics
     * @param result Command result
     * @return Exit code
     */
    int getContractStatistics(const CommandResult& result);
    
    /**
     * @brief Monitor contract events
     * @param result Command result
     * @return Exit code
     */
    int monitorContractEvents(const CommandResult& result);
    
    /**
     * @brief Get contract transaction history
     * @param result Command result
     * @return Exit code
     */
    int getContractTransactionHistory(const CommandResult& result);
    
    /**
     * @brief Verify contract on blockchain
     * @param result Command result
     * @return Exit code
     */
    int verifyContract(const CommandResult& result);
    
    // Wallet commands
    /**
     * @brief Create a new wallet account
     * @param result Command result
     * @return Exit code
     */
    int walletCreateAccount(const CommandResult& result);
    
    /**
     * @brief Import wallet account from private key
     * @param result Command result
     * @return Exit code
     */
    int walletImportAccount(const CommandResult& result);
    
    /**
     * @brief List all wallet accounts
     * @param result Command result
     * @return Exit code
     */
    int walletListAccounts(const CommandResult& result);
    
    /**
     * @brief Export wallet account
     * @param result Command result
     * @return Exit code
     */
    int walletExportAccount(const CommandResult& result);
    
    /**
     * @brief Remove wallet account
     * @param result Command result
     * @return Exit code
     */
    int walletRemoveAccount(const CommandResult& result);
    
    /**
     * @brief Set default wallet account
     * @param result Command result
     * @return Exit code
     */
    int walletSetDefault(const CommandResult& result);

private:
    std::unique_ptr<core::Blockchain> blockchain_; ///< Blockchain instance
    std::unique_ptr<ContractCLI> contract_cli_; ///< Contract CLI manager
    std::unique_ptr<wallet::Wallet> wallet_; ///< Wallet instance
    
    /**
     * @brief Initialize blockchain if not already initialized
     * @return True if blockchain is ready
     */
    bool ensureBlockchainInitialized();
    
    /**
     * @brief Format output as JSON
     * @param data Data to format
     * @return JSON string
     */
    std::string formatJson(const std::map<std::string, std::string>& data) const;
    
    /**
     * @brief Format output as table
     * @param data Data to format
     * @return Table string
     */
    std::string formatTable(const std::map<std::string, std::string>& data) const;
    
    /**
     * @brief Print error message
     * @param message Error message
     */
    void printError(const std::string& message) const;
    
    /**
     * @brief Print success message
     * @param message Success message
     */
    void printSuccess(const std::string& message) const;
    
    /**
     * @brief Print information message
     * @param message Information message
     */
    void printInfo(const std::string& message) const;
    
    /**
     * @brief Validate required arguments
     * @param args Command arguments
     * @param required_args List of required argument names
     * @return True if all required arguments are present
     */
    bool validateRequiredArgs(const std::map<std::string, std::string>& args,
                             const std::vector<std::string>& required_args) const;
    
    /**
     * @brief Parse amount string to integer
     * @param amount_str Amount string
     * @return Amount in smallest unit
     */
    uint64_t parseAmount(const std::string& amount_str) const;
    
    /**
     * @brief Format amount from integer to string
     * @param amount Amount in smallest unit
     * @return Formatted amount string
     */
    std::string formatAmount(uint64_t amount) const;
    
    /**
     * @brief Validate address format
     * @param address Address to validate
     * @return True if address is valid
     */
    bool validateAddress(const std::string& address) const;
    
    /**
     * @brief Validate private key format
     * @param private_key Private key to validate
     * @return True if private key is valid
     */
    bool validatePrivateKey(const std::string& private_key) const;
    
    /**
     * @brief Get current timestamp
     * @return Current timestamp
     */
    uint64_t getCurrentTimestamp() const;
    
    /**
     * @brief Load configuration
     * @return True if configuration was loaded successfully
     */
    bool loadConfiguration();
    
    /**
     * @brief Save configuration
     * @return True if configuration was saved successfully
     */
    bool saveConfiguration();
    
    // Node runtime instance
    std::unique_ptr<node::NodeRuntime> node_runtime_;    ///< Global node runtime instance
};

} // namespace cli
} // namespace deo
