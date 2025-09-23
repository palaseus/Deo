# Test CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Find GoogleTest
find_package(GTest REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Test sources
set(TEST_SOURCES
    core/test_blockchain.cpp
    core/test_block.cpp
    core/test_transaction.cpp
    consensus/test_consensus.cpp
    networking/test_peer_manager.cpp
    crypto/test_crypto.cpp
    storage/test_storage.cpp
    vm/test_vm.cpp
    cli/test_cli.cpp
    utils/test_utils.cpp
)

# Create test executable
add_executable(blockchain_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(blockchain_tests
    GTest::GTest
    GTest::Main
    blockchain_core
    blockchain_consensus
    blockchain_networking
    blockchain_crypto
    blockchain_storage
    blockchain_vm
    blockchain_cli
    blockchain_utils
)

# Add test to CTest
enable_testing()
add_test(NAME blockchain_tests COMMAND blockchain_tests)
