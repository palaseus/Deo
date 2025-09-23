/**
 * @file virtual_machine.h
 * @brief Virtual Machine for smart contract execution
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#ifndef DEO_VM_VIRTUAL_MACHINE_H
#define DEO_VM_VIRTUAL_MACHINE_H

#include <vector>
#include <string>
#include <map>
#include <stack>
#include <memory>
#include <cstdint>
#include <functional>
#include "vm/uint256.h"

namespace deo {
namespace vm {

/**
 * @brief VM instruction opcodes
 */
enum class Opcode : uint8_t {
    // Stack operations
    PUSH0 = 0x5F,
    PUSH1 = 0x60,
    PUSH2 = 0x61,
    PUSH3 = 0x62,
    PUSH4 = 0x63,
    PUSH5 = 0x64,
    PUSH6 = 0x65,
    PUSH7 = 0x66,
    PUSH8 = 0x67,
    PUSH32 = 0x7F,
    POP = 0x50,
    DUP1 = 0x80,
    DUP2 = 0x81,
    DUP3 = 0x82,
    DUP4 = 0x83,
    DUP5 = 0x84,
    DUP6 = 0x85,
    DUP7 = 0x86,
    DUP8 = 0x87,
    SWAP1 = 0x90,
    SWAP2 = 0x91,
    SWAP3 = 0x92,
    SWAP4 = 0x93,
    SWAP5 = 0x94,
    SWAP6 = 0x95,
    SWAP7 = 0x96,
    SWAP8 = 0x97,
    
    // Arithmetic operations
    ADD = 0x01,
    MUL = 0x02,
    SUB = 0x03,
    DIV = 0x04,
    MOD = 0x06,
    ADDMOD = 0x08,
    MULMOD = 0x09,
    EXP = 0x0A,
    SIGNEXTEND = 0x0B,
    
    // Comparison operations
    LT = 0x10,
    GT = 0x11,
    SLT = 0x12,
    SGT = 0x13,
    EQ = 0x14,
    ISZERO = 0x15,
    AND = 0x16,
    OR = 0x17,
    XOR = 0x18,
    NOT = 0x19,
    BYTE = 0x1A,
    SHL = 0x1B,
    SHR = 0x1C,
    SAR = 0x1D,
    
    // Control flow
    JUMP = 0x56,
    JUMPI = 0x57,
    PC = 0x58,
    MSIZE = 0x59,
    GAS = 0x5A,
    JUMPDEST = 0x5B,
    
    // Memory operations
    MLOAD = 0x51,
    MSTORE = 0x52,
    MSTORE8 = 0x53,
    SLOAD = 0x54,
    SSTORE = 0x55,
    
    // Cryptographic operations
    SHA3 = 0x20,
    
    // System operations
    ADDRESS = 0x30,
    BALANCE = 0x31,
    ORIGIN = 0x32,
    CALLER = 0x33,
    CALLVALUE = 0x34,
    CALLDATALOAD = 0x35,
    CALLDATASIZE = 0x36,
    CALLDATACOPY = 0x37,
    CODESIZE = 0x38,
    CODECOPY = 0x39,
    GASPRICE = 0x3A,
    EXTCODESIZE = 0x3B,
    EXTCODECOPY = 0x3C,
    RETURNDATASIZE = 0x3D,
    RETURNDATACOPY = 0x3E,
    EXTCODEHASH = 0x3F,
    
    // Block information
    BLOCKHASH = 0x40,
    COINBASE = 0x41,
    TIMESTAMP = 0x42,
    NUMBER = 0x43,
    DIFFICULTY = 0x44,
    GASLIMIT = 0x45,
    
    // Logging
    LOG0 = 0xA0,
    LOG1 = 0xA1,
    LOG2 = 0xA2,
    LOG3 = 0xA3,
    LOG4 = 0xA4,
    
    // System calls
    CREATE = 0xF0,
    CALL = 0xF1,
    CALLCODE = 0xF2,
    RETURN = 0xF3,
    DELEGATECALL = 0xF4,
    CREATE2 = 0xF5,
    STATICCALL = 0xFA,
    REVERT = 0xFD,
    INVALID = 0xFE,
    SELFDESTRUCT = 0xFF
};

/**
 * @brief VM execution result
 */
struct ExecutionResult {
    bool success;
    std::vector<uint8_t> return_data;
    uint64_t gas_used;
    std::string error_message;
    
    ExecutionResult() : success(false), gas_used(0) {}
};

/**
 * @brief VM execution context
 */
struct ExecutionContext {
    std::vector<uint8_t> code;           ///< Contract bytecode
    std::vector<uint8_t> input_data;     ///< Input data for contract call
    std::string caller_address;          ///< Address of the caller
    std::string contract_address;        ///< Address of the contract
    uint64_t gas_limit;                  ///< Gas limit for execution
    uint64_t gas_price;                  ///< Gas price
    uint64_t value;                      ///< Value being transferred
    uint64_t block_number;               ///< Current block number
    uint64_t block_timestamp;            ///< Current block timestamp
    std::string block_coinbase;          ///< Block coinbase address
    
    ExecutionContext() : gas_limit(0), gas_price(0), value(0), 
                        block_number(0), block_timestamp(0) {}
};

/**
 * @brief VM state during execution
 */
struct VMState {
    std::stack<uint256_t> stack;         ///< Execution stack
    std::vector<uint8_t> memory;         ///< Memory buffer
    std::map<uint256_t, uint256_t> storage; ///< Contract storage
    uint64_t pc;                         ///< Program counter
    uint64_t gas_remaining;              ///< Remaining gas
    bool halted;                         ///< Execution halted flag
    
    VMState(uint64_t gas_limit) : pc(0), gas_remaining(gas_limit), halted(false) {
        memory.reserve(1024); // Reserve initial memory
    }
};

/**
 * @brief Gas costs for different operations
 */
struct GasCosts {
    static constexpr uint64_t ZERO = 0;
    static constexpr uint64_t BASE = 2;
    static constexpr uint64_t VERY_LOW = 3;
    static constexpr uint64_t LOW = 5;
    static constexpr uint64_t MID = 8;
    static constexpr uint64_t HIGH = 10;
    static constexpr uint64_t EXT = 20;
    static constexpr uint64_t SPECIAL = 1;
    static constexpr uint64_t JUMPDEST = 1;
    static constexpr uint64_t SELFDESTRUCT = 5000;
    static constexpr uint64_t CREATE = 32000;
    static constexpr uint64_t CALL = 40;
    static constexpr uint64_t CALLVALUE = 9000;
    static constexpr uint64_t NEWACCOUNT = 25000;
    static constexpr uint64_t EXP = 10;
    static constexpr uint64_t MEMORY = 3;
    static constexpr uint64_t QUADCOEFFDIV = 512;
    static constexpr uint64_t LOG = 375;
    static constexpr uint64_t LOGDATA = 8;
    static constexpr uint64_t LOGTOPIC = 375;
    static constexpr uint64_t SHA3 = 30;
    static constexpr uint64_t SHA3WORD = 6;
    static constexpr uint64_t COPY = 3;
    static constexpr uint64_t BLOCKHASH = 20;
    static constexpr uint64_t CODECOPY = 3;
    static constexpr uint64_t EXTCODECOPY = 3;
    static constexpr uint64_t BALANCE = 20;
    static constexpr uint64_t EXTCODESIZE = 20;
    static constexpr uint64_t EXTCODEHASH = 20;
    static constexpr uint64_t SLOAD = 50;
    static constexpr uint64_t SSTORE_SET = 20000;
    static constexpr uint64_t SSTORE_RESET = 5000;
    static constexpr uint64_t SSTORE_CLEARS = 15000;
    static constexpr uint64_t INVALID = 0;
};

/**
 * @brief Virtual Machine class
 */
class VirtualMachine {
public:
    /**
     * @brief Constructor
     */
    VirtualMachine();
    
    /**
     * @brief Destructor
     */
    ~VirtualMachine();
    
    /**
     * @brief Execute contract code
     * @param context Execution context
     * @return Execution result
     */
    ExecutionResult execute(const ExecutionContext& context);
    
    /**
     * @brief Get gas cost for an instruction
     * @param opcode Instruction opcode
     * @param state Current VM state
     * @return Gas cost
     */
    uint64_t getGasCost(Opcode opcode, const VMState& state) const;
    
    /**
     * @brief Validate bytecode
     * @param code Contract bytecode
     * @return True if valid
     */
    bool validateBytecode(const std::vector<uint8_t>& code) const;
    
    /**
     * @brief Get VM statistics
     * @return JSON string with VM statistics
     */
    std::string getStatistics() const;
    
private:
    // Instruction execution
    void executeInstruction(Opcode opcode, VMState& state, const ExecutionContext& context);
    
    // Instruction handlers
    void handlePush(VMState& state, const std::vector<uint8_t>& code, uint8_t push_size);
    void handlePop(VMState& state);
    void handleDup(VMState& state, uint8_t depth);
    void handleSwap(VMState& state, uint8_t depth);
    void handleAdd(VMState& state);
    void handleMul(VMState& state);
    void handleSub(VMState& state);
    void handleDiv(VMState& state);
    void handleMod(VMState& state);
    void handleLt(VMState& state);
    void handleGt(VMState& state);
    void handleEq(VMState& state);
    void handleIsZero(VMState& state);
    void handleAnd(VMState& state);
    void handleOr(VMState& state);
    void handleXor(VMState& state);
    void handleNot(VMState& state);
    void handleJump(VMState& state, const std::vector<uint8_t>& code);
    void handleJumpI(VMState& state, const std::vector<uint8_t>& code);
    void handleJumpDest(VMState& state);
    void handlePC(VMState& state);
    void handleMSize(VMState& state);
    void handleGas(VMState& state);
    void handleMLoad(VMState& state);
    void handleMStore(VMState& state);
    void handleMStore8(VMState& state);
    void handleSLoad(VMState& state, const ExecutionContext& context);
    void handleSStore(VMState& state, const ExecutionContext& context);
    void handleAddress(VMState& state, const ExecutionContext& context);
    void handleBalance(VMState& state, const ExecutionContext& context);
    void handleCaller(VMState& state, const ExecutionContext& context);
    void handleCallValue(VMState& state, const ExecutionContext& context);
    void handleCallDataLoad(VMState& state, const ExecutionContext& context);
    void handleCallDataSize(VMState& state, const ExecutionContext& context);
    void handleCallDataCopy(VMState& state, const ExecutionContext& context);
    void handleCodeSize(VMState& state, const ExecutionContext& context);
    void handleCodeCopy(VMState& state, const ExecutionContext& context);
    void handleGasPrice(VMState& state, const ExecutionContext& context);
    void handleBlockHash(VMState& state, const ExecutionContext& context);
    void handleCoinbase(VMState& state, const ExecutionContext& context);
    void handleTimestamp(VMState& state, const ExecutionContext& context);
    void handleNumber(VMState& state, const ExecutionContext& context);
    void handleDifficulty(VMState& state, const ExecutionContext& context);
    void handleGasLimit(VMState& state, const ExecutionContext& context);
    void handleSha3(VMState& state);
    void handleReturn(VMState& state, const ExecutionContext& context);
    void handleRevert(VMState& state, const ExecutionContext& context);
    void handleInvalid(VMState& state);
    void handleSelfDestruct(VMState& state, const ExecutionContext& context);
    
    // Utility functions
    void expandMemory(VMState& state, uint64_t size);
    uint64_t calculateMemoryCost(uint64_t size) const;
    bool isValidJumpDestination(const std::vector<uint8_t>& code, uint64_t dest) const;
    uint256_t readUint256(const std::vector<uint8_t>& data, uint64_t offset) const;
    void writeUint256(std::vector<uint8_t>& data, uint64_t offset, const uint256_t& value) const;
    
    // State management
    std::map<std::string, std::map<uint256_t, uint256_t>> contract_storage_;
    std::map<std::string, uint256_t> account_balances_;
    std::map<std::string, uint64_t> account_nonces_;
    
    // Statistics
    uint64_t total_executions_;
    uint64_t total_gas_used_;
    uint64_t total_instructions_executed_;
};

} // namespace vm
} // namespace deo

#endif // DEO_VM_VIRTUAL_MACHINE_H