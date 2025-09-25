/**
 * @file virtual_machine.cpp
 * @brief Virtual Machine for smart contract execution
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "vm/virtual_machine.h"
#include "vm/uint256.h"
#include "utils/logger.h"
#include "crypto/hash.h"

#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace deo {
namespace vm {

VirtualMachine::VirtualMachine() 
    : total_executions_(0)
    , total_gas_used_(0)
    , total_instructions_executed_(0) {
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Virtual Machine initialized");
}

VirtualMachine::~VirtualMachine() {
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Virtual Machine destroyed");
}

ExecutionResult VirtualMachine::execute(const ExecutionContext& context) {
    ExecutionResult result;
    
    try {
        // Validate execution context
        if (context.code.empty()) {
            result.error_message = "Empty bytecode";
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Empty bytecode provided");
            return result;
        }
        
        if (context.gas_limit == 0) {
            result.error_message = "Zero gas limit";
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Zero gas limit provided");
            return result;
        }
        
        // Validate bytecode
        if (!validateBytecode(context.code)) {
            result.error_message = "Invalid bytecode";
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Bytecode validation failed. Code size: " + std::to_string(context.code.size()));
            return result;
        }
        
        // Initialize VM state
        VMState state(context.gas_limit);
        
        // Execute instructions
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Starting VM execution. Code size: " + std::to_string(context.code.size()) + ", Gas limit: " + std::to_string(context.gas_limit));
        
        uint64_t max_instructions = 10000; // Prevent infinite loops
        uint64_t instruction_count = 0;
        
        while (!state.halted && state.pc < context.code.size() && instruction_count < max_instructions) {
            Opcode opcode = static_cast<Opcode>(context.code[state.pc]);
            // Debug output disabled for cleaner test output
            // DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Executing opcode: 0x" + std::to_string(static_cast<int>(opcode)) + " at PC: " + std::to_string(state.pc));
            // std::cout << "DEBUG: Executing opcode: 0x" << std::hex << static_cast<int>(opcode) << " at PC: " << std::dec << state.pc << std::endl;
            
            // Check gas
            uint64_t gas_cost = getGasCost(opcode, state);
            if (state.gas_remaining < gas_cost) {
                result.error_message = "Out of gas";
                result.gas_used = context.gas_limit - state.gas_remaining;
                DEO_LOG_ERROR(VIRTUAL_MACHINE, "Out of gas. Remaining: " + std::to_string(state.gas_remaining) + ", Cost: " + std::to_string(gas_cost));
                return result;
            }
            
            // Execute instruction
            state.gas_remaining -= gas_cost;
            
            // Execute instruction based on opcode
            executeInstruction(opcode, state, context);
            
            // Increment PC after instruction execution
            // PUSH instructions handle their own PC increment
            if (!state.halted) {
                if (opcode >= Opcode::PUSH1 && opcode <= Opcode::PUSH32) {
                    // PUSH instructions already incremented PC by the correct amount
                    // Don't increment again
                } else {
                    // All other instructions just increment by 1
                    state.pc++;
                }
            }
            
            total_instructions_executed_++;
            instruction_count++;
        }
        
        // Check for infinite loop
        if (instruction_count >= max_instructions) {
            result.error_message = "Maximum instruction limit reached";
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Maximum instruction limit reached");
            return result;
        }
        
        // Update statistics
        total_executions_++;
        total_gas_used_ += (context.gas_limit - state.gas_remaining);
        
        result.success = true;
        result.gas_used = context.gas_limit - state.gas_remaining;
        
        // Copy return data from memory if available
        if (!state.memory.empty()) {
            result.return_data = state.memory;
        }
        
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Contract execution completed successfully. Gas used: " + std::to_string(result.gas_used));
        
    } catch (const std::exception& e) {
        result.error_message = "Execution error: " + std::string(e.what());
        DEO_LOG_ERROR(VIRTUAL_MACHINE, result.error_message);
    }
    
    return result;
}

uint64_t VirtualMachine::getGasCost(Opcode opcode, const VMState& /* state */) const {
    switch (opcode) {
        case Opcode::STOP:
            return GasCosts::ZERO;
        case Opcode::PUSH0:
        case Opcode::PUSH1:
        case Opcode::PUSH2:
        case Opcode::PUSH3:
        case Opcode::PUSH4:
        case Opcode::PUSH5:
        case Opcode::PUSH6:
        case Opcode::PUSH7:
        case Opcode::PUSH8:
            return GasCosts::VERY_LOW;
        case Opcode::PUSH32:
            return GasCosts::VERY_LOW;
        case Opcode::POP:
        case Opcode::DUP1:
        case Opcode::DUP2:
        case Opcode::DUP3:
        case Opcode::DUP4:
        case Opcode::DUP5:
        case Opcode::DUP6:
        case Opcode::DUP7:
        case Opcode::DUP8:
        case Opcode::SWAP1:
        case Opcode::SWAP2:
        case Opcode::SWAP3:
        case Opcode::SWAP4:
        case Opcode::SWAP5:
        case Opcode::SWAP6:
        case Opcode::SWAP7:
        case Opcode::SWAP8:
            return GasCosts::VERY_LOW;
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::DIV:
        case Opcode::MOD:
            return GasCosts::VERY_LOW;
        case Opcode::MUL:
            return GasCosts::LOW;
        case Opcode::LT:
        case Opcode::GT:
        case Opcode::EQ:
        case Opcode::ISZERO:
        case Opcode::AND:
        case Opcode::OR:
        case Opcode::XOR:
        case Opcode::NOT:
            return GasCosts::VERY_LOW;
        case Opcode::JUMP:
        case Opcode::JUMPI:
            return GasCosts::MID;
        case Opcode::JUMPDEST:
            return GasCosts::JUMPDEST;
        case Opcode::PC:
        case Opcode::MSIZE:
        case Opcode::GAS:
            return GasCosts::BASE;
        case Opcode::MLOAD:
            return GasCosts::VERY_LOW;
        case Opcode::MSTORE:
        case Opcode::MSTORE8:
            return GasCosts::VERY_LOW;
        case Opcode::SLOAD:
            return GasCosts::SLOAD;
        case Opcode::SSTORE:
            return GasCosts::SSTORE_SET; // Simplified
        case Opcode::ADDRESS:
        case Opcode::BALANCE:
        case Opcode::CALLER:
        case Opcode::CALLVALUE:
        case Opcode::CALLDATALOAD:
        case Opcode::CALLDATASIZE:
        case Opcode::CODESIZE:
        case Opcode::GASPRICE:
        case Opcode::COINBASE:
        case Opcode::TIMESTAMP:
        case Opcode::NUMBER:
        case Opcode::DIFFICULTY:
        case Opcode::GASLIMIT:
            return GasCosts::BASE;
        case Opcode::CALLDATACOPY:
        case Opcode::CODECOPY:
            return GasCosts::VERY_LOW;
        case Opcode::BLOCKHASH:
            return GasCosts::BLOCKHASH;
        case Opcode::SHA3:
            return GasCosts::SHA3;
        case Opcode::RETURN:
        case Opcode::REVERT:
            return GasCosts::ZERO;
        case Opcode::INVALID:
            return GasCosts::ZERO;
        case Opcode::SELFDESTRUCT:
            return GasCosts::SELFDESTRUCT;
        default:
            return GasCosts::INVALID;
    }
}

bool VirtualMachine::validateBytecode(const std::vector<uint8_t>& code) const {
    if (code.empty()) return true;
    
    // Check for valid opcodes and jump destinations
    for (size_t i = 0; i < code.size(); ++i) {
        Opcode opcode = static_cast<Opcode>(code[i]);
        
        // Validate opcode is one of the defined opcodes
        if (!isValidOpcode(opcode)) {
            DEO_LOG_ERROR(VIRTUAL_MACHINE, "Invalid opcode 0x" + 
                         std::to_string(static_cast<int>(opcode)) + " at position " + std::to_string(i));
            return false;
        }
        
        // Check PUSH instructions
        if (opcode >= Opcode::PUSH1 && opcode <= Opcode::PUSH32) {
            uint8_t push_size = static_cast<uint8_t>(opcode) - 0x5F;
            if (opcode == Opcode::PUSH32) push_size = 32;
            
            // Check if we have enough bytes for the push data
            // We need 1 byte for opcode + push_size bytes for data
            if (i + 1 + push_size > code.size()) {
                DEO_LOG_ERROR(VIRTUAL_MACHINE, "Invalid PUSH instruction at position " + std::to_string(i) + 
                             ": need " + std::to_string(1 + push_size) + " bytes, have " + 
                             std::to_string(code.size() - i) + " bytes remaining");
                return false;
            }
            
            // Skip the push data bytes in the validation loop
            i += push_size;
        }
    }
    
    return true;
}

bool VirtualMachine::isValidOpcode(Opcode opcode) const {
    // Check if opcode is one of the defined opcodes
    switch (opcode) {
        case Opcode::STOP:
        case Opcode::PUSH0:
        case Opcode::PUSH1:
        case Opcode::PUSH2:
        case Opcode::PUSH3:
        case Opcode::PUSH4:
        case Opcode::PUSH5:
        case Opcode::PUSH6:
        case Opcode::PUSH7:
        case Opcode::PUSH8:
        case Opcode::PUSH32:
        case Opcode::POP:
        case Opcode::DUP1:
        case Opcode::DUP2:
        case Opcode::DUP3:
        case Opcode::DUP4:
        case Opcode::DUP5:
        case Opcode::DUP6:
        case Opcode::DUP7:
        case Opcode::DUP8:
        case Opcode::SWAP1:
        case Opcode::SWAP2:
        case Opcode::SWAP3:
        case Opcode::SWAP4:
        case Opcode::SWAP5:
        case Opcode::SWAP6:
        case Opcode::SWAP7:
        case Opcode::SWAP8:
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::DIV:
        case Opcode::MOD:
        case Opcode::MUL:
        case Opcode::ADDMOD:
        case Opcode::MULMOD:
        case Opcode::EXP:
        case Opcode::SIGNEXTEND:
        case Opcode::LT:
        case Opcode::GT:
        case Opcode::SLT:
        case Opcode::SGT:
        case Opcode::EQ:
        case Opcode::ISZERO:
        case Opcode::AND:
        case Opcode::OR:
        case Opcode::XOR:
        case Opcode::NOT:
        case Opcode::BYTE:
        case Opcode::SHL:
        case Opcode::SHR:
        case Opcode::SAR:
        case Opcode::JUMP:
        case Opcode::JUMPI:
        case Opcode::PC:
        case Opcode::MSIZE:
        case Opcode::GAS:
        case Opcode::JUMPDEST:
        case Opcode::MLOAD:
        case Opcode::MSTORE:
        case Opcode::MSTORE8:
        case Opcode::SLOAD:
        case Opcode::SSTORE:
        case Opcode::SHA3:
        case Opcode::ADDRESS:
        case Opcode::BALANCE:
        case Opcode::ORIGIN:
        case Opcode::CALLER:
        case Opcode::CALLVALUE:
        case Opcode::CALLDATALOAD:
        case Opcode::CALLDATASIZE:
        case Opcode::CALLDATACOPY:
        case Opcode::CODESIZE:
        case Opcode::CODECOPY:
        case Opcode::GASPRICE:
        case Opcode::EXTCODESIZE:
        case Opcode::EXTCODECOPY:
        case Opcode::RETURNDATASIZE:
        case Opcode::RETURNDATACOPY:
        case Opcode::EXTCODEHASH:
        case Opcode::BLOCKHASH:
        case Opcode::COINBASE:
        case Opcode::TIMESTAMP:
        case Opcode::NUMBER:
        case Opcode::DIFFICULTY:
        case Opcode::GASLIMIT:
        case Opcode::LOG0:
        case Opcode::LOG1:
        case Opcode::LOG2:
        case Opcode::LOG3:
        case Opcode::LOG4:
        case Opcode::CREATE:
        case Opcode::CALL:
        case Opcode::CALLCODE:
        case Opcode::RETURN:
        case Opcode::DELEGATECALL:
        case Opcode::CREATE2:
        case Opcode::STATICCALL:
        case Opcode::REVERT:
        case Opcode::INVALID:
        case Opcode::SELFDESTRUCT:
            return true;
        default:
            return false;
    }
}

std::string VirtualMachine::getStatistics() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"total_executions\": " << total_executions_ << ",\n";
    ss << "  \"total_gas_used\": " << total_gas_used_ << ",\n";
    ss << "  \"total_instructions_executed\": " << total_instructions_executed_ << ",\n";
    ss << "  \"average_gas_per_execution\": " << (total_executions_ > 0 ? total_gas_used_ / total_executions_ : 0) << ",\n";
    ss << "  \"contracts_deployed\": " << contract_storage_.size() << "\n";
    ss << "}";
    return ss.str();
}

} // namespace vm
} // namespace deo