#include <iostream>
#include <vector>
#include "vm/virtual_machine.h"

int main() {
    std::cout << "=== Direct VM Test ===" << std::endl;
    
    deo::vm::VirtualMachine vm;
    
    // Test with just PUSH1 5
    std::vector<uint8_t> code = {0x60, 0x05};
    
    std::cout << "Bytecode: ";
    for (auto byte : code) {
        std::cout << "0x" << std::hex << (int)byte << " ";
    }
    std::cout << std::endl;
    
    deo::vm::ExecutionContext context;
    context.code = code;
    context.gas_limit = 100000;
    context.contract_address = "0x1234567890123456789012345678901234567890";
    context.caller_address = "0x0987654321098765432109876543210987654321";
    context.value = 0;
    context.gas_price = 20;
    context.block_number = 1;
    context.block_timestamp = 1234567890;
    context.block_coinbase = "0x0000000000000000000000000000000000000000";
    
    std::cout << "Executing VM..." << std::endl;
    
    deo::vm::ExecutionResult result = vm.execute(context);
    
    std::cout << "Result: " << (result.success ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Gas used: " << result.gas_used << std::endl;
    if (!result.success) {
        std::cout << "Error: " << result.error_message << std::endl;
    }
    
    return 0;
}
