/**
 * @file hash.h
 * @brief Cryptographic hash functions for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace deo {
namespace crypto {

/**
 * @brief Cryptographic hash functions using OpenSSL
 * 
 * Provides SHA-256 and other cryptographic hash functions
 * for the blockchain implementation.
 */
class Hash {
public:
    /**
     * @brief Compute SHA-256 hash of input string
     * @param input Input string to hash
     * @return SHA-256 hash as hexadecimal string
     */
    static std::string sha256(const std::string& input);
    
    /**
     * @brief Compute SHA-256 hash of binary data
     * @param data Binary data to hash
     * @return SHA-256 hash as hexadecimal string
     */
    static std::string sha256(const std::vector<uint8_t>& data);
    
    /**
     * @brief Compute SHA-256 hash of input string (binary output)
     * @param input Input string to hash
     * @return SHA-256 hash as binary data
     */
    static std::vector<uint8_t> sha256Binary(const std::string& input);
    
    /**
     * @brief Compute SHA-256 hash of binary data (binary output)
     * @param data Binary data to hash
     * @return SHA-256 hash as binary data
     */
    static std::vector<uint8_t> sha256Binary(const std::vector<uint8_t>& data);
    
    /**
     * @brief Compute double SHA-256 hash (SHA-256 of SHA-256)
     * @param input Input string to hash
     * @return Double SHA-256 hash as hexadecimal string
     */
    static std::string doubleSha256(const std::string& input);
    
    /**
     * @brief Compute double SHA-256 hash (SHA-256 of SHA-256)
     * @param data Binary data to hash
     * @return Double SHA-256 hash as hexadecimal string
     */
    static std::string doubleSha256(const std::vector<uint8_t>& data);
    
    /**
     * @brief Compute RIPEMD-160 hash
     * @param input Input string to hash
     * @return RIPEMD-160 hash as hexadecimal string
     */
    static std::string ripemd160(const std::string& input);
    
    /**
     * @brief Compute RIPEMD-160 hash
     * @param data Binary data to hash
     * @return RIPEMD-160 hash as hexadecimal string
     */
    static std::string ripemd160(const std::vector<uint8_t>& data);
    
    /**
     * @brief Compute SHA-256 followed by RIPEMD-160 (Bitcoin address hash)
     * @param input Input string to hash
     * @return Hash160 hash as hexadecimal string
     */
    static std::string hash160(const std::string& input);
    
    /**
     * @brief Compute SHA-256 followed by RIPEMD-160 (Bitcoin address hash)
     * @param data Binary data to hash
     * @return Hash160 hash as hexadecimal string
     */
    static std::string hash160(const std::vector<uint8_t>& data);
    
    /**
     * @brief Convert hexadecimal string to binary data
     * @param hex Hexadecimal string
     * @return Binary data
     */
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
    
    /**
     * @brief Convert binary data to hexadecimal string
     * @param data Binary data
     * @return Hexadecimal string
     */
    static std::string bytesToHex(const std::vector<uint8_t>& data);
    
    /**
     * @brief Convert binary data to hexadecimal string
     * @param data Binary data pointer
     * @param length Data length
     * @return Hexadecimal string
     */
    static std::string bytesToHex(const uint8_t* data, size_t length);
    
    /**
     * @brief Verify hash format (64 hex characters for SHA-256)
     * @param hash Hash string to verify
     * @return True if hash format is valid
     */
    static bool isValidHash(const std::string& hash);
    
    /**
     * @brief Generate random bytes
     * @param length Number of bytes to generate
     * @return Random bytes
     */
    static std::vector<uint8_t> randomBytes(size_t length);
    
    /**
     * @brief Compute HMAC-SHA256
     * @param key HMAC key
     * @param data Data to authenticate
     * @return HMAC-SHA256 as hexadecimal string
     */
    static std::string hmacSha256(const std::string& key, const std::string& data);
    
    /**
     * @brief Compute HMAC-SHA256
     * @param key HMAC key
     * @param data Data to authenticate
     * @return HMAC-SHA256 as binary data
     */
    static std::vector<uint8_t> hmacSha256Binary(const std::string& key, const std::string& data);

private:
    /**
     * @brief Initialize OpenSSL if not already initialized
     */
    static void initializeOpenSSL();
    
    /**
     * @brief Check if OpenSSL is initialized
     * @return True if initialized
     */
    static bool isOpenSSLInitialized();
};

} // namespace crypto
} // namespace deo