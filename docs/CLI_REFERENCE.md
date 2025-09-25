# CLI Reference Documentation

## Overview

The Deo Blockchain provides a comprehensive command-line interface (CLI) for interacting with the blockchain system. The CLI supports all major blockchain operations including node management, transaction processing, smart contract deployment, and consensus operations.

## CLI Commands

### Node Management

#### Start Node
```bash
./bin/DeoBlockchain start-node [options]
```

**Options:**
- `--port <port>`: Specify the network port (default: 30303)
- `--rpc-port <port>`: Specify the RPC port (default: 8545)
- `--data-dir <path>`: Specify the data directory
- `--config <file>`: Specify configuration file

**Example:**
```bash
./bin/DeoBlockchain start-node --port 30303 --rpc-port 8545
```

#### Node Status
```bash
./bin/DeoBlockchain node-status
```

Displays current node status including:
- Network connectivity
- Block height
- Peer count
- Memory usage
- Storage statistics

#### Stop Node
```bash
./bin/DeoBlockchain stop-node
```

Gracefully shuts down the node and saves state.

### Blockchain Operations

#### View Blockchain Statistics
```bash
./bin/DeoBlockchain show-stats
```

Displays comprehensive blockchain statistics:
- Total blocks
- Block height
- Transaction count
- Storage usage
- Network metrics

#### Get Block Information
```bash
./bin/DeoBlockchain get-block <block_hash_or_height>
```

Retrieves and displays block information including:
- Block header details
- Transaction list
- Merkle root
- Timestamp

#### Get Transaction Information
```bash
./bin/DeoBlockchain get-transaction <tx_hash>
```

Displays transaction details:
- Inputs and outputs
- Gas usage
- Block inclusion
- Status

### Transaction Operations

#### Create Transaction
```bash
./bin/DeoBlockchain create-transaction --from <address> --to <address> --amount <value> [options]
```

**Options:**
- `--gas <gas>`: Specify gas limit
- `--gas-price <price>`: Specify gas price
- `--nonce <nonce>`: Specify transaction nonce

**Example:**
```bash
./bin/DeoBlockchain create-transaction --from 0x123... --to 0x456... --amount 1000 --gas 21000
```

#### Send Transaction
```bash
./bin/DeoBlockchain send-transaction <tx_data>
```

Broadcasts a transaction to the network.

#### Get Balance
```bash
./bin/DeoBlockchain get-balance <address>
```

Retrieves the balance of a specific address.

### Smart Contract Operations

#### Compile Contract
```bash
./bin/DeoBlockchain compile-contract --input <source_file> --output <bytecode_file> [options]
```

**Options:**
- `--optimize`: Enable bytecode optimization
- `--abi`: Generate ABI file
- `--gas-estimate`: Estimate gas requirements

**Example:**
```bash
./bin/DeoBlockchain compile-contract --input contract.sol --output contract.bin --optimize
```

#### Deploy Contract
```bash
./bin/DeoBlockchain deploy-contract --bytecode <bytecode_file> --gas <gas_limit> [options]
```

**Options:**
- `--constructor-args <args>`: Constructor arguments
- `--gas-price <price>`: Gas price for deployment

**Example:**
```bash
./bin/DeoBlockchain deploy-contract --bytecode contract.bin --gas 1000000
```

#### Call Contract Function
```bash
./bin/DeoBlockchain call-contract --address <contract_address> --function <function_name> --args <arguments> [options]
```

**Options:**
- `--value <value>`: Ether value to send
- `--gas <gas>`: Gas limit for call
- `--from <address>`: Calling address

**Example:**
```bash
./bin/DeoBlockchain call-contract --address 0x123... --function transfer --args "0x456...,1000"
```

### Consensus Operations

#### Proof-of-Stake Operations

##### Register Validator
```bash
./bin/DeoBlockchain register-validator --address <address> --stake <amount> [options]
```

**Options:**
- `--commission <rate>`: Validator commission rate
- `--description <text>`: Validator description

**Example:**
```bash
./bin/DeoBlockchain register-validator --address 0x123... --stake 1000000 --commission 0.05
```

##### Delegate Stake
```bash
./bin/DeoBlockchain delegate-stake --validator <validator_address> --amount <amount>
```

Delegates stake to a validator.

##### Undelegate Stake
```bash
./bin/DeoBlockchain undelegate-stake --validator <validator_address> --amount <amount>
```

Undelegates stake from a validator.

##### View Validator Information
```bash
./bin/DeoBlockchain validator-info --address <validator_address>
```

Displays validator details:
- Stake amount
- Commission rate
- Delegation count
- Performance metrics

##### View Stake Distribution
```bash
./bin/DeoBlockchain stake-distribution
```

Shows the distribution of stake across all validators.

##### View PoS Statistics
```bash
./bin/DeoBlockchain pos-statistics
```

Displays Proof-of-Stake consensus statistics.

#### Proof-of-Work Operations

##### Start Mining
```bash
./bin/DeoBlockchain start-mining [options]
```

**Options:**
- `--threads <count>`: Number of mining threads
- `--difficulty <level>`: Mining difficulty

##### Stop Mining
```bash
./bin/DeoBlockchain stop-mining
```

Stops the mining process.

##### Mining Status
```bash
./bin/DeoBlockchain mining-status
```

Shows current mining status and statistics.

### Network Operations

#### Connect to Peer
```bash
./bin/DeoBlockchain connect-peer --address <peer_address> --port <peer_port>
```

Manually connect to a specific peer.

#### List Peers
```bash
./bin/DeoBlockchain list-peers
```

Displays connected peers and their status.

#### Network Statistics
```bash
./bin/DeoBlockchain network-stats
```

Shows network performance metrics.

### Storage Operations

#### Backup Blockchain
```bash
./bin/DeoBlockchain backup --output <backup_file> [options]
```

**Options:**
- `--compression`: Enable compression
- `--incremental`: Create incremental backup

#### Restore Blockchain
```bash
./bin/DeoBlockchain restore --input <backup_file>
```

Restores blockchain from backup.

#### Compact Storage
```bash
./bin/DeoBlockchain compact-storage
```

Optimizes storage by removing unnecessary data.

### Testing and Debugging

#### Run Tests
```bash
./bin/DeoBlockchain_tests [options]
```

**Options:**
- `--gtest_filter=<filter>`: Run specific tests
- `--gtest_output=xml:<file>`: Output test results to XML

**Examples:**
```bash
# Run all tests
./bin/DeoBlockchain_tests

# Run specific test suite
./bin/DeoBlockchain_tests --gtest_filter="VMTest.*"

# Run with XML output
./bin/DeoBlockchain_tests --gtest_output=xml:test_results.xml
```

#### Performance Testing
```bash
./bin/DeoBlockchain test-performance [options]
```

**Options:**
- `--duration <seconds>`: Test duration
- `--threads <count>`: Number of test threads
- `--transactions <count>`: Number of transactions to process

#### Determinism Testing
```bash
./bin/DeoBlockchain test-determinism
```

Tests deterministic execution across multiple runs.

### Configuration

#### Configuration File
The CLI supports configuration files in JSON format:

```json
{
  "network": {
    "port": 30303,
    "rpc_port": 8545,
    "max_peers": 50
  },
  "consensus": {
    "type": "proof_of_stake",
    "min_stake": 1000000,
    "epoch_length": 100
  },
  "storage": {
    "data_dir": "./data",
    "cache_size": 1024,
    "compression": true
  }
}
```

#### Environment Variables
- `DEO_DATA_DIR`: Data directory path
- `DEO_LOG_LEVEL`: Logging level (DEBUG, INFO, WARN, ERROR)
- `DEO_NETWORK_PORT`: Network port
- `DEO_RPC_PORT`: RPC port

### Error Handling

The CLI provides comprehensive error handling with:
- Clear error messages
- Exit codes for programmatic use
- Detailed logging options
- Debug mode for troubleshooting

### Exit Codes
- `0`: Success
- `1`: General error
- `2`: Configuration error
- `3`: Network error
- `4`: Consensus error
- `5`: Storage error

### Examples

#### Complete Workflow
```bash
# Start node
./bin/DeoBlockchain start-node --port 30303

# Check status
./bin/DeoBlockchain node-status

# Create and send transaction
./bin/DeoBlockchain create-transaction --from 0x123... --to 0x456... --amount 1000
./bin/DeoBlockchain send-transaction <tx_data>

# Deploy smart contract
./bin/DeoBlockchain compile-contract --input contract.sol --output contract.bin
./bin/DeoBlockchain deploy-contract --bytecode contract.bin --gas 1000000

# Register as validator
./bin/DeoBlockchain register-validator --address 0x123... --stake 1000000

# View statistics
./bin/DeoBlockchain show-stats
./bin/DeoBlockchain pos-statistics
```

#### Testing Workflow
```bash
# Run comprehensive tests
./bin/DeoBlockchain_tests

# Run specific tests
./bin/DeoBlockchain_tests --gtest_filter="*Consensus*"

# Performance testing
./bin/DeoBlockchain test-performance --duration 60 --threads 4

# Determinism testing
./bin/DeoBlockchain test-determinism
```

This CLI provides a complete interface for all blockchain operations, making it suitable for both development and production use.
