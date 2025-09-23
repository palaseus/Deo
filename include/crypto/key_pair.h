/**
 * @file key_pair.h
 * @brief Key pair management for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

#include "signature.h"

namespace deo {
namespace crypto {

/**
 * @brief Key pair management class
 * 
 * Manages public/private key pairs for the blockchain, providing
 * secure key generation, storage, and cryptographic operations.
 */
class KeyPair {
public:
    /**
     * @brief Default constructor - generates new key pair
     */
    KeyPair();
    
    /**
     * @brief Constructor from existing private key
     * @param private_key Existing private key (hex string)
     */
    explicit KeyPair(const std::string& private_key);
    
    /**
     * @brief Destructor - clears sensitive data
     */
    ~KeyPair();
    
    // Disable copy and move semantics for security
    KeyPair(const KeyPair&) = delete;
    KeyPair& operator=(const KeyPair&) = delete;
    KeyPair(KeyPair&&) = delete;
    KeyPair& operator=(KeyPair&&) = delete;
    
    /**
     * @brief Sign data with private key
     * @param data Data to sign
     * @return Signature as hexadecimal string
     */
    std::string sign(const std::string& data) const;
    
    /**
     * @brief Sign data with private key (binary output)
     * @param data Data to sign
     * @return Signature as binary data
     */
    std::vector<uint8_t> signBinary(const std::string& data) const;
    
    /**
     * @brief Verify signature with public key
     * @param data Original data
     * @param signature Signature to verify
     * @return True if signature is valid
     */
    bool verify(const std::string& data, const std::string& signature) const;
    
    /**
     * @brief Verify signature with public key (binary input)
     * @param data Original data
     * @param signature Signature to verify (binary)
     * @return True if signature is valid
     */
    bool verifyBinary(const std::string& data, const std::vector<uint8_t>& signature) const;
    
    /**
     * @brief Export keys to encrypted format
     * @param password Encryption password
     * @return Encrypted key data as JSON
     */
    nlohmann::json exportKeys(const std::string& password) const;
    
    /**
     * @brief Import keys from encrypted format
     * @param encrypted_data Encrypted key data
     * @param password Decryption password
     * @return True if successful
     */
    bool importKeys(const nlohmann::json& encrypted_data, const std::string& password);
    
    /**
     * @brief Check if key pair is valid
     * @return True if key pair is valid
     */
    bool isValid() const;
    
    /**
     * @brief Get public key
     * @return Public key as hexadecimal string
     */
    const std::string& getPublicKey() const { return public_key_; }
    
    /**
     * @brief Get private key (use with caution)
     * @return Private key as hexadecimal string
     */
    const std::string& getPrivateKey() const { return private_key_; }
    
    /**
     * @brief Get Bitcoin address
     * @return Bitcoin address (Base58Check encoded)
     */
    const std::string& getAddress() const { return address_; }
    
    /**
     * @brief Get key pair information
     * @return JSON string with key information
     */
    std::string getInfo() const;
    
    /**
     * @brief Generate new key pair
     * @return True if successful
     */
    bool generateKeyPair();
    
    /**
     * @brief Derive public key from private key
     * @return True if successful
     */
    bool derivePublicKey();
    
    /**
     * @brief Derive address from public key
     * @return True if successful
     */
    bool deriveAddress();
    
    /**
     * @brief Encrypt data with public key
     * @param data Data to encrypt
     * @return Encrypted data
     */
    std::string encrypt(const std::string& data) const;
    
    /**
     * @brief Decrypt data with private key
     * @param encrypted_data Encrypted data
     * @return Decrypted data
     */
    std::string decrypt(const std::string& encrypted_data) const;
    
    /**
     * @brief Get compressed public key
     * @return Compressed public key
     */
    std::string getCompressedPublicKey() const;
    
    /**
     * @brief Get uncompressed public key
     * @return Uncompressed public key
     */
    std::string getUncompressedPublicKey() const;
    
    /**
     * @brief Check if public key is compressed
     * @return True if compressed
     */
    bool isCompressed() const;

private:
    std::string private_key_;    ///< Private key (hex string)
    std::string public_key_;     ///< Public key (hex string)
    std::string address_;        ///< Bitcoin address
    
    /**
     * @brief Clear sensitive data from memory
     */
    void clearSensitiveData();
    
    /**
     * @brief Validate key pair consistency
     * @return True if keys are consistent
     */
    bool validateKeyPair() const;
    
    /**
     * @brief Encrypt string with password
     * @param data Data to encrypt
     * @param password Encryption password
     * @return Encrypted data
     */
    std::string encryptString(const std::string& data, const std::string& password) const;
    
    /**
     * @brief Decrypt string with password
     * @param encrypted_data Encrypted data
     * @param password Decryption password
     * @return Decrypted data
     */
    std::string decryptString(const std::string& encrypted_data, const std::string& password) const;
};

} // namespace crypto
} // namespace deo