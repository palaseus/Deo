/**
 * @file uint256.cpp
 * @brief 256-bit unsigned integer implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "vm/uint256.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace deo {
namespace vm {

uint256_t::uint256_t(const std::string& hex_string) : data_{0} {
    if (hex_string.empty()) return;
    
    // Remove 0x prefix if present
    std::string hex = hex_string;
    if (hex.length() >= 2 && hex.substr(0, 2) == "0x") {
        hex = hex.substr(2);
    }
    
    // Pad with zeros if necessary
    if (hex.length() < 64) {
        hex = std::string(64 - hex.length(), '0') + hex;
    }
    
    // Parse hex string
    for (size_t i = 0; i < 64 && i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        uint8_t byte_val = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        
        int word_idx = (63 - i) / 16;  // Which 64-bit word
        int byte_idx = ((63 - i) % 16) / 2;  // Which byte in the word
        
        data_[word_idx] |= (static_cast<uint64_t>(byte_val) << (byte_idx * 8));
    }
}

uint256_t::uint256_t(const std::array<uint8_t, 32>& bytes) : data_{0} {
    for (int i = 0; i < 32; ++i) {
        int word_idx = i / 8;
        int byte_idx = i % 8;
        data_[word_idx] |= (static_cast<uint64_t>(bytes[i]) << (byte_idx * 8));
    }
}

uint256_t uint256_t::operator+(const uint256_t& other) const {
    uint256_t result;
    uint64_t carry = 0;
    
    for (int i = 0; i < 4; ++i) {
        auto [sum, new_carry] = addWithCarry(data_[i], other.data_[i], carry);
        result.data_[i] = sum;
        carry = new_carry;
    }
    
    return result;
}

uint256_t uint256_t::operator-(const uint256_t& other) const {
    uint256_t result;
    uint64_t borrow = 0;
    
    for (int i = 0; i < 4; ++i) {
        uint64_t a = data_[i];
        uint64_t b = other.data_[i] + borrow;
        
        if (a >= b) {
            result.data_[i] = a - b;
            borrow = 0;
        } else {
            result.data_[i] = a - b;  // This will wrap around
            borrow = 1;
        }
    }
    
    return result;
}

uint256_t uint256_t::operator*(const uint256_t& other) const {
    uint256_t result;
    
    for (int i = 0; i < 4; ++i) {
        uint64_t carry = 0;
        for (int j = 0; j < 4; ++j) {
            if (i + j < 4) {
                auto [product, new_carry] = multiplyWithCarry(data_[i], other.data_[j], carry);
                result.data_[i + j] += product;
                carry = new_carry;
            }
        }
    }
    
    return result;
}

uint256_t uint256_t::operator/(const uint256_t& other) const {
    if (other.isZero()) {
        throw std::runtime_error("Division by zero");
    }
    
    // Handle simple cases
    if (*this < other) {
        return uint256_t(0);
    }
    
    if (other == uint256_t(1)) {
        return *this;
    }
    
    // For small values, use simple division
    if (data_[1] == 0 && data_[2] == 0 && data_[3] == 0 &&
        other.data_[1] == 0 && other.data_[2] == 0 && other.data_[3] == 0) {
        return uint256_t(data_[0] / other.data_[0]);
    }
    
    // For larger values, use binary search
    uint256_t low(0);
    uint256_t high = *this;
    uint256_t result(0);
    
    while (low <= high) {
        uint256_t mid = low + (high - low) / uint256_t(2);
        uint256_t product = mid * other;
        
        if (product == *this) {
            return mid;
        } else if (product < *this) {
            result = mid;
            low = mid + uint256_t(1);
        } else {
            high = mid - uint256_t(1);
        }
    }
    
    return result;
}

uint256_t uint256_t::operator%(const uint256_t& other) const {
    if (other.isZero()) {
        throw std::runtime_error("Modulo by zero");
    }
    
    uint256_t remainder = *this;
    
    // Simple long division algorithm
    for (int i = 255; i >= 0; --i) {
        remainder = remainder << 1;
        if (remainder >= other) {
            remainder = remainder - other;
        }
    }
    
    return remainder;
}

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

uint256_t uint256_t::operator<<(int shift) const {
    if (shift <= 0) return *this;
    if (shift >= 256) return uint256_t(0);
    
    uint256_t result;
    int word_shift = shift / 64;
    int bit_shift = shift % 64;
    
    for (int i = 0; i < 4; ++i) {
        if (i + word_shift < 4) {
            result.data_[i + word_shift] |= (data_[i] << bit_shift);
        }
        if (bit_shift > 0 && i + word_shift + 1 < 4) {
            result.data_[i + word_shift + 1] |= (data_[i] >> (64 - bit_shift));
        }
    }
    
    return result;
}

uint256_t uint256_t::operator>>(int shift) const {
    if (shift <= 0) return *this;
    if (shift >= 256) return uint256_t(0);
    
    uint256_t result;
    int word_shift = shift / 64;
    int bit_shift = shift % 64;
    
    for (int i = 0; i < 4; ++i) {
        if (i >= word_shift) {
            result.data_[i - word_shift] |= (data_[i] >> bit_shift);
        }
        if (bit_shift > 0 && i >= word_shift + 1) {
            result.data_[i - word_shift - 1] |= (data_[i] << (64 - bit_shift));
        }
    }
    
    return result;
}

bool uint256_t::operator<(const uint256_t& other) const {
    for (int i = 3; i >= 0; --i) {
        if (data_[i] < other.data_[i]) return true;
        if (data_[i] > other.data_[i]) return false;
    }
    return false;
}

bool uint256_t::operator>(const uint256_t& other) const {
    for (int i = 3; i >= 0; --i) {
        if (data_[i] > other.data_[i]) return true;
        if (data_[i] < other.data_[i]) return false;
    }
    return false;
}

bool uint256_t::operator<=(const uint256_t& other) const {
    return !(*this > other);
}

bool uint256_t::operator>=(const uint256_t& other) const {
    return !(*this < other);
}

bool uint256_t::operator==(const uint256_t& other) const {
    for (int i = 0; i < 4; ++i) {
        if (data_[i] != other.data_[i]) return false;
    }
    return true;
}

bool uint256_t::operator!=(const uint256_t& other) const {
    return !(*this == other);
}

bool uint256_t::isZero() const {
    for (int i = 0; i < 4; ++i) {
        if (data_[i] != 0) return false;
    }
    return true;
}

uint64_t uint256_t::toUint64() const {
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

std::array<uint8_t, 32> uint256_t::toBytes() const {
    std::array<uint8_t, 32> bytes{};
    for (int i = 0; i < 32; ++i) {
        int word_idx = i / 8;
        int byte_idx = i % 8;
        bytes[i] = static_cast<uint8_t>((data_[word_idx] >> (byte_idx * 8)) & 0xFF);
    }
    return bytes;
}

uint8_t uint256_t::getByte(int pos) const {
    if (pos < 0 || pos >= 32) return 0;
    int word_idx = pos / 8;
    int byte_idx = pos % 8;
    return static_cast<uint8_t>((data_[word_idx] >> (byte_idx * 8)) & 0xFF);
}

void uint256_t::setByte(int pos, uint8_t value) {
    if (pos < 0 || pos >= 32) return;
    int word_idx = pos / 8;
    int byte_idx = pos % 8;
    data_[word_idx] &= ~(0xFFULL << (byte_idx * 8));
    data_[word_idx] |= (static_cast<uint64_t>(value) << (byte_idx * 8));
}

std::pair<uint64_t, uint64_t> uint256_t::addWithCarry(uint64_t a, uint64_t b, uint64_t carry) const {
    uint64_t sum = a + b + carry;
    uint64_t new_carry = (sum < a) ? 1 : 0;
    return {sum, new_carry};
}

std::pair<uint64_t, uint64_t> uint256_t::multiplyWithCarry(uint64_t a, uint64_t b, uint64_t carry) const {
    __uint128_t product = static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b) + carry;
    return {static_cast<uint64_t>(product), static_cast<uint64_t>(product >> 64)};
}

} // namespace vm
} // namespace deo
