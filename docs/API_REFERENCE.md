# Deo Blockchain API Reference

## Overview

This document provides comprehensive API documentation for the Deo Blockchain system. The API is organized into logical modules with clear interfaces and comprehensive error handling.

## Core API

### Blockchain Class

The `Blockchain` class is the central component that manages the blockchain state and operations.

#### Constructor
```cpp
Blockchain(const BlockchainConfig& config);
```

#### Key Methods

##### Initialization
```cpp
bool initialize();
void shutdown();
bool isInitialized() const;
```

##### Block Operations
```cpp
bool addBlock(std::shared_ptr<Block> block);
std::shared_ptr<Block> getBlock(const std::string& hash);
std::shared_ptr<Block> getBlock(uint32_t height);
std::shared_ptr<Block> getGenesisBlock();
std::shared_ptr<Block> getBestBlock();
```

##### Transaction Operations
```cpp
bool addTransaction(std::shared_ptr<Transaction> transaction);
std::vector<std::shared_ptr<Transaction>> getMempoolTransactions();
bool removeTransaction(const std::string& transaction_id);
```

##### State Management
```cpp
BlockchainState getState() const;
uint32_t getHeight() const;
uint64_t getTotalDifficulty() const;
std::string getBestBlockHash() const;
```

##### Validation
```cpp
bool verifyBlockchain();
bool validateBlock(std::shared_ptr<Block> block);
bool validateTransaction(std::shared_ptr<Transaction> transaction);
```

##### Mining
```cpp
bool startMining();
void stopMining();
bool isMining() const;
MiningStats getMiningStats() const;
```

### Block Class

The `Block` class represents a block in the blockchain.

#### Constructor
```cpp
Block(const BlockHeader& header, const std::vector<std::shared_ptr<Transaction>>& transactions);
```

#### Key Methods

##### Block Information
```cpp
std::string getHash() const;
std::string calculateHash() const;
BlockHeader getHeader() const;
std::vector<std::shared_ptr<Transaction>> getTransactions() const;
```

##### Block Operations
```cpp
bool addTransaction(std::shared_ptr<Transaction> transaction);
bool removeTransaction(const std::string& transaction_id);
bool verify() const;
```

##### Serialization
```cpp
std::string serialize() const;
bool deserialize(const std::string& data);
```

### Transaction Class

The `Transaction` class represents a transaction in the blockchain.

#### Constructor
```cpp
Transaction();
```

#### Key Methods

##### Transaction Information
```cpp
std::string getId() const;
std::string getHash() const;
Transaction::Type getType() const;
std::vector<TransactionInput> getInputs() const;
std::vector<TransactionOutput> getOutputs() const;
```

##### Transaction Operations
```cpp
void addInput(const TransactionInput& input);
void addOutput(const TransactionOutput& output);
bool sign(const std::string& private_key);
bool verify() const;
bool validate() const;
```

##### Serialization
```cpp
std::string serialize() const;
bool deserialize(const std::string& data);
```

## Virtual Machine API

### VirtualMachine Class

The `VirtualMachine` class executes smart contracts and provides a secure execution environment.

#### Constructor
```cpp
VirtualMachine();
```

#### Key Methods

##### Execution
```cpp
ExecutionResult execute(const ExecutionContext& context);
bool validateBytecode(const std::vector<uint8_t>& bytecode);
```

##### Gas Management
```cpp
uint64_t getGasCost(Opcode opcode, const VMState& state);
uint64_t getTotalGasUsed() const;
uint64_t getTotalExecutions() const;
```

##### Statistics
```cpp
VMStatistics getStatistics() const;
void resetStatistics();
```

### ExecutionContext

The `ExecutionContext` provides execution context for smart contracts.

#### Constructor
```cpp
ExecutionContext(const std::vector<uint8_t>& code, uint64_t gas_limit, 
                const std::string& sender, const std::string& recipient);
```

#### Key Methods
```cpp
std::vector<uint8_t> getCode() const;
uint64_t getGasLimit() const;
std::string getSender() const;
std::string getRecipient() const;
```

## Consensus API

### ProofOfWork Class

The `ProofOfWork` class implements the Proof of Work consensus mechanism.

#### Constructor
```cpp
ProofOfWork(uint32_t initial_difficulty);
```

#### Key Methods

##### Initialization
```cpp
bool initialize();
void shutdown();
```

##### Mining
```cpp
bool startMining();
void stopMining();
bool isMining() const;
bool mineBlock(std::shared_ptr<Block> block, uint64_t max_nonce);
```

##### Validation
```cpp
bool validateBlock(std::shared_ptr<Block> block);
bool meetsDifficultyTarget(const std::string& hash, uint32_t difficulty);
```

##### Statistics
```cpp
MiningStats getMiningStatistics() const;
double getHashRate() const;
uint64_t getBlocksMined() const;
uint64_t getTotalHashes() const;
```

### ProofOfStake Class

The `ProofOfStake` class implements the Proof of Stake consensus mechanism.

#### Constructor
```cpp
ProofOfStake();
```

#### Key Methods

##### Initialization
```cpp
bool initialize();
void shutdown();
```

##### Validation
```cpp
bool validateBlock(std::shared_ptr<Block> block);
```

##### Statistics
```cpp
std::unordered_map<std::string, int> getStatistics() const;
```

## Network API

### NetworkManager Class

The `NetworkManager` class manages network operations and peer communication.

#### Constructor
```cpp
NetworkManager(const NetworkConfig& config);
```

#### Key Methods

##### Initialization
```cpp
bool initialize(std::shared_ptr<Blockchain> blockchain, 
                std::shared_ptr<ConsensusEngine> consensus_engine);
void shutdown();
```

##### Network Operations
```cpp
bool startNetwork();
void stopNetwork();
bool isNetworkActive() const;
```

##### Peer Management
```cpp
std::vector<Peer> getPeers() const;
bool addPeer(const std::string& address, uint16_t port);
bool removePeer(const std::string& peer_id);
```

##### Message Handling
```cpp
void broadcastBlock(std::shared_ptr<Block> block);
void broadcastTransaction(std::shared_ptr<Transaction> transaction);
```

### TcpNetworkManager Class

The `TcpNetworkManager` class provides TCP-based network communication.

#### Constructor
```cpp
TcpNetworkManager(const NetworkConfig& config);
```

#### Key Methods

##### Connection Management
```cpp
bool startListening(uint16_t port);
void stopListening();
bool connectToPeer(const std::string& address, uint16_t port);
void disconnectFromPeer(const std::string& peer_id);
```

##### Message Operations
```cpp
bool sendMessage(const std::string& peer_id, const NetworkMessage& message);
void setMessageHandler(std::function<void(const NetworkMessage&)> handler);
```

## Storage API

### BlockStorage Class

The `BlockStorage` class provides persistent storage for blocks.

#### Constructor
```cpp
BlockStorage(const std::string& data_directory);
```

#### Key Methods

##### Initialization
```cpp
bool initialize();
void shutdown();
```

##### Block Operations
```cpp
bool storeBlock(std::shared_ptr<Block> block);
std::shared_ptr<Block> getBlock(const std::string& hash);
std::shared_ptr<Block> getBlock(uint32_t height);
bool hasBlock(const std::string& hash);
```

##### Batch Operations
```cpp
std::vector<std::shared_ptr<Block>> getBlocks(uint32_t start_height, uint32_t end_height);
std::vector<std::string> getBlockHashes(uint32_t start_height, uint32_t end_height);
```

##### Statistics
```cpp
std::unordered_map<std::string, int> getStatistics() const;
```

### StateStorage Class

The `StateStorage` class provides persistent storage for account and contract states.

#### Constructor
```cpp
StateStorage(const std::string& data_directory);
```

#### Key Methods

##### Initialization
```cpp
bool initialize();
void shutdown();
```

##### Account Operations
```cpp
bool setAccountState(const std::string& address, const AccountState& state);
std::shared_ptr<AccountState> getAccountState(const std::string& address);
bool hasAccount(const std::string& address);
bool deleteAccount(const std::string& address);
```

##### Balance Operations
```cpp
uint64_t getBalance(const std::string& address);
bool setBalance(const std::string& address, uint64_t balance);
```

##### Contract Storage
```cpp
bool setStorageValue(const std::string& contract_address, 
                    const std::string& key, const std::string& value);
std::string getStorageValue(const std::string& contract_address, const std::string& key);
std::unordered_map<std::string, std::string> getAllStorage(const std::string& contract_address);
```

## Security API

### SecurityAuditor Class

The `SecurityAuditor` class provides comprehensive security auditing capabilities.

#### Constructor
```cpp
SecurityAuditor();
```

#### Key Methods

##### Audit Operations
```cpp
std::vector<SecurityAuditResult> performFullAudit();
std::vector<SecurityAuditResult> performComponentAudit(const std::string& component);
```

##### Security Monitoring
```cpp
void startSecurityMonitoring();
void stopSecurityMonitoring();
bool isSecurityMonitoringActive() const;
```

##### Threat Detection
```cpp
bool detectSuspiciousActivity(const std::string& activity_type, const std::string& details);
void reportSecurityIncident(const std::string& incident_type, const std::string& details);
```

##### Security Metrics
```cpp
std::unordered_map<std::string, int> getSecurityMetrics() const;
std::vector<std::string> getSecurityRecommendations() const;
```

## Performance API

### PerformanceOptimizer Class

The `PerformanceOptimizer` class provides performance optimization utilities.

#### Constructor
```cpp
PerformanceOptimizer();
```

#### Key Methods

##### Performance Monitoring
```cpp
void startMonitoring();
void stopMonitoring();
PerformanceMetrics getMetrics() const;
void recordOperation(bool success, double latency_ms);
```

##### Caching
```cpp
template<typename Key, typename Value>
PerformanceCache<Key, Value>& getCache(const std::string& name);
```

##### Thread Pool
```cpp
ThreadPool& getThreadPool();
```

##### Optimization
```cpp
void enableOptimizations(bool enable);
void optimizeMemoryUsage();
size_t getMemoryUsage() const;
```

## Error Handling

### Exception Types

The API uses standard C++ exceptions with custom error types:

- `BlockchainException`: Base exception for blockchain operations
- `ValidationException`: Exception for validation failures
- `NetworkException`: Exception for network operations
- `StorageException`: Exception for storage operations
- `SecurityException`: Exception for security violations

### Error Codes

Common error codes used throughout the API:

- `SUCCESS`: Operation completed successfully
- `INVALID_INPUT`: Invalid input parameters
- `VALIDATION_FAILED`: Validation failed
- `NETWORK_ERROR`: Network operation failed
- `STORAGE_ERROR`: Storage operation failed
- `SECURITY_VIOLATION`: Security violation detected
- `PERMISSION_DENIED`: Permission denied
- `RESOURCE_EXHAUSTED`: Resource exhausted
- `NOT_FOUND`: Resource not found
- `ALREADY_EXISTS`: Resource already exists

## Configuration

### BlockchainConfig

Configuration for the blockchain system:

```cpp
struct BlockchainConfig {
    std::string network_id;
    std::string data_directory;
    uint32_t block_time;
    uint32_t max_block_size;
    uint32_t max_transactions_per_block;
    uint32_t difficulty_adjustment_interval;
    uint64_t genesis_timestamp;
    std::string genesis_merkle_root;
    uint32_t initial_difficulty;
    bool enable_mining;
    bool enable_networking;
};
```

### NetworkConfig

Configuration for the network system:

```cpp
struct NetworkConfig {
    std::string listen_address;
    uint16_t listen_port;
    uint32_t max_connections;
    uint32_t connection_timeout_ms;
    uint32_t message_timeout_ms;
    bool enable_encryption;
    bool enable_compression;
};
```

## Usage Examples

### Basic Blockchain Operations

```cpp
// Create blockchain configuration
BlockchainConfig config;
config.network_id = "test_network";
config.data_directory = "/tmp/blockchain";
config.block_time = 600;
config.enable_mining = true;

// Create and initialize blockchain
auto blockchain = std::make_unique<Blockchain>(config);
blockchain->initialize();

// Create a transaction
auto transaction = std::make_shared<Transaction>();
transaction->addInput(TransactionInput("prev_tx_hash", 0, "signature", "public_key"));
transaction->addOutput(TransactionOutput(1000, "recipient_address"));
transaction->sign("private_key");

// Add transaction to blockchain
blockchain->addTransaction(transaction);

// Create and mine a block
auto block = std::make_shared<Block>(block_header, {transaction});
auto pow = std::make_unique<ProofOfWork>(1);
pow->initialize();
pow->mineBlock(block, 10000);

// Add block to blockchain
blockchain->addBlock(block);
```

### Smart Contract Execution

```cpp
// Create virtual machine
auto vm = std::make_unique<VirtualMachine>();

// Create execution context
ExecutionContext context(bytecode, 100000, "sender", "recipient");

// Execute smart contract
auto result = vm->execute(context);

if (result.success) {
    std::cout << "Contract executed successfully" << std::endl;
    std::cout << "Gas used: " << result.gas_used << std::endl;
} else {
    std::cout << "Contract execution failed: " << result.error_message << std::endl;
}
```

### Network Operations

```cpp
// Create network configuration
NetworkConfig network_config;
network_config.listen_address = "0.0.0.0";
network_config.listen_port = 8080;
network_config.max_connections = 100;

// Create network manager
auto network_manager = std::make_unique<NetworkManager>(network_config);
network_manager->initialize(blockchain, consensus_engine);

// Start network
network_manager->startNetwork();

// Broadcast a block
network_manager->broadcastBlock(block);
```

## Conclusion

This API reference provides comprehensive documentation for the Deo Blockchain system. The API is designed to be intuitive, well-documented, and easy to use while providing the flexibility and power needed for advanced blockchain applications.

For more detailed information about specific components, please refer to the individual module documentation and source code comments.
