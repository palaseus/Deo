# Deo Blockchain

A comprehensive research implementation of a modern blockchain system designed for academic study and experimental development. Deo implements a complete distributed ledger with smart contract capabilities, peer-to-peer networking, and Web3-compatible APIs.

## Features

- **Multiple Consensus Mechanisms**: Proof-of-Work, Proof-of-Stake, and Proof-of-Authority
- **Smart Contract Support**: EVM-compatible virtual machine with gas metering
- **Peer-to-Peer Networking**: Robust P2P protocol with gossip propagation (fully integrated)
- **Storage System**: LevelDB backend (default, operational) with JSON fallback
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
- **Proof-of-Work** ✅: SHA-256 based mining with adjustable difficulty (fully implemented)
- **Proof-of-Stake** ✅: Validator management with stake delegation and slashing (fully implemented)
- **Proof-of-Authority** ✅: Pre-authorized validator set with round-robin production (fully implemented)

**Note**: Advanced consensus mechanisms (DPoS, PBFT, Hybrid Consensus) are planned but not yet implemented. See `docs/ADVANCED_CONSENSUS.md` for details.

### Networking
- TCP-based P2P communication (implemented)
- Gossip protocol for efficient propagation (fully integrated)
- Peer discovery and reputation systems (implemented)
- Network synchronization (FastSync available with LevelDB + P2P)
- NAT traversal support (planned)

### Storage
- LevelDB backend (default and operational) for blocks and state
- JSON-based fallback storage (operational)
- State trie structures for account management
- Block pruning with deletion support (implemented)
- State snapshots and restore capabilities (implemented)
- Compression enabled via LevelDB Snappy compression

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
- **Storage System**: LevelDB backend (default) with JSON fallback ✅
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

✅ **Fully Integrated**:
- P2P networking (fully integrated with NodeRuntime)
- Network message propagation (gossip protocol operational and integrated)
- Chain synchronization (FastSync available with LevelDB + P2P)
- Block pruning with actual deletion operations
- State snapshot creation and restoration

📋 **Recent Enhancements**:
- ✅ LevelDB storage backend (now default)
- ✅ Performance benchmarking suite
- ✅ P2P network integration
- ✅ State backup/restore functionality
- Additional developer tools (contract templates, formatting, linting)

## Performance Metrics

| Metric | Status | Notes |
|--------|--------|-------|
| Transaction Throughput | Benchmarked | See `tests/performance/` for benchmark tests |
| Block Production Time | Benchmarked | Measured with PoW consensus (configurable difficulty) |
| State Read Latency | Operational | LevelDB backend available (default), JSON fallback |
| Network Sync Time | Operational | FastSync available with LevelDB + P2P networking |
| Memory Usage | Operational | Varies by blockchain state size |
| Storage Efficiency | Operational | LevelDB default backend provides optimized storage |

*Note: Performance benchmarks are available in the test suite. Run `./bin/DeoBlockchain_tests --gtest_filter="*Benchmark*"` to execute benchmarks. See `docs/PERFORMANCE_OPTIMIZATION.md` for detailed performance guidance.*

## Testing

The project includes comprehensive testing with 50+ test cases covering:

- **Unit Tests**: Core components (Transaction, Block, KeyPair, Wallet)
- **VM Tests**: Virtual machine execution and opcode validation
- **Integration Tests**: End-to-end system validation including multi-node scenarios
- **Performance Tests**: Benchmarking suite with JSON/LevelDB backend comparison
- **Security Tests**: Vulnerability assessment and threat detection
- **Network Tests**: P2P protocol and message propagation tests
- **Multi-Node Tests**: End-to-end multi-node P2P networking validation
- **Workflow Tests**: Complete node lifecycle and operation workflows

### Running Tests

```bash
# Run all tests
./build/bin/DeoBlockchain_tests

# Run integration tests
./build/bin/DeoBlockchain_tests --gtest_filter="*Integration*"

# Run multi-node tests
./build/bin/DeoBlockchain_tests --gtest_filter="*MultiNode*"

# Run workflow tests
./build/bin/DeoBlockchain_tests --gtest_filter="*Workflow*"

# Run performance benchmarks
./build/bin/DeoBlockchain_tests --gtest_filter="*Benchmark*"
```

*Test suite continues to expand with comprehensive coverage of all system components.*

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