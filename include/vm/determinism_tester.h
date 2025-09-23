/**
 * @file determinism_tester.h
 * @brief Determinism testing framework for VM execution
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>

#include "vm/virtual_machine.h"
#include "vm/smart_contract_manager.h"
#include "vm/state_store.h"

namespace deo {
namespace vm {

/**
 * @brief Determinism test result
 */
struct DeterminismTestResult {
    bool all_identical;                    ///< Whether all executions produced identical results
    std::string error_message;             ///< Error message if test failed
    std::vector<ExecutionResult> results;  ///< Results from each execution
    std::map<std::string, std::string> state_hashes; ///< State hashes after each execution
    
    /**
     * @brief Constructor
     */
    DeterminismTestResult() : all_identical(false) {}
};

/**
 * @brief Determinism testing framework
 * 
 * This class provides utilities to test that VM execution is deterministic
 * across multiple instances and executions. It ensures that the same input
 * always produces the same output, which is critical for blockchain consensus.
 */
class DeterminismTester {
public:
    /**
     * @brief Constructor
     */
    DeterminismTester();
    
    /**
     * @brief Destructor
     */
    ~DeterminismTester();
    
    /**
     * @brief Test contract deployment determinism
     * @param bytecode Contract bytecode to deploy
     * @param num_instances Number of VM instances to test
     * @return Test result
     */
    DeterminismTestResult testContractDeploymentDeterminism(
        const std::vector<uint8_t>& bytecode,
        size_t num_instances = 3
    );
    
    /**
     * @brief Test contract call determinism
     * @param contract_address Contract address to call
     * @param call_data Call data
     * @param num_instances Number of VM instances to test
     * @return Test result
     */
    DeterminismTestResult testContractCallDeterminism(
        const std::string& contract_address,
        const std::vector<uint8_t>& call_data,
        size_t num_instances = 3
    );
    
    /**
     * @brief Test VM instruction determinism
     * @param bytecode Bytecode to execute
     * @param context Execution context
     * @param num_instances Number of VM instances to test
     * @return Test result
     */
    DeterminismTestResult testVMInstructionDeterminism(
        const std::vector<uint8_t>& bytecode,
        const ExecutionContext& context,
        size_t num_instances = 3
    );
    
    /**
     * @brief Test state transition determinism
     * @param transactions List of transactions to execute
     * @param num_instances Number of VM instances to test
     * @return Test result
     */
    DeterminismTestResult testStateTransitionDeterminism(
        const std::vector<std::vector<uint8_t>>& transactions,
        size_t num_instances = 3
    );
    
    /**
     * @brief Run comprehensive determinism test suite
     * @return True if all tests passed
     */
    bool runDeterminismTestSuite();

private:
    /**
     * @brief Create a fresh VM instance
     * @return New VM instance
     */
    std::unique_ptr<VirtualMachine> createVMInstance();
    
    /**
     * @brief Create a fresh state store
     * @param instance_id Instance identifier
     * @return New state store
     */
    std::shared_ptr<StateStore> createStateStore(const std::string& instance_id);
    
    /**
     * @brief Create a fresh contract manager
     * @param state_store State store to use
     * @return New contract manager
     */
    std::unique_ptr<SmartContractManager> createContractManager(
        std::shared_ptr<StateStore> state_store
    );
    
    /**
     * @brief Calculate state hash
     * @param state_store State store to hash
     * @return State hash
     */
    std::string calculateStateHash(std::shared_ptr<StateStore> state_store);
    
    /**
     * @brief Compare execution results
     * @param results List of execution results
     * @return True if all results are identical
     */
    bool compareExecutionResults(const std::vector<ExecutionResult>& results);
    
    /**
     * @brief Compare state hashes
     * @param state_hashes Map of state hashes
     * @return True if all state hashes are identical
     */
    bool compareStateHashes(const std::map<std::string, std::string>& state_hashes);
    
    /**
     * @brief Generate test bytecode
     * @return Test bytecode
     */
    std::vector<uint8_t> generateTestBytecode();
    
    /**
     * @brief Generate test execution context
     * @return Test execution context
     */
    ExecutionContext generateTestExecutionContext();
};

} // namespace vm
} // namespace deo
