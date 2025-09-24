/**
 * @file uint256_safe.cpp
 * @brief Safe 256-bit unsigned integer implementation
 * @author Deo Blockchain Team
 * @version 2.0.0
 */

#include "vm/uint256.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cassert>

namespace deo {
namespace vm {

// Safe constructor with bounds checking
uint256_t::uint256_t(uint64_t value) : data_{0} {
    data_[0] = value;
}

uint256_t::uint256_t(const std::string& hex_string) : data_{0} {
    if (hex_string.empty()) return;
    
    // Remove 0x prefix if present
    std::string hex = hex_string;
    if (hex.length() >= 2 && hex.substr(0, 2) == "0x") {
        hex = hex.substr(2);
    }
    
    // Validate hex string
    if (hex.length() > 64) {
        throw std::invalid_argument("Hex string too long for uint256_t");
    }
    
    // Pad with zeros if necessary
    if (hex.length() < 64) {
        hex = std::string(64 - hex.length(), '0') + hex;
    }
    
    // Parse hex string safely
    for (size_t i = 0; i < 64 && i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        if (byte_str.length() != 2) {
            throw std::invalid_argument("Invalid hex string format");
        }
        
        try {
            uint8_t byte_val = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
            
            int word_idx = (63 - i) / 16;  // Which 64-bit word
            int byte_idx = ((63 - i) % 16) / 2;  // Which byte in the word
            
            if (word_idx >= 0 && word_idx < 4 && byte_idx >= 0 && byte_idx < 8) {
                data_[word_idx] |= (static_cast<uint64_t>(byte_val) << (byte_idx * 8));
            }
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid hex character in string");
        }
    }
}

uint256_t::uint256_t(const std::array<uint8_t, 32>& bytes) : data_{0} {
    for (int i = 0; i < 32; ++i) {
        int word_idx = i / 8;
        int byte_idx = i % 8;
        data_[word_idx] |= (static_cast<uint64_t>(bytes[i]) << (byte_idx * 8));
    }
}

// Safe addition with overflow checking
uint256_t uint256_t::operator+(const uint256_t& other) const {
    uint256_t result;
    uint64_t carry = 0;
    
    for (int i = 0; i < 4; ++i) {
        uint64_t sum = data_[i] + other.data_[i] + carry;
        result.data_[i] = sum;
        carry = (sum < data_[i]) ? 1 : 0;  // Overflow detection
    }
    
    if (carry != 0) {
        throw std::overflow_error("Addition overflow in uint256_t");
    }
    
    return result;
}

// Safe subtraction with underflow checking
uint256_t uint256_t::operator-(const uint256_t& other) const {
    if (*this < other) {
        throw std::underflow_error("Subtraction underflow in uint256_t");
    }
    
    uint256_t result;
    uint64_t borrow = 0;
    
    for (int i = 0; i < 4; ++i) {
        uint64_t diff = data_[i] - other.data_[i] - borrow;
        result.data_[i] = diff;
        borrow = (diff > data_[i]) ? 1 : 0;  // Underflow detection
    }
    
    return result;
}

// Safe multiplication with overflow checking
uint256_t uint256_t::operator*(const uint256_t& other) const {
    uint256_t result(0);
    
    for (int i = 0; i < 4; ++i) {
        if (data_[i] == 0) continue;
        
        for (int j = 0; j < 4; ++j) {
            if (other.data_[j] == 0) continue;
            
            // Check for overflow before multiplication
            if (i + j >= 4) {
                throw std::overflow_error("Multiplication overflow in uint256_t");
            }
            
            // Use 128-bit intermediate result to detect overflow
            __uint128_t product = static_cast<__uint128_t>(data_[i]) * static_cast<__uint128_t>(other.data_[j]);
            
            if (product > UINT64_MAX) {
                throw std::overflow_error("Multiplication overflow in uint256_t");
            }
            
            uint64_t product_low = static_cast<uint64_t>(product);
            uint64_t product_high = static_cast<uint64_t>(product >> 64);
            
            if (product_high != 0) {
                throw std::overflow_error("Multiplication overflow in uint256_t");
            }
            
            // Add to result with carry propagation
            uint256_t temp_result = result;
            uint64_t carry = product_low;
            
            for (int k = i + j; k < 4 && carry != 0; ++k) {
                uint64_t sum = temp_result.data_[k] + carry;
                temp_result.data_[k] = sum;
                carry = (sum < temp_result.data_[k]) ? 1 : 0;
            }
            
            if (carry != 0) {
                throw std::overflow_error("Multiplication overflow in uint256_t");
            }
            
            result = temp_result;
        }
    }
    
    return result;
}

// Safe division with proper error handling
uint256_t uint256_t::operator/(const uint256_t& other) const {
    if (other.isZero()) {
        throw std::runtime_error("Division by zero");
    }
    
    if (*this < other) {
        return uint256_t(0);
    }
    
    if (other == uint256_t(1)) {
        return *this;
    }
    
    // Use binary search for division to avoid infinite loops
    uint256_t low(0);
    uint256_t high = *this;
    uint256_t result(0);
    int iterations = 0;
    const int max_iterations = 256;  // Prevent infinite loops
    
    while (low <= high && iterations < max_iterations) {
        uint256_t mid = low + (high - low) / uint256_t(2);
        
        try {
            uint256_t product = mid * other;
            
            if (product == *this) {
                return mid;
            } else if (product < *this) {
                result = mid;
                low = mid + uint256_t(1);
            } else {
                high = mid - uint256_t(1);
            }
        } catch (const std::overflow_error&) {
            high = mid - uint256_t(1);
        }
        
        iterations++;
    }
    
    if (iterations >= max_iterations) {
        throw std::runtime_error("Division algorithm exceeded maximum iterations");
    }
    
    return result;
}

// Safe modulo operation
uint256_t uint256_t::operator%(const uint256_t& other) const {
    if (other.isZero()) {
        throw std::runtime_error("Modulo by zero");
    }
    
    if (*this < other) {
        return *this;
    }
    
    try {
        uint256_t quotient = *this / other;
        return *this - (quotient * other);
    } catch (const std::exception& e) {
        throw std::runtime_error("Modulo operation failed: " + std::string(e.what()));
    }
}

// Safe comparison operators
bool uint256_t::operator==(const uint256_t& other) const {
    for (int i = 0; i < 4; ++i) {
        if (data_[i] != other.data_[i]) {
            return false;
        }
    }
    return true;
}

bool uint256_t::operator!=(const uint256_t& other) const {
    return !(*this == other);
}

bool uint256_t::operator<(const uint256_t& other) const {
    for (int i = 3; i >= 0; --i) {
        if (data_[i] < other.data_[i]) {
            return true;
        } else if (data_[i] > other.data_[i]) {
            return false;
        }
    }
    return false;
}

bool uint256_t::operator<=(const uint256_t& other) const {
    return *this < other || *this == other;
}

bool uint256_t::operator>(const uint256_t& other) const {
    return !(*this <= other);
}

bool uint256_t::operator>=(const uint256_t& other) const {
    return !(*this < other);
}

// Safe bitwise operations
uint256_t uint256_t::operator&(const uint256_t& other) const {
    uint256_t result;
    for (int i = 0; i < 4; ++i) {
        result.data_[i] = data_[i] & other.data_[i];
    }
    return result;
}

uint256_t uint256_t::operator|(const uint256_t& other) const {
    uint256_t result;
    for (int i = 0; i < 4; ++i) {
        result.data_[i] = data_[i] | other.data_[i];
    }
    return result;
}

uint256_t uint256_t::operator^(const uint256_t& other) const {
    uint256_t result;
    for (int i = 0; i < 4; ++i) {
        result.data_[i] = data_[i] ^ other.data_[i];
    }
    return result;
}

uint256_t uint256_t::operator~() const {
    uint256_t result;
    for (int i = 0; i < 4; ++i) {
        result.data_[i] = ~data_[i];
    }
    return result;
}

// Safe shift operations
uint256_t uint256_t::operator<<(int shift) const {
    if (shift < 0) {
        throw std::invalid_argument("Negative shift amount");
    }
    if (shift >= 256) {
        return uint256_t(0);
    }
    
    uint256_t result;
    int word_shift = shift / 64;
    int bit_shift = shift % 64;
    
    for (int i = 0; i < 4; ++i) {
        if (i >= word_shift) {
            result.data_[i - word_shift] |= (data_[i] << bit_shift);
            if (bit_shift > 0 && i - word_shift + 1 < 4) {
                result.data_[i - word_shift + 1] |= (data_[i] >> (64 - bit_shift));
            }
        }
    }
    
    return result;
}

uint256_t uint256_t::operator>>(int shift) const {
    if (shift < 0) {
        throw std::invalid_argument("Negative shift amount");
    }
    if (shift >= 256) {
        return uint256_t(0);
    }
    
    uint256_t result;
    int word_shift = shift / 64;
    int bit_shift = shift % 64;
    
    for (int i = 0; i < 4; ++i) {
        if (i + word_shift < 4) {
            result.data_[i] |= (data_[i + word_shift] >> bit_shift);
            if (bit_shift > 0 && i + word_shift + 1 < 4) {
                result.data_[i] |= (data_[i + word_shift + 1] << (64 - bit_shift));
            }
        }
    }
    
    return result;
}

// Safe utility functions
bool uint256_t::isZero() const {
    for (int i = 0; i < 4; ++i) {
        if (data_[i] != 0) {
            return false;
        }
    }
    return true;
}

uint64_t uint256_t::toUint64() const {
    // Check if value fits in uint64_t
    for (int i = 1; i < 4; ++i) {
        if (data_[i] != 0) {
            throw std::overflow_error("Value too large for uint64_t");
        }
    }
    return data_[0];
}

std::string uint256_t::toString() const {
    std::stringstream ss;
    ss << "0x";
    ss << std::hex << std::setfill('0');
    
    for (int i = 3; i >= 0; --i) {
        ss << std::setw(16) << data_[i];
    }
    
    return ss.str();
}

std::string uint256_t::toDecimalString() const {
    if (isZero()) {
        return "0";
    }
    
    std::string result = "0";
    uint256_t temp = *this;
    
    while (!temp.isZero()) {
        uint256_t remainder = temp % uint256_t(10);
        result = std::to_string(remainder.toUint64()) + result;
        temp = temp / uint256_t(10);
    }
    
    return result;
}

} // namespace vm
} // namespace deo
