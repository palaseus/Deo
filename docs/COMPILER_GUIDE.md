# Smart Contract Compiler Guide

## Overview

The Deo Blockchain includes a comprehensive smart contract compilation system that supports Solidity-like syntax and generates EVM-compatible bytecode. The compiler provides lexical analysis, parsing, optimization, and bytecode generation capabilities.

## Compiler Architecture

### Core Components

1. **Lexical Analyzer**: Tokenizes source code into meaningful units
2. **Parser**: Builds Abstract Syntax Tree (AST) from tokens
3. **Semantic Analyzer**: Performs type checking and validation
4. **Code Generator**: Produces EVM-compatible bytecode
5. **Optimizer**: Optimizes bytecode for gas efficiency

### Compilation Pipeline

```
Source Code → Lexical Analysis → Parsing → AST → Semantic Analysis → Code Generation → Optimization → Bytecode
```

## Supported Language Features

### Basic Types
- `uint256`: 256-bit unsigned integer
- `int256`: 256-bit signed integer
- `bool`: Boolean type
- `address`: 160-bit address type
- `bytes32`: 32-byte array
- `string`: Dynamic string type

### Control Structures
- `if/else` statements
- `for` loops
- `while` loops
- `do/while` loops
- `switch/case` statements

### Functions
- Function declarations
- Parameter passing
- Return values
- Function overloading
- Modifiers

### Contract Features
- Contract declarations
- Inheritance
- Interfaces
- Libraries
- Events
- Modifiers

## Compiler Usage

### Command Line Interface

#### Basic Compilation
```bash
./bin/DeoBlockchain compile-contract --input contract.sol --output contract.bin
```

#### Advanced Options
```bash
./bin/DeoBlockchain compile-contract \
  --input contract.sol \
  --output contract.bin \
  --optimize \
  --abi contract.abi \
  --gas-estimate
```

#### Available Options
- `--input <file>`: Source file path
- `--output <file>`: Output bytecode file
- `--abi <file>`: Generate ABI file
- `--optimize`: Enable bytecode optimization
- `--gas-estimate`: Estimate gas requirements
- `--verbose`: Verbose compilation output
- `--warnings`: Show compilation warnings

### Programmatic Usage

```cpp
#include "compiler/contract_compiler.h"

// Create compiler instance
deo::compiler::ContractCompiler compiler;

// Compile source code
std::string source_code = "contract MyContract { ... }";
auto result = compiler.compile(source_code);

if (result.success) {
    std::cout << "Bytecode: " << result.bytecode << std::endl;
    std::cout << "ABI: " << result.abi << std::endl;
} else {
    std::cerr << "Compilation error: " << result.error_message << std::endl;
}
```

## Language Syntax

### Contract Declaration
```solidity
contract MyContract {
    // Contract body
}
```

### State Variables
```solidity
contract MyContract {
    uint256 public balance;
    address private owner;
    bool internal isActive;
}
```

### Functions
```solidity
contract MyContract {
    function transfer(address to, uint256 amount) public returns (bool) {
        // Function implementation
        return true;
    }
    
    function getBalance() public view returns (uint256) {
        return balance;
    }
}
```

### Events
```solidity
contract MyContract {
    event Transfer(address indexed from, address indexed to, uint256 value);
    
    function transfer(address to, uint256 amount) public {
        // Implementation
        emit Transfer(msg.sender, to, amount);
    }
}
```

### Modifiers
```solidity
contract MyContract {
    modifier onlyOwner() {
        require(msg.sender == owner, "Not the owner");
        _;
    }
    
    function restrictedFunction() public onlyOwner {
        // Only owner can call this
    }
}
```

## Compilation Process

### 1. Lexical Analysis

The lexical analyzer tokenizes the source code:

```cpp
// Example tokens
Token(TokenType::KEYWORD, "contract")
Token(TokenType::IDENTIFIER, "MyContract")
Token(TokenType::LEFT_BRACE, "{")
Token(TokenType::KEYWORD, "function")
Token(TokenType::IDENTIFIER, "transfer")
```

### 2. Parsing

The parser builds an Abstract Syntax Tree:

```cpp
struct ASTNode {
    TokenType type;
    std::string value;
    std::vector<std::shared_ptr<ASTNode>> children;
};
```

### 3. Semantic Analysis

Performs type checking and validation:
- Variable declaration checking
- Type compatibility validation
- Function signature verification
- Scope resolution

### 4. Code Generation

Generates EVM-compatible bytecode:

```cpp
// Example bytecode generation
std::vector<uint8_t> bytecode;
bytecode.push_back(0x60); // PUSH1
bytecode.push_back(0x00); // 0x00
bytecode.push_back(0x60); // PUSH1
bytecode.push_back(0x00); // 0x00
bytecode.push_back(0x52); // MSTORE
```

## Optimization

### Bytecode Optimization

The compiler includes several optimization passes:

1. **Constant Folding**: Evaluates constant expressions at compile time
2. **Dead Code Elimination**: Removes unreachable code
3. **Jump Optimization**: Optimizes jump instructions
4. **Stack Optimization**: Minimizes stack operations

### Gas Optimization

- **Function Inlining**: Inlines small functions to reduce call overhead
- **Loop Unrolling**: Unrolls small loops for better performance
- **Storage Optimization**: Optimizes storage access patterns

## Error Handling

### Compilation Errors

The compiler provides detailed error reporting:

```cpp
struct CompilationError {
    std::string message;
    int line_number;
    int column_number;
    std::string source_line;
};
```

### Common Error Types

1. **Syntax Errors**: Invalid syntax
2. **Type Errors**: Type mismatches
3. **Semantic Errors**: Logic errors
4. **Gas Errors**: Gas limit exceeded

### Error Examples

```
Error: Expected ';' at line 10, column 5
  function transfer(address to, uint256 amount) public
                                              ^
```

## ABI Generation

### Application Binary Interface

The compiler generates ABI files for contract interaction:

```json
{
  "contractName": "MyContract",
  "abi": [
    {
      "name": "transfer",
      "type": "function",
      "inputs": [
        {"name": "to", "type": "address"},
        {"name": "amount", "type": "uint256"}
      ],
      "outputs": [
        {"name": "", "type": "bool"}
      ],
      "stateMutability": "nonpayable"
    }
  ]
}
```

## Gas Estimation

### Gas Calculation

The compiler estimates gas requirements for:
- Function calls
- Storage operations
- Computational complexity
- Memory usage

### Gas Optimization Tips

1. **Use appropriate data types**: Choose the smallest type that fits
2. **Optimize loops**: Minimize loop iterations
3. **Cache storage reads**: Avoid repeated storage access
4. **Use events**: Events are cheaper than storage

## Advanced Features

### Library Support

```solidity
library MathLib {
    function add(uint256 a, uint256 b) internal pure returns (uint256) {
        return a + b;
    }
}

contract MyContract {
    using MathLib for uint256;
    
    function calculate(uint256 a, uint256 b) public pure returns (uint256) {
        return a.add(b);
    }
}
```

### Interface Support

```solidity
interface IERC20 {
    function transfer(address to, uint256 amount) external returns (bool);
    function balanceOf(address account) external view returns (uint256);
}

contract MyContract is IERC20 {
    // Implementation
}
```

### Inheritance

```solidity
contract BaseContract {
    uint256 public baseValue;
    
    function baseFunction() public virtual {
        // Base implementation
    }
}

contract DerivedContract is BaseContract {
    function baseFunction() public override {
        // Override implementation
    }
}
```

## Debugging

### Debug Information

The compiler can generate debug information:

```bash
./bin/DeoBlockchain compile-contract --input contract.sol --output contract.bin --debug
```

### Debug Features

- Source mapping
- Variable tracking
- Step-by-step execution
- Gas usage analysis

## Best Practices

### Code Organization

1. **Modular Design**: Break contracts into smaller modules
2. **Clear Naming**: Use descriptive names for variables and functions
3. **Documentation**: Add comments for complex logic
4. **Error Handling**: Implement proper error handling

### Security Considerations

1. **Input Validation**: Validate all inputs
2. **Access Control**: Implement proper access controls
3. **Reentrancy Protection**: Use reentrancy guards
4. **Integer Overflow**: Handle integer overflow cases

### Performance Optimization

1. **Gas Efficiency**: Optimize for gas usage
2. **Storage Optimization**: Minimize storage operations
3. **Function Optimization**: Optimize function calls
4. **Loop Optimization**: Optimize loop structures

## Examples

### Simple Token Contract

```solidity
contract SimpleToken {
    mapping(address => uint256) public balances;
    address public owner;
    
    constructor() {
        owner = msg.sender;
    }
    
    function mint(address to, uint256 amount) public {
        require(msg.sender == owner, "Not authorized");
        balances[to] += amount;
    }
    
    function transfer(address to, uint256 amount) public returns (bool) {
        require(balances[msg.sender] >= amount, "Insufficient balance");
        balances[msg.sender] -= amount;
        balances[to] += amount;
        return true;
    }
}
```

### Compilation Example

```bash
# Compile the contract
./bin/DeoBlockchain compile-contract --input SimpleToken.sol --output SimpleToken.bin --abi SimpleToken.abi --optimize

# Deploy the contract
./bin/DeoBlockchain deploy-contract --bytecode SimpleToken.bin --gas 1000000
```

This compiler provides a complete toolchain for smart contract development, enabling developers to create, compile, and deploy contracts on the Deo blockchain.
