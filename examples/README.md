# Deo Blockchain Examples

This directory contains examples and sample code for using the Deo blockchain system for research and development purposes.

## Directory Structure

```
examples/
├── README.md                 # This file
├── contracts/               # Smart contract examples
│   ├── simple_storage.sol   # Basic storage contract
│   ├── token.sol           # ERC-20 like token contract
│   └── voting.sol          # Simple voting contract
├── bytecode/               # Pre-compiled bytecode examples
│   ├── bytecode.bin        # Simple arithmetic operations
│   ├── bytecode_simple.bin # Basic stack operations
│   └── bytecode_clean.bin  # Clean execution example
├── scripts/                # Automation and testing scripts
│   ├── deploy_contract.sh  # Contract deployment script
│   ├── test_network.sh     # Network testing script
│   └── benchmark.sh        # Performance benchmarking
└── api/                    # API usage examples
    ├── web3_examples.py    # Python Web3 integration
    ├── curl_examples.sh    # Command-line API usage
    └── javascript_examples.js # JavaScript integration
```

## Smart Contract Examples

### Simple Storage Contract

A basic contract demonstrating state storage and retrieval:

```solidity
contract SimpleStorage {
    uint256 public storedData;
    
    function set(uint256 x) public {
        storedData = x;
    }
    
    function get() public view returns (uint256) {
        return storedData;
    }
}
```

### Token Contract

An ERC-20 compatible token implementation:

```solidity
contract Token {
    mapping(address => uint256) public balances;
    uint256 public totalSupply;
    
    function transfer(address to, uint256 amount) public returns (bool) {
        require(balances[msg.sender] >= amount);
        balances[msg.sender] -= amount;
        balances[to] += amount;
        return true;
    }
    
    function balanceOf(address account) public view returns (uint256) {
        return balances[account];
    }
}
```

### Voting Contract

A simple voting mechanism:

```solidity
contract Voting {
    mapping(bytes32 => uint256) public votes;
    mapping(address => bool) public hasVoted;
    
    function vote(bytes32 candidate) public {
        require(!hasVoted[msg.sender]);
        hasVoted[msg.sender] = true;
        votes[candidate]++;
    }
    
    function getVotes(bytes32 candidate) public view returns (uint256) {
        return votes[candidate];
    }
}
```

## Usage Examples

### Contract Compilation

```bash
# Compile a smart contract
./build/bin/deo compile-contract --input examples/contracts/simple_storage.sol --output examples/contracts/simple_storage.bin

# Validate source code
./build/bin/deo validate-source --input examples/contracts/token.sol
```

### Contract Deployment

```bash
# Deploy a contract
./build/bin/deo deploy-contract --bytecode examples/contracts/simple_storage.bin --gas 100000

# Get deployment information
./build/bin/deo get-contract-info --address 0x123...
```

### Contract Interaction

```bash
# Call a contract function
./build/bin/deo call-contract --address 0x123... --function set --args "42"

# Get contract storage
./build/bin/deo get-contract-storage --address 0x123... --key "0x0"
```

### Network Operations

```bash
# Start a node
./build/bin/deo start-node --port 8545

# Connect to a peer
./build/bin/deo connect-peer --address 127.0.0.1:8546

# Broadcast a transaction
./build/bin/deo broadcast-tx --transaction "0x..."
```

### Web3 API Usage

#### Python Example

```python
import requests
import json

def call_web3_method(method, params):
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": 1
    }
    
    response = requests.post(
        "http://localhost:8545",
        json=payload,
        headers={"Content-Type": "application/json"}
    )
    
    return response.json()

# Get current block number
result = call_web3_method("eth_blockNumber", [])
print(f"Current block: {result['result']}")

# Get account balance
balance = call_web3_method("eth_getBalance", ["0x123...", "latest"])
print(f"Balance: {balance['result']}")
```

#### JavaScript Example

```javascript
const Web3 = require('web3');

// Connect to Deo node
const web3 = new Web3('http://localhost:8545');

async function interactWithDeo() {
    try {
        // Get current block number
        const blockNumber = await web3.eth.getBlockNumber();
        console.log('Current block:', blockNumber);
        
        // Get account balance
        const balance = await web3.eth.getBalance('0x123...');
        console.log('Balance:', balance);
        
        // Call a contract function
        const result = await web3.eth.call({
            to: '0xcontract...',
            data: '0x...' // Function call data
        });
        console.log('Contract result:', result);
        
    } catch (error) {
        console.error('Error:', error);
    }
}

interactWithDeo();
```

#### cURL Examples

```bash
# Get current block number
curl -X POST -H "Content-Type: application/json" \
  --data '{"jsonrpc":"2.0","method":"eth_blockNumber","params":[],"id":1}' \
  http://localhost:8545

# Get account balance
curl -X POST -H "Content-Type: application/json" \
  --data '{"jsonrpc":"2.0","method":"eth_getBalance","params":["0x123...","latest"],"id":1}' \
  http://localhost:8545

# Call contract function
curl -X POST -H "Content-Type: application/json" \
  --data '{"jsonrpc":"2.0","method":"eth_call","params":[{"to":"0xcontract...","data":"0x..."},"latest"],"id":1}' \
  http://localhost:8545
```

## Testing and Benchmarking

### Performance Testing

```bash
# Run comprehensive tests
./build/bin/deo_tests

# Test specific components
./build/bin/deo_tests --gtest_filter="VMTest.*"
./build/bin/deo_tests --gtest_filter="*Blockchain*"

# Performance benchmarking
./build/bin/deo test-vm
./build/bin/deo test-determinism
```

### Network Testing

```bash
# Test network connectivity
./build/bin/deo test-networking

# Simulate multiple nodes
./examples/scripts/test_network.sh

# Benchmark network performance
./examples/scripts/benchmark.sh
```

### Contract Testing

```bash
# Test contract compilation
./build/bin/deo compile-contract --input examples/contracts/simple_storage.sol --output /tmp/test.bin

# Test contract deployment
./build/bin/deo deploy-contract --bytecode /tmp/test.bin --gas 100000

# Test contract interaction
./build/bin/deo call-contract --address 0x... --function get
```

## Research Applications

### Consensus Mechanism Comparison

```bash
# Test Proof-of-Work consensus
./build/bin/deo start-node --consensus pow --mining-difficulty 4

# Test Proof-of-Authority consensus
./build/bin/deo start-node --consensus poa --validators validator1,validator2

# Compare performance
./examples/scripts/benchmark.sh --consensus pow
./examples/scripts/benchmark.sh --consensus poa
```

### Network Resilience Testing

```bash
# Test under network partitions
./examples/scripts/test_network.sh --partition-test

# Test under high load
./examples/scripts/test_network.sh --stress-test

# Test adversarial scenarios
./examples/scripts/test_network.sh --adversarial-test
```

### Storage System Analysis

```bash
# Test storage performance
./build/bin/deo test-storage --blocks 10000

# Test compression efficiency
./build/bin/deo test-storage --compression-test

# Test cache effectiveness
./build/bin/deo test-storage --cache-test
```

## Development Workflow

### 1. Contract Development

```bash
# Create new contract
cp examples/contracts/simple_storage.sol my_contract.sol

# Edit contract
vim my_contract.sol

# Compile contract
./build/bin/deo compile-contract --input my_contract.sol --output my_contract.bin

# Validate contract
./build/bin/deo validate-source --input my_contract.sol
```

### 2. Testing and Debugging

```bash
# Deploy for testing
./build/bin/deo deploy-contract --bytecode my_contract.bin --gas 100000

# Debug contract execution
./build/bin/deo get-contract-debug-info --address 0x...

# Monitor contract events
./build/bin/deo monitor-events --address 0x...
```

### 3. Performance Analysis

```bash
# Profile contract execution
./build/bin/deo estimate-gas --contract 0x... --function myFunction --args "..."

# Benchmark network performance
./examples/scripts/benchmark.sh --duration 300

# Analyze storage usage
./build/bin/deo show-stats --detailed
```

## Troubleshooting

### Common Issues

1. **Compilation Errors**: Check contract syntax and dependencies
2. **Deployment Failures**: Verify gas limits and account balances
3. **Network Issues**: Check peer connectivity and firewall settings
4. **Performance Issues**: Monitor resource usage and optimize parameters

### Debug Commands

```bash
# Check node status
./build/bin/deo node-status

# View logs
./build/bin/deo show-logs

# Test connectivity
./build/bin/deo test-networking

# Validate blockchain state
./build/bin/deo validate-blockchain
```

### Getting Help

- Check the main README.md for comprehensive documentation
- Review the architecture documentation in docs/ARCHITECTURE.md
- Examine the research methodology in docs/RESEARCH_METHODOLOGY.md
- Run tests to verify system functionality
- Use the built-in help system: `./build/bin/deo --help`

These examples provide a comprehensive starting point for using the Deo blockchain system for research, development, and experimentation. The modular design allows for easy extension and customization based on specific research needs.
