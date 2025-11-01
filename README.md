# Deo Blockchain

A comprehensive research implementation of a modern blockchain system designed for academic study and experimental development. Deo implements a complete distributed ledger with smart contract capabilities, peer-to-peer networking, and Web3-compatible APIs.

## Features

- **Multiple Consensus Mechanisms**: Proof-of-Work, Proof-of-Stake, and Proof-of-Authority
- **Smart Contract Support**: EVM-compatible virtual machine with gas metering
- **Peer-to-Peer Networking**: Robust P2P protocol with gossip propagation (full integration pending)
- **Storage System**: JSON-based state storage with LevelDB support planned
- **Web3 Compatibility**: JSON-RPC API with HTTP server and Basic Authentication
- **Wallet Management**: Full wallet module with account creation, import/export, and transaction signing
- **Comprehensive Testing**: 43+ test cases with Google Test framework
- **Security Features**: Automated security auditing and threat detection
- **Performance Optimization**: Multi-threaded processing and intelligent caching

## Quick Start

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2019+)
- CMake 3.15 or higher
- LevelDB development libraries
- OpenSSL development libraries

### Build Instructions

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libleveldb-dev libssl-dev

# Clone and build
git clone https://github.com/palaseus/Deo.git
cd Deo
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Running Tests

```bash
# Run all tests
./bin/DeoBlockchain_tests

# Run specific test suites
./bin/DeoBlockchain_tests --gtest_filter="VMTest.*"
./bin/DeoBlockchain_tests --gtest_filter="BlockchainTest.*"
```

### Basic Usage

```bash
# Start a node
./bin/DeoBlockchain

# Check node status
./bin/DeoBlockchain --status

# View blockchain statistics
./bin/DeoBlockchain --stats
```

## Architecture

Deo implements a modular architecture with clear separation of concerns:

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Application   │    │   Smart Contract│    │   Network Layer │
│      Layer      │    │      Layer      │    │                 │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│  JSON-RPC API   │    │  Virtual Machine│    │  P2P Protocol   │
│  CLI Interface  │    │  Contract Mgr   │    │  Gossip Protocol│
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│   Core Layer    │    │   Consensus     │    │   Storage Layer │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│  Blockchain     │    │  PoW/PoA/PoS    │    │  LevelDB        │
│  Transactions   │    │  Synchronizer   │    │  State Trie     │
│  Merkle Trees   │    │  Validator      │    │  Block Storage  │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Core Components

### Virtual Machine
- Deterministic execution with gas metering
- 256-bit arithmetic support
- EVM-compatible opcodes
- Precompiled cryptographic functions

### Consensus Mechanisms
- **Proof-of-Work**: SHA-256 based mining with adjustable difficulty
- **Proof-of-Stake**: Validator management with stake delegation and slashing
- **Proof-of-Authority**: Pre-authorized validator set with round-robin production

### Networking
- TCP-based P2P communication (implemented)
- Gossip protocol for efficient propagation (implemented, full integration pending)
- Peer discovery and reputation systems (implemented)
- NAT traversal support (planned)

### Storage
- JSON-based state storage (operational)
- State trie structures for account management
- LevelDB migration planned for production use
- Block pruning and compression (planned)
- Snapshot and backup capabilities (planned)

## Documentation

Comprehensive documentation is available in the `docs/` directory. See the [Documentation Index](docs/README.md) for a complete overview.

### Quick Start Documentation
- **[Architecture Guide](docs/ARCHITECTURE.md)**: Complete system architecture and design
- **[CLI Reference](docs/CLI_REFERENCE.md)**: Command-line interface documentation
- **[Build System](docs/BUILD_SYSTEM.md)**: Development setup and configuration
- **[Testing Framework](docs/TESTING_FRAMEWORK.md)**: Testing methodology and results

### Advanced Features
- **[API Reference](docs/API_REFERENCE.md)**: Comprehensive API documentation
- **[Compiler Guide](docs/COMPILER_GUIDE.md)**: Smart contract compilation
- **[Performance Optimization](docs/PERFORMANCE_OPTIMIZATION.md)**: Performance tuning and monitoring
- **[Security Audit](docs/SECURITY_AUDIT.md)**: Security auditing and validation
- **[Advanced Consensus](docs/ADVANCED_CONSENSUS.md)**: Advanced consensus mechanisms
- **[Advanced Storage](docs/ADVANCED_STORAGE.md)**: Advanced storage systems

### Research Documentation
- **[Proof of Stake](docs/PROOF_OF_STAKE.md)**: PoS implementation details
- **[Research Methodology](docs/RESEARCH_METHODOLOGY.md)**: Research approach and methodology
- **[Deployment Guide](docs/DEPLOYMENT_GUIDE.md)**: Production deployment instructions

## Implementation Status

This implementation provides the following capabilities:

- **Deterministic Virtual Machine**: Fully deterministic execution with comprehensive gas metering ✅
- **Adversarial Network Resilience**: Peer reputation systems and misbehavior detection ✅
- **Storage System**: JSON-based state storage with LevelDB migration planned ⚠️
- **Web3 Compatibility**: JSON-RPC API with 30+ methods including Web3-compatible endpoints ✅
- **Wallet Management**: Complete wallet module with CLI and JSON-RPC integration ✅
- **Proof-of-Stake Implementation**: PoS consensus with validator management ✅
- **HTTP Server**: Full HTTP server implementation with Basic Authentication ✅
- **CLI Commands**: Essential commands including gas estimation and contract storage access ✅

### Currently Working Features

✅ **Fully Implemented and Tested**:
- Core blockchain operations (blocks, transactions, Merkle trees)
- Virtual Machine with EVM-compatible opcodes
- Multiple consensus mechanisms (PoW, PoS, PoA)
- JSON-RPC API with HTTP server and authentication
- Wallet module with account management
- Cryptographic operations (signing, hashing, key management)
- Transaction validation and fee calculation
- Smart contract deployment and execution

⚠️ **Implemented but Pending Full Integration**:
- P2P networking (TCP layer complete, full NodeRuntime integration pending)
- Network message propagation (gossip protocol ready, needs NodeRuntime wiring)
- Chain synchronization (structure in place, needs network integration)

📋 **Planned Enhancements**:
- LevelDB storage backend migration
- Performance benchmarking and optimization
- Expanded test coverage
- Additional developer tools (contract templates, formatting, linting)

## Performance Metrics

| Metric | Status | Notes |
|--------|--------|-------|
| Transaction Throughput | Not Benchmarked | Performance testing pending |
| Block Production Time | Not Benchmarked | PoW/PoS/PoA implementations exist |
| State Read Latency | Operational | JSON-based storage (LevelDB migration planned) |
| Network Sync Time | Not Benchmarked | P2P network integration in progress |
| Memory Usage | Operational | Varies by blockchain state size |
| Storage Efficiency | Operational | JSON-based storage with compression planned |

*Note: Performance benchmarks are not yet validated. The system is functional but metrics require formal testing.*

## Testing

The project includes comprehensive testing with 43+ test cases covering:

- **Unit Tests**: Core components (Transaction, Block, KeyPair, Wallet)
- **VM Tests**: Virtual machine execution and opcode validation
- **Integration Tests**: End-to-end system validation (ongoing)
- **Performance Tests**: Benchmarking and profiling (planned)
- **Security Tests**: Vulnerability assessment and threat detection
- **Network Tests**: P2P protocol and message propagation (planned)

*Note: Test suite is actively expanding. Current coverage includes core blockchain operations, cryptographic operations, and wallet functionality.*

## License

This project is released under the MIT License. See LICENSE file for details.

## Contributing

This is a research project. Contributions are welcome for:

- Bug fixes and performance improvements
- Additional consensus mechanism implementations
- Enhanced testing and benchmarking tools
- Documentation improvements
- Research paper collaborations

## Contact

For research collaborations, technical questions, or academic inquiries, please contact the research team through the project repository.

---

**Note**: This implementation is intended for research and educational purposes. While the codebase includes comprehensive testing and security measures, it should not be used in production environments without additional security audits and performance optimizations.