cmake_minimum_required(VERSION 3.20)
project(DeoBlockchain VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)  # -Werror temporarily disabled
    # Suppress OpenSSL 3.0 deprecation warnings
    add_compile_options(-Wno-deprecated-declarations)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(include)

# Find required packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# LevelDB - use system package
find_library(LEVELDB_LIBRARY leveldb REQUIRED)
find_path(LEVELDB_INCLUDE_DIR leveldb/db.h REQUIRED)

# nlohmann/json
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Source files
set(CORE_SOURCES
    src/core/blockchain.cpp
    src/core/block.cpp
    src/core/transaction.cpp
    src/core/transaction_receipt.cpp
    src/core/merkle_tree.cpp
    src/core/state_trie.cpp
    src/core/transaction_fees.cpp
    src/storage/leveldb_block_storage.cpp
    src/storage/leveldb_state_storage.cpp
    src/storage/block_pruning_manager.cpp
    src/storage/state_snapshot_manager.cpp
    src/sync/fast_sync_manager.cpp
    src/testing/scale_testing_framework.cpp
    src/testing/adversarial_testing_framework.cpp
    src/compiler/contract_compiler.cpp
    src/cli/contract_cli.cpp
    src/consensus/consensus_engine.cpp
    src/consensus/proof_of_work.cpp
    src/consensus/proof_of_authority.cpp
    src/consensus/consensus_synchronizer.cpp
    src/network/peer_manager.cpp
    src/network/p2p_network_manager.cpp
    src/network/tcp_network.cpp
    src/network/gossip_protocol.cpp
    src/network/transaction_mempool.cpp
    src/network/peer_connection_manager.cpp
    src/network/peer_authentication.cpp
    src/network/network_messages.cpp
    src/crypto/hash.cpp
    src/crypto/signature.cpp
    src/crypto/key_pair.cpp
    src/storage/block_storage.cpp
    src/storage/state_storage.cpp
    src/vm/uint256.cpp
    src/vm/virtual_machine.cpp
    src/vm/instruction_handlers.cpp
    src/vm/smart_contract.cpp
    src/vm/smart_contract_manager.cpp
    src/vm/state_store.cpp
    src/vm/vm_block_validator.cpp
    src/vm/determinism_tester.cpp
    src/node/node_runtime.cpp
    src/api/json_rpc_server.cpp
    src/cli/command_parser.cpp
    src/cli/commands.cpp
    src/utils/logger.cpp
    src/utils/config.cpp
    src/utils/error_handler.cpp
)

# Header files
set(CORE_HEADERS
    include/core/blockchain.h
    include/core/block.h
    include/core/transaction.h
    include/core/transaction_receipt.h
    include/core/merkle_tree.h
    include/core/state_trie.h
    include/storage/leveldb_storage.h
    include/storage/block_pruning.h
    include/sync/fast_sync.h
    include/testing/scale_testing.h
    include/testing/adversarial_testing.h
    include/compiler/contract_compiler.h
    include/cli/contract_cli.h
    include/consensus/consensus_engine.h
    include/consensus/proof_of_work.h
    include/consensus/proof_of_authority.h
    include/consensus/consensus_synchronizer.h
    include/network/network_messages.h
    include/network/peer_manager.h
    include/network/p2p_network_manager.h
    include/network/tcp_network.h
    include/network/transaction_mempool.h
    include/network/peer_connection_manager.h
    include/network/peer_authentication.h
    include/core/transaction_fees.h
    include/crypto/hash.h
    include/crypto/signature.h
    include/crypto/key_pair.h
    include/storage/block_storage.h
    include/storage/state_storage.h
    include/vm/uint256.h
    include/vm/virtual_machine.h
    include/vm/smart_contract.h
    include/vm/smart_contract_manager.h
    include/vm/state_store.h
    include/vm/vm_block_validator.h
    include/vm/determinism_tester.h
    include/node/node_runtime.h
    include/api/json_rpc_server.h
    include/cli/command_parser.h
    include/cli/commands.h
    include/utils/logger.h
    include/utils/config.h
    include/utils/error_handler.h
)

# Create core library
add_library(deo_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_link_libraries(deo_core 
    Threads::Threads 
    OpenSSL::SSL 
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ${LEVELDB_LIBRARY}
)
target_include_directories(deo_core PRIVATE ${LEVELDB_INCLUDE_DIR})

# Main executable
add_executable(deo main.cpp)
target_link_libraries(deo deo_core)

# Test sources
set(TEST_SOURCES
    tests/core/test_blockchain.cpp
    tests/core/test_block.cpp
    tests/core/test_transaction.cpp
    tests/core/test_transaction_fees.cpp
    tests/consensus/test_consensus.cpp
    tests/network/test_peer_connection_manager.cpp
    tests/network/test_peer_reputation.cpp
    tests/network/test_peer_reputation_simple.cpp
    tests/network/test_peer_authentication.cpp
    tests/crypto/test_crypto.cpp
    tests/storage/test_storage.cpp
    tests/storage/test_leveldb_storage.cpp
    tests/storage/test_block_pruning.cpp
    tests/sync/test_fast_sync.cpp
    tests/testing/test_scale_testing.cpp
    tests/testing/test_adversarial_testing.cpp
    tests/compiler/test_contract_compiler.cpp
    tests/vm/test_vm.cpp
    tests/cli/test_cli.cpp
    tests/utils/test_utils.cpp
)

# Create test executable
add_executable(deo_tests ${TEST_SOURCES})
target_link_libraries(deo_tests 
    deo_core 
    gtest_main 
    gmock_main
)

# Add tests
add_test(NAME DeoTests COMMAND deo_tests)

# Coverage (if enabled)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(deo_core PRIVATE --coverage)
    target_compile_options(deo_tests PRIVATE --coverage)
    target_link_libraries(deo_core --coverage)
    target_link_libraries(deo_tests --coverage)
endif()

# Install targets
install(TARGETS deo deo_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)
