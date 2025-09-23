/**
 * @file instruction_handlers.cpp
 * @brief Virtual Machine instruction handlers implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "vm/virtual_machine.h"
#include "vm/uint256.h"
#include "utils/logger.h"

#include <stdexcept>

namespace deo {
namespace vm {

void VirtualMachine::executeInstruction(Opcode opcode, VMState& state, const ExecutionContext& context) {
    switch (opcode) {
        case Opcode::PUSH0:
            state.stack.push(uint256_t(0));
            break;
        case Opcode::PUSH1:
        case Opcode::PUSH2:
        case Opcode::PUSH3:
        case Opcode::PUSH4:
        case Opcode::PUSH5:
        case Opcode::PUSH6:
        case Opcode::PUSH7:
        case Opcode::PUSH8:
            handlePush(state, context.code, static_cast<uint8_t>(opcode) - 0x5F);
            break;
        case Opcode::PUSH32:
            handlePush(state, context.code, 32);
            break;
        case Opcode::POP:
            handlePop(state);
            break;
        case Opcode::DUP1:
        case Opcode::DUP2:
        case Opcode::DUP3:
        case Opcode::DUP4:
        case Opcode::DUP5:
        case Opcode::DUP6:
        case Opcode::DUP7:
        case Opcode::DUP8:
            handleDup(state, static_cast<uint8_t>(opcode) - 0x80 + 1);
            break;
        case Opcode::SWAP1:
        case Opcode::SWAP2:
        case Opcode::SWAP3:
        case Opcode::SWAP4:
        case Opcode::SWAP5:
        case Opcode::SWAP6:
        case Opcode::SWAP7:
        case Opcode::SWAP8:
            handleSwap(state, static_cast<uint8_t>(opcode) - 0x90 + 1);
            break;
        case Opcode::ADD:
            handleAdd(state);
            break;
        case Opcode::MUL:
            handleMul(state);
            break;
        case Opcode::SUB:
            handleSub(state);
            break;
        case Opcode::DIV:
            handleDiv(state);
            break;
        case Opcode::MOD:
            handleMod(state);
            break;
        case Opcode::LT:
            handleLt(state);
            break;
        case Opcode::GT:
            handleGt(state);
            break;
        case Opcode::EQ:
            handleEq(state);
            break;
        case Opcode::ISZERO:
            handleIsZero(state);
            break;
        case Opcode::AND:
            handleAnd(state);
            break;
        case Opcode::OR:
            handleOr(state);
            break;
        case Opcode::XOR:
            handleXor(state);
            break;
        case Opcode::NOT:
            handleNot(state);
            break;
        case Opcode::JUMP:
            handleJump(state, context.code);
            break;
        case Opcode::JUMPI:
            handleJumpI(state, context.code);
            break;
        case Opcode::JUMPDEST:
            handleJumpDest(state);
            break;
        case Opcode::PC:
            handlePC(state);
            break;
        case Opcode::MSIZE:
            handleMSize(state);
            break;
        case Opcode::GAS:
            handleGas(state);
            break;
        case Opcode::MLOAD:
            handleMLoad(state);
            break;
        case Opcode::MSTORE:
            handleMStore(state);
            break;
        case Opcode::MSTORE8:
            handleMStore8(state);
            break;
        case Opcode::SLOAD:
            handleSLoad(state, context);
            break;
        case Opcode::SSTORE:
            handleSStore(state, context);
            break;
        case Opcode::ADDRESS:
            handleAddress(state, context);
            break;
        case Opcode::BALANCE:
            handleBalance(state, context);
            break;
        case Opcode::CALLER:
            handleCaller(state, context);
            break;
        case Opcode::CALLVALUE:
            handleCallValue(state, context);
            break;
        case Opcode::CALLDATALOAD:
            handleCallDataLoad(state, context);
            break;
        case Opcode::CALLDATASIZE:
            handleCallDataSize(state, context);
            break;
        case Opcode::CALLDATACOPY:
            handleCallDataCopy(state, context);
            break;
        case Opcode::CODESIZE:
            handleCodeSize(state, context);
            break;
        case Opcode::CODECOPY:
            handleCodeCopy(state, context);
            break;
        case Opcode::GASPRICE:
            handleGasPrice(state, context);
            break;
        case Opcode::BLOCKHASH:
            handleBlockHash(state, context);
            break;
        case Opcode::COINBASE:
            handleCoinbase(state, context);
            break;
        case Opcode::TIMESTAMP:
            handleTimestamp(state, context);
            break;
        case Opcode::NUMBER:
            handleNumber(state, context);
            break;
        case Opcode::DIFFICULTY:
            handleDifficulty(state, context);
            break;
        case Opcode::GASLIMIT:
            handleGasLimit(state, context);
            break;
        case Opcode::SHA3:
            handleSha3(state);
            break;
        case Opcode::RETURN:
            handleReturn(state, context);
            state.halted = true;
            break;
        case Opcode::REVERT:
            handleRevert(state, context);
            state.halted = true;
            break;
        case Opcode::INVALID:
            handleInvalid(state);
            state.halted = true;
            break;
        case Opcode::SELFDESTRUCT:
            handleSelfDestruct(state, context);
            state.halted = true;
            break;
        default:
            throw std::runtime_error("Unknown opcode: " + std::to_string(static_cast<int>(opcode)));
    }
}

// Stack operations
void VirtualMachine::handlePush(VMState& state, const std::vector<uint8_t>& code, uint8_t push_size) {
    // Check stack overflow (EVM stack limit is 1024 items)
    if (state.stack.size() >= 1024) {
        throw std::runtime_error("Stack overflow");
    }
    
    // Check bounds: we need 1 byte for opcode + push_size bytes for data
    if (state.pc + 1 + push_size > code.size()) {
        throw std::runtime_error("Push instruction out of bounds: PC=" + std::to_string(state.pc) + 
                                ", push_size=" + std::to_string(push_size) + 
                                ", code_size=" + std::to_string(code.size()));
    }
    
    // Read the specified number of bytes and convert to uint256_t
    uint256_t value = uint256_t(0);
    for (uint8_t i = 0; i < push_size; ++i) {
        uint8_t byte_value = code[state.pc + 1 + i];
        value = (value * uint256_t(256)) + uint256_t(byte_value);
    }
    
    // Push value onto stack
    state.stack.push(value);
    
    // Increment PC by 1 (opcode) + push_size (data bytes)
    // The main loop will NOT increment PC for PUSH instructions
    state.pc += push_size;
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "PUSH" + std::to_string(push_size) + " executed: pushed " + 
                  value.toString() + " onto stack. Stack size: " + std::to_string(state.stack.size()));
}

void VirtualMachine::handlePop(VMState& state) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow");
    }
    state.stack.pop();
}

void VirtualMachine::handleDup(VMState& state, uint8_t depth) {
    if (state.stack.size() < depth) {
        throw std::runtime_error("Stack underflow for DUP");
    }
    
    std::stack<uint256_t> temp;
    for (uint8_t i = 0; i < depth - 1; ++i) {
        temp.push(state.stack.top());
        state.stack.pop();
    }
    
    uint256_t value = state.stack.top();
    
    while (!temp.empty()) {
        state.stack.push(temp.top());
        temp.pop();
    }
    
    state.stack.push(value);
}

void VirtualMachine::handleSwap(VMState& state, uint8_t depth) {
    if (state.stack.size() < static_cast<size_t>(depth + 1)) {
        throw std::runtime_error("Stack underflow for SWAP");
    }
    
    std::stack<uint256_t> temp;
    for (uint8_t i = 0; i < depth; ++i) {
        temp.push(state.stack.top());
        state.stack.pop();
    }
    
    uint256_t top = state.stack.top();
    state.stack.pop();
    uint256_t bottom = temp.top();
    temp.pop();
    
    state.stack.push(bottom);
    
    while (!temp.empty()) {
        state.stack.push(temp.top());
        temp.pop();
    }
    
    state.stack.push(top);
}

// Arithmetic operations
void VirtualMachine::handleAdd(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for ADD: stack size=" + std::to_string(state.stack.size()));
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    uint256_t result = a + b;
    state.stack.push(result);
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "ADD executed: " + a.toString() + " + " + b.toString() + " = " + result.toString());
}

void VirtualMachine::handleMul(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for MUL");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(a * b);
}

void VirtualMachine::handleSub(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for SUB");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(a - b);
}

void VirtualMachine::handleDiv(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for DIV");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    if (b.isZero()) {
        state.stack.push(uint256_t(0));
    } else {
        state.stack.push(a / b);
    }
}

void VirtualMachine::handleMod(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for MOD");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    if (b.isZero()) {
        state.stack.push(uint256_t(0));
    } else {
        state.stack.push(a % b);
    }
}

// Comparison operations
void VirtualMachine::handleLt(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for LT");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(a < b ? uint256_t(1) : uint256_t(0));
}

void VirtualMachine::handleGt(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for GT");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(a > b ? uint256_t(1) : uint256_t(0));
}

void VirtualMachine::handleEq(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for EQ");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(a == b ? uint256_t(1) : uint256_t(0));
}

void VirtualMachine::handleIsZero(VMState& state) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow for ISZERO");
    }
    
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(a.isZero() ? uint256_t(1) : uint256_t(0));
}

// Bitwise operations
void VirtualMachine::handleAnd(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for AND");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(a & b);
}

void VirtualMachine::handleOr(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for OR");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(a | b);
}

void VirtualMachine::handleXor(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for XOR");
    }
    
    uint256_t b = state.stack.top();
    state.stack.pop();
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(a ^ b);
}

void VirtualMachine::handleNot(VMState& state) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow for NOT");
    }
    
    uint256_t a = state.stack.top();
    state.stack.pop();
    
    state.stack.push(~a);
}

// Control flow
void VirtualMachine::handleJump(VMState& state, const std::vector<uint8_t>& code) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow for JUMP");
    }
    
    uint256_t dest = state.stack.top();
    state.stack.pop();
    
    if (!isValidJumpDestination(code, dest.toUint64())) {
        throw std::runtime_error("Invalid jump destination");
    }
    
    state.pc = dest.toUint64();
}

void VirtualMachine::handleJumpI(VMState& state, const std::vector<uint8_t>& code) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for JUMPI");
    }
    
    uint256_t dest = state.stack.top();
    state.stack.pop();
    uint256_t condition = state.stack.top();
    state.stack.pop();
    
    if (!condition.isZero()) {
        if (!isValidJumpDestination(code, dest.toUint64())) {
            throw std::runtime_error("Invalid jump destination");
        }
        state.pc = dest.toUint64();
    }
}

void VirtualMachine::handleJumpDest(VMState& /* state */) {
    // JUMPDEST is a no-op, just marks a valid jump destination
}

void VirtualMachine::handlePC(VMState& state) {
    state.stack.push(uint256_t(state.pc));
}

void VirtualMachine::handleMSize(VMState& state) {
    state.stack.push(uint256_t(state.memory.size()));
}

void VirtualMachine::handleGas(VMState& state) {
    state.stack.push(uint256_t(state.gas_remaining));
}

// Memory operations
void VirtualMachine::handleMLoad(VMState& state) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow for MLOAD");
    }
    
    uint256_t offset = state.stack.top();
    state.stack.pop();
    
    expandMemory(state, offset.toUint64() + 32);
    
    uint256_t value = readUint256(state.memory, offset.toUint64());
    state.stack.push(value);
}

void VirtualMachine::handleMStore(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for MSTORE");
    }
    
    uint256_t offset = state.stack.top();
    state.stack.pop();
    uint256_t value = state.stack.top();
    state.stack.pop();
    
    expandMemory(state, offset.toUint64() + 32);
    writeUint256(state.memory, offset.toUint64(), value);
}

void VirtualMachine::handleMStore8(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for MSTORE8");
    }
    
    uint256_t offset = state.stack.top();
    state.stack.pop();
    uint256_t value = state.stack.top();
    state.stack.pop();
    
    expandMemory(state, offset.toUint64() + 1);
    state.memory[offset.toUint64()] = static_cast<uint8_t>(value.toUint64() & 0xFF);
}

// Storage operations
void VirtualMachine::handleSLoad(VMState& state, const ExecutionContext& context) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow for SLOAD");
    }
    
    uint256_t key = state.stack.top();
    state.stack.pop();
    
    auto it = contract_storage_[context.contract_address].find(key);
    if (it != contract_storage_[context.contract_address].end()) {
        state.stack.push(it->second);
    } else {
        state.stack.push(uint256_t(0));
    }
}

void VirtualMachine::handleSStore(VMState& state, const ExecutionContext& context) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for SSTORE");
    }
    
    uint256_t key = state.stack.top();
    state.stack.pop();
    uint256_t value = state.stack.top();
    state.stack.pop();
    
    contract_storage_[context.contract_address][key] = value;
}

// System operations
void VirtualMachine::handleAddress(VMState& state, const ExecutionContext& /* context */) {
    // Convert address string to uint256_t (simplified)
    uint256_t address = uint256_t(0); // Simplified implementation
    state.stack.push(address);
}

void VirtualMachine::handleBalance(VMState& state, const ExecutionContext& /* context */) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow for BALANCE");
    }
    
    [[maybe_unused]] uint256_t address = state.stack.top();
    state.stack.pop();
    
    // Simplified balance lookup
    state.stack.push(uint256_t(0));
}

void VirtualMachine::handleCaller(VMState& state, const ExecutionContext& /* context */) {
    // Convert caller address string to uint256_t (simplified)
    uint256_t caller = uint256_t(0); // Simplified implementation
    state.stack.push(caller);
}

void VirtualMachine::handleCallValue(VMState& state, const ExecutionContext& context) {
    state.stack.push(uint256_t(context.value));
}

void VirtualMachine::handleCallDataLoad(VMState& state, const ExecutionContext& context) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow for CALLDATALOAD");
    }
    
    uint256_t offset = state.stack.top();
    state.stack.pop();
    
    if (offset.toUint64() >= context.input_data.size()) {
        state.stack.push(uint256_t(0));
    } else {
        uint256_t value = readUint256(context.input_data, offset.toUint64());
        state.stack.push(value);
    }
}

void VirtualMachine::handleCallDataSize(VMState& state, const ExecutionContext& context) {
    state.stack.push(uint256_t(context.input_data.size()));
}

void VirtualMachine::handleCallDataCopy(VMState& state, const ExecutionContext& context) {
    if (state.stack.size() < 3) {
        throw std::runtime_error("Stack underflow for CALLDATACOPY");
    }
    
    uint256_t dest_offset = state.stack.top();
    state.stack.pop();
    uint256_t src_offset = state.stack.top();
    state.stack.pop();
    uint256_t size = state.stack.top();
    state.stack.pop();
    
    expandMemory(state, dest_offset.toUint64() + size.toUint64());
    
    for (uint64_t i = 0; i < size.toUint64(); ++i) {
        if (src_offset.toUint64() + i < context.input_data.size()) {
            state.memory[dest_offset.toUint64() + i] = context.input_data[src_offset.toUint64() + i];
        } else {
            state.memory[dest_offset.toUint64() + i] = 0;
        }
    }
}

void VirtualMachine::handleCodeSize(VMState& state, const ExecutionContext& context) {
    state.stack.push(uint256_t(context.code.size()));
}

void VirtualMachine::handleCodeCopy(VMState& state, const ExecutionContext& context) {
    if (state.stack.size() < 3) {
        throw std::runtime_error("Stack underflow for CODECOPY");
    }
    
    uint256_t dest_offset = state.stack.top();
    state.stack.pop();
    uint256_t src_offset = state.stack.top();
    state.stack.pop();
    uint256_t size = state.stack.top();
    state.stack.pop();
    
    expandMemory(state, dest_offset.toUint64() + size.toUint64());
    
    for (uint64_t i = 0; i < size.toUint64(); ++i) {
        if (src_offset.toUint64() + i < context.code.size()) {
            state.memory[dest_offset.toUint64() + i] = context.code[src_offset.toUint64() + i];
        } else {
            state.memory[dest_offset.toUint64() + i] = 0;
        }
    }
}

void VirtualMachine::handleGasPrice(VMState& state, const ExecutionContext& context) {
    state.stack.push(uint256_t(context.gas_price));
}

void VirtualMachine::handleBlockHash(VMState& state, const ExecutionContext& /* context */) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow for BLOCKHASH");
    }
    
    [[maybe_unused]] uint256_t block_number = state.stack.top();
    state.stack.pop();
    
    // Simplified block hash lookup
    state.stack.push(uint256_t(0));
}

void VirtualMachine::handleCoinbase(VMState& state, const ExecutionContext& /* context */) {
    // Convert coinbase address string to uint256_t (simplified)
    uint256_t coinbase = uint256_t(0); // Simplified implementation
    state.stack.push(coinbase);
}

void VirtualMachine::handleTimestamp(VMState& state, const ExecutionContext& context) {
    state.stack.push(uint256_t(context.block_timestamp));
}

void VirtualMachine::handleNumber(VMState& state, const ExecutionContext& context) {
    state.stack.push(uint256_t(context.block_number));
}

void VirtualMachine::handleDifficulty(VMState& state, const ExecutionContext& /* context */) {
    // Simplified difficulty
    state.stack.push(uint256_t(1));
}

void VirtualMachine::handleGasLimit(VMState& state, const ExecutionContext& context) {
    state.stack.push(uint256_t(context.gas_limit));
}

void VirtualMachine::handleSha3(VMState& state) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for SHA3");
    }
    
    uint256_t offset = state.stack.top();
    state.stack.pop();
    uint256_t size = state.stack.top();
    state.stack.pop();
    
    expandMemory(state, offset.toUint64() + size.toUint64());
    
    // Simplified SHA3 implementation
    std::vector<uint8_t> data(state.memory.begin() + offset.toUint64(), 
                             state.memory.begin() + offset.toUint64() + size.toUint64());
    
    // Use a simple hash for now
    uint256_t hash = uint256_t(0);
    for (size_t i = 0; i < data.size(); ++i) {
        hash = hash + uint256_t(data[i]);
    }
    
    state.stack.push(hash);
}

void VirtualMachine::handleReturn(VMState& state, const ExecutionContext& /* context */) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for RETURN");
    }
    
    uint256_t offset = state.stack.top();
    state.stack.pop();
    uint256_t size = state.stack.top();
    state.stack.pop();
    
    expandMemory(state, offset.toUint64() + size.toUint64());
    
    // Return data is stored in memory
    // This would be handled by the caller
}

void VirtualMachine::handleRevert(VMState& state, const ExecutionContext& /* context */) {
    if (state.stack.size() < 2) {
        throw std::runtime_error("Stack underflow for REVERT");
    }
    
    uint256_t offset = state.stack.top();
    state.stack.pop();
    uint256_t size = state.stack.top();
    state.stack.pop();
    
    expandMemory(state, offset.toUint64() + size.toUint64());
    
    // Revert data is stored in memory
    // This would be handled by the caller
}

void VirtualMachine::handleInvalid(VMState& /* state */) {
    throw std::runtime_error("INVALID instruction executed");
}

void VirtualMachine::handleSelfDestruct(VMState& state, const ExecutionContext& /* context */) {
    if (state.stack.empty()) {
        throw std::runtime_error("Stack underflow for SELFDESTRUCT");
    }
    
    [[maybe_unused]] uint256_t beneficiary = state.stack.top();
    state.stack.pop();
    
    // Simplified self-destruct implementation
    // In a real implementation, this would transfer the contract's balance
    // to the beneficiary and mark the contract for deletion
}

// Utility functions
void VirtualMachine::expandMemory(VMState& state, uint64_t size) {
    if (size > state.memory.size()) {
        state.memory.resize(size, 0);
    }
}

uint64_t VirtualMachine::calculateMemoryCost(uint64_t size) const {
    return (size / 32) * GasCosts::MEMORY;
}

bool VirtualMachine::isValidJumpDestination(const std::vector<uint8_t>& code, uint64_t dest) const {
    if (dest >= code.size()) return false;
    return static_cast<Opcode>(code[dest]) == Opcode::JUMPDEST;
}

uint256_t VirtualMachine::readUint256(const std::vector<uint8_t>& data, uint64_t offset) const {
    uint256_t result;
    
    for (int i = 0; i < 32; ++i) {
        if (offset + i < data.size()) {
            result.setByte(31 - i, data[offset + i]);
        }
    }
    
    return result;
}

void VirtualMachine::writeUint256(std::vector<uint8_t>& data, uint64_t offset, const uint256_t& value) const {
    for (int i = 0; i < 32; ++i) {
        if (offset + i < data.size()) {
            data[offset + i] = value.getByte(31 - i);
        }
    }
}

} // namespace vm
} // namespace deo
