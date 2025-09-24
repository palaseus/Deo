# Build System Documentation

## Overview

The Deo blockchain project uses CMake as its primary build system, providing cross-platform support for Linux, macOS, and Windows. This document outlines the build configuration, dependencies, and development setup.

## Build Configuration

### CMake Configuration

The project uses CMake 3.15+ with the following configuration:

```cmake
cmake_minimum_required(VERSION 3.15)
project(DeoBlockchain VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion")

# Enable sanitizers for debugging
option(ENABLE_SANITIZERS "Enable sanitizers" ON)
if(ENABLE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()
```

### Build Targets

The build system generates the following targets:

1. **DeoBlockchain**: Main executable
2. **DeoBlockchain_tests**: Test suite executable
3. **DeoBlockchain_lib**: Static library (optional)

### Dependencies

#### Required Dependencies

- **C++17 Compiler**: GCC 7+, Clang 5+, MSVC 2019+
- **CMake**: 3.15 or higher
- **LevelDB**: 1.20+ for persistent storage
- **OpenSSL**: 1.1.1+ for cryptographic operations
- **nlohmann/json**: 3.7+ for JSON serialization

#### Optional Dependencies

- **Google Test**: For unit testing
- **Google Mock**: For mocking in tests
- **Valgrind**: For memory debugging (Linux)
- **AddressSanitizer**: For memory safety (GCC/Clang)

### Dependency Installation

#### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install build-essential cmake libleveldb-dev libssl-dev
sudo apt-get install libgtest-dev libgmock-dev  # For testing
sudo apt-get install valgrind  # For memory debugging
```

#### macOS

```bash
brew install cmake leveldb openssl
brew install googletest  # For testing
```

#### Windows (vcpkg)

```bash
vcpkg install leveldb openssl nlohmann-json
vcpkg install gtest gmock  # For testing
```

## Build Instructions

### Standard Build

```bash
# Clone repository
git clone <repository-url>
cd Deo

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)  # Linux/macOS
# or
cmake --build . --config Release  # Windows
```

### Debug Build

```bash
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Release Build

```bash
mkdir build-release && cd build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Sanitizer Build

```bash
mkdir build-sanitizer && cd build-sanitizer
cmake -DENABLE_SANITIZERS=ON ..
make -j$(nproc)
```

## Build Options

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_SANITIZERS` | ON | Enable AddressSanitizer, UBSan, TSan |
| `ENABLE_TESTING` | ON | Build test suite |
| `ENABLE_COVERAGE` | OFF | Enable code coverage |
| `ENABLE_DOCS` | OFF | Generate documentation |
| `BUILD_SHARED_LIBS` | OFF | Build shared libraries |

### Compiler Options

| Option | Description |
|--------|-------------|
| `-Wall -Wextra` | Enable all warnings |
| `-Wpedantic` | Strict ISO C++ compliance |
| `-Wshadow` | Warn about shadowed variables |
| `-Wconversion` | Warn about implicit conversions |
| `-fsanitize=address` | AddressSanitizer for memory errors |
| `-fsanitize=undefined` | UndefinedBehaviorSanitizer |
| `-fsanitize=thread` | ThreadSanitizer for race conditions |

## Project Structure

### Source Organization

```
src/
├── api/           # JSON-RPC and Web3 API
├── cli/           # Command-line interface
├── compiler/      # Smart contract compilation
├── consensus/     # Consensus mechanisms
├── core/          # Core blockchain components
├── crypto/        # Cryptographic primitives
├── network/       # P2P networking
├── node/          # Node runtime
├── storage/       # Persistent storage
├── sync/          # Blockchain synchronization
├── testing/       # Testing frameworks
├── utils/         # Utilities and logging
└── vm/            # Virtual machine

include/
├── api/           # API headers
├── cli/           # CLI headers
├── compiler/      # Compiler headers
├── consensus/     # Consensus headers
├── core/          # Core headers
├── crypto/        # Crypto headers
├── network/       # Network headers
├── node/          # Node headers
├── storage/       # Storage headers
├── sync/          # Sync headers
├── testing/       # Testing headers
├── utils/         # Utility headers
└── vm/            # VM headers

tests/
├── consensus/     # Consensus tests
├── core/          # Core tests
├── network/       # Network tests
├── testing/       # Testing framework tests
└── vm/            # VM tests
```

### CMakeLists.txt Structure

```cmake
# Main CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(DeoBlockchain)

# Include subdirectories
add_subdirectory(src)
add_subdirectory(tests)

# Create executables
add_executable(${PROJECT_NAME} ${SOURCES})
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})
target_link_libraries(${PROJECT_NAME}_tests ${TEST_LIBRARIES})
```

## Development Setup

### IDE Configuration

#### Visual Studio Code

```json
{
    "cmake.configureArgs": [
        "-DENABLE_SANITIZERS=ON",
        "-DENABLE_TESTING=ON"
    ],
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.generator": "Unix Makefiles"
}
```

#### CLion

1. Open project in CLion
2. Configure CMake settings:
   - Build type: Debug
   - CMake options: `-DENABLE_SANITIZERS=ON`
3. Set build directory to `build/`

### Debugging Setup

#### GDB Configuration

```bash
# Enable debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Run with GDB
gdb ./build/bin/DeoBlockchain_tests
(gdb) run --gtest_filter="VMTest.*"
```

#### Valgrind Integration

```bash
# Memory leak detection
valgrind --tool=memcheck --leak-check=full ./build/bin/DeoBlockchain_tests

# Call graph analysis
valgrind --tool=callgrind ./build/bin/DeoBlockchain_tests
```

### Continuous Integration

#### GitHub Actions

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install build-essential cmake libleveldb-dev libssl-dev
    
    - name: Configure
      run: |
        mkdir build && cd build
        cmake -DENABLE_SANITIZERS=ON ..
    
    - name: Build
      run: |
        cd build
        make -j$(nproc)
    
    - name: Test
      run: |
        cd build
        ./bin/DeoBlockchain_tests
```

#### Jenkins Pipeline

```groovy
pipeline {
    agent any
    
    stages {
        stage('Build') {
            steps {
                sh 'mkdir build && cd build && cmake .. && make -j$(nproc)'
            }
        }
        
        stage('Test') {
            steps {
                sh 'cd build && ./bin/DeoBlockchain_tests'
            }
        }
        
        stage('Coverage') {
            steps {
                sh 'cd build && gcov -r src/*.cpp'
            }
        }
    }
}
```

## Build Optimization

### Compiler Optimizations

#### Release Build

```cmake
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_BUILD_TYPE Release)
```

#### Debug Build

```cmake
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")
set(CMAKE_BUILD_TYPE Debug)
```

### Link-Time Optimization

```cmake
# Enable LTO
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
```

### Parallel Builds

```bash
# Use all available cores
make -j$(nproc)

# Or specify number of jobs
make -j8
```

## Cross-Platform Support

### Linux

- **GCC**: 7.0+ with C++17 support
- **Clang**: 5.0+ with C++17 support
- **Distribution**: Ubuntu 18.04+, CentOS 7+, Debian 9+

### macOS

- **Xcode**: 10.0+ with C++17 support
- **Homebrew**: For dependency management
- **macOS**: 10.14+ (Mojave or later)

### Windows

- **Visual Studio**: 2019+ with C++17 support
- **vcpkg**: For dependency management
- **Windows**: 10+ (64-bit)

## Troubleshooting

### Common Build Issues

#### Missing Dependencies

```bash
# Error: Could not find LevelDB
sudo apt-get install libleveldb-dev

# Error: Could not find OpenSSL
sudo apt-get install libssl-dev
```

#### Compiler Version Issues

```bash
# Error: C++17 not supported
# Update compiler or use different version
export CC=gcc-9
export CXX=g++-9
```

#### Memory Issues

```bash
# Error: Out of memory during build
# Reduce parallel jobs
make -j2  # Instead of make -j$(nproc)
```

### Debug Build Issues

#### Sanitizer Conflicts

```bash
# Error: AddressSanitizer and ThreadSanitizer conflict
# Disable one sanitizer
cmake -DENABLE_SANITIZERS=OFF ..
```

#### Test Failures

```bash
# Run specific tests
./build/bin/DeoBlockchain_tests --gtest_filter="VMTest.*"

# Run with verbose output
./build/bin/DeoBlockchain_tests --gtest_output=xml:test_results.xml
```

## Performance Tuning

### Build Performance

```bash
# Use ccache for faster builds
sudo apt-get install ccache
export CC="ccache gcc"
export CXX="ccache g++"

# Use ninja instead of make
sudo apt-get install ninja-build
cmake -GNinja ..
ninja
```

### Runtime Performance

```bash
# Profile with perf (Linux)
perf record ./build/bin/DeoBlockchain_tests
perf report

# Profile with Instruments (macOS)
instruments -t "Time Profiler" ./build/bin/DeoBlockchain_tests
```

## Conclusion

The Deo build system provides comprehensive cross-platform support with extensive testing, debugging, and optimization capabilities. The system is designed for both development and research applications with robust CI/CD integration.

For build issues or development questions, please refer to the troubleshooting section or contact the development team.

