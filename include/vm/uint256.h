/**
 * @file uint256.h
 * @brief 256-bit unsigned integer implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#ifndef DEO_VM_UINT256_H
#define DEO_VM_UINT256_H

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace deo {
namespace vm {

/**
 * @brief 256-bit unsigned integer class
 */
class uint256_t {
public:
    /**
     * @brief Default constructor (zero)
     */
    uint256_t() : data_{0} {}
    
    /**
     * @brief Constructor from 64-bit unsigned integer
     * @param value 64-bit value
     */
    explicit uint256_t(uint64_t value) : data_{0} {
        data_[0] = value;
    }
    
    /**
     * @brief Constructor from string (hex)
     * @param hex_string Hex string representation
     */
    explicit uint256_t(const std::string& hex_string);
    
    /**
     * @brief Constructor from byte array
     * @param bytes Byte array (32 bytes)
     */
    explicit uint256_t(const std::array<uint8_t, 32>& bytes);
    
    /**
     * @brief Copy constructor
     * @param other Other uint256_t
     */
    uint256_t(const uint256_t& other) = default;
    
    /**
     * @brief Assignment operator
     * @param other Other uint256_t
     * @return Reference to this
     */
    uint256_t& operator=(const uint256_t& other) = default;
    
    /**
     * @brief Addition operator
     * @param other Other uint256_t
     * @return Result of addition
     */
    uint256_t operator+(const uint256_t& other) const;
    
    /**
     * @brief Subtraction operator
     * @param other Other uint256_t
     * @return Result of subtraction
     */
    uint256_t operator-(const uint256_t& other) const;
    
    /**
     * @brief Multiplication operator
     * @param other Other uint256_t
     * @return Result of multiplication
     */
    uint256_t operator*(const uint256_t& other) const;
    
    /**
     * @brief Division operator
     * @param other Other uint256_t
     * @return Result of division
     */
    uint256_t operator/(const uint256_t& other) const;
    
    /**
     * @brief Modulo operator
     * @param other Other uint256_t
     * @return Result of modulo
     */
    uint256_t operator%(const uint256_t& other) const;
    
    /**
     * @brief Bitwise AND operator
     * @param other Other uint256_t
     * @return Result of bitwise AND
     */
    uint256_t operator&(const uint256_t& other) const;
    
    /**
     * @brief Bitwise OR operator
     * @param other Other uint256_t
     * @return Result of bitwise OR
     */
    uint256_t operator|(const uint256_t& other) const;
    
    /**
     * @brief Bitwise XOR operator
     * @param other Other uint256_t
     * @return Result of bitwise XOR
     */
    uint256_t operator^(const uint256_t& other) const;
    
    /**
     * @brief Bitwise NOT operator
     * @return Result of bitwise NOT
     */
    uint256_t operator~() const;
    
    /**
     * @brief Left shift operator
     * @param shift Number of bits to shift
     * @return Result of left shift
     */
    uint256_t operator<<(int shift) const;
    
    /**
     * @brief Right shift operator
     * @param shift Number of bits to shift
     * @return Result of right shift
     */
    uint256_t operator>>(int shift) const;
    
    /**
     * @brief Less than operator
     * @param other Other uint256_t
     * @return True if this < other
     */
    bool operator<(const uint256_t& other) const;
    
    /**
     * @brief Greater than operator
     * @param other Other uint256_t
     * @return True if this > other
     */
    bool operator>(const uint256_t& other) const;
    
    /**
     * @brief Less than or equal operator
     * @param other Other uint256_t
     * @return True if this <= other
     */
    bool operator<=(const uint256_t& other) const;
    
    /**
     * @brief Greater than or equal operator
     * @param other Other uint256_t
     * @return True if this >= other
     */
    bool operator>=(const uint256_t& other) const;
    
    /**
     * @brief Equality operator
     * @param other Other uint256_t
     * @return True if this == other
     */
    bool operator==(const uint256_t& other) const;
    
    /**
     * @brief Inequality operator
     * @param other Other uint256_t
     * @return True if this != other
     */
    bool operator!=(const uint256_t& other) const;
    
    /**
     * @brief Check if zero
     * @return True if zero
     */
    bool isZero() const;
    
    /**
     * @brief Get as 64-bit unsigned integer (truncated)
     * @return 64-bit value
     */
    uint64_t toUint64() const;
    
    /**
     * @brief Get as string (hex representation)
     * @return Hex string
     */
    std::string toString() const;
    
    /**
     * @brief Get as byte array
     * @return Byte array (32 bytes)
     */
    std::array<uint8_t, 32> toBytes() const;
    
    /**
     * @brief Get byte at position
     * @param pos Byte position (0-31)
     * @return Byte value
     */
    uint8_t getByte(int pos) const;
    
    /**
     * @brief Set byte at position
     * @param pos Byte position (0-31)
     * @param value Byte value
     */
    void setByte(int pos, uint8_t value);

private:
    std::array<uint64_t, 4> data_; ///< 4 x 64-bit words
    
    /**
     * @brief Add with carry
     * @param a First operand
     * @param b Second operand
     * @param carry Input carry
     * @return Result and output carry
     */
    std::pair<uint64_t, uint64_t> addWithCarry(uint64_t a, uint64_t b, uint64_t carry) const;
    
    /**
     * @brief Multiply with carry
     * @param a First operand
     * @param b Second operand
     * @param carry Input carry
     * @return Result and output carry
     */
    std::pair<uint64_t, uint64_t> multiplyWithCarry(uint64_t a, uint64_t b, uint64_t carry) const;
};

} // namespace vm
} // namespace deo

#endif // DEO_VM_UINT256_H
