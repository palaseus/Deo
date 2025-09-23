/**
 * @file signature.h
 * @brief Digital signature functions for the Deo Blockchain
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
 * @brief Digital signature functions using OpenSSL ECDSA
 * 
 * Provides ECDSA signature generation and verification using the secp256k1 curve,
 * which is the same curve used by Bitcoin.
 */
class Signature {
public:
    /**
     * @brief Sign data with a private key
     * @param data Data to sign
     * @param private_key Private key (hex string)
     * @return Signature as hexadecimal string
     */
    static std::string sign(const std::string& data, const std::string& private_key);
    
    /**
     * @brief Sign data with a private key (binary output)
     * @param data Data to sign
     * @param private_key Private key (hex string)
     * @return Signature as binary data
     */
    static std::vector<uint8_t> signBinary(const std::string& data, const std::string& private_key);
    
    /**
     * @brief Verify signature
     * @param data Original data
     * @param signature Signature to verify
     * @param public_key Public key (hex string)
     * @return True if signature is valid
     */
    static bool verify(const std::string& data, const std::string& signature, const std::string& public_key);
    
    /**
     * @brief Verify signature (binary input)
     * @param data Original data
     * @param signature Signature to verify (binary)
     * @param public_key Public key (hex string)
     * @return True if signature is valid
     */
    static bool verifyBinary(const std::string& data, const std::vector<uint8_t>& signature, const std::string& public_key);
    
    /**
     * @brief Derive public key from private key
     * @param private_key Private key (hex string)
     * @return Public key as hexadecimal string
     */
    static std::string getPublicKey(const std::string& private_key);
    
    /**
     * @brief Generate a new key pair
     * @param private_key Output private key (hex string)
     * @param public_key Output public key (hex string)
     * @return True if successful
     */
    static bool generateKeyPair(std::string& private_key, std::string& public_key);
    
    /**
     * @brief Validate private key format
     * @param private_key Private key to validate
     * @return True if valid
     */
    static bool isValidPrivateKey(const std::string& private_key);
    
    /**
     * @brief Validate public key format
     * @param public_key Public key to validate
     * @return True if valid
     */
    static bool isValidPublicKey(const std::string& public_key);
    
    /**
     * @brief Convert public key to Bitcoin address
     * @param public_key Public key (hex string)
     * @return Bitcoin address (Base58Check encoded)
     */
    static std::string publicKeyToAddress(const std::string& public_key);
    
    /**
     * @brief Convert public key to compressed format
     * @param public_key Uncompressed public key (hex string)
     * @return Compressed public key (hex string)
     */
    static std::string compressPublicKey(const std::string& public_key);
    
    /**
     * @brief Convert public key to uncompressed format
     * @param public_key Compressed public key (hex string)
     * @return Uncompressed public key (hex string)
     */
    static std::string uncompressPublicKey(const std::string& public_key);
    
    /**
     * @brief Check if public key is compressed
     * @param public_key Public key to check
     * @return True if compressed
     */
    static bool isCompressedPublicKey(const std::string& public_key);
    
    /**
     * @brief Recover public key from signature and message
     * @param data Original data
     * @param signature Signature
     * @param recovery_id Recovery ID (0 or 1)
     * @return Recovered public key or empty string if failed
     */
    static std::string recoverPublicKey(const std::string& data, const std::string& signature, int recovery_id);
    
    /**
     * @brief Get signature recovery ID
     * @param data Original data
     * @param signature Signature
     * @param public_key Public key
     * @return Recovery ID (0 or 1) or -1 if failed
     */
    static int getRecoveryId(const std::string& data, const std::string& signature, const std::string& public_key);

private:
    /**
     * @brief Initialize OpenSSL if not already initialized
     */
    static void initializeOpenSSL();
    
    /**
     * @brief Get secp256k1 curve
     * @return EC_GROUP pointer or nullptr if failed
     */
    static void* getSecp256k1Group();
    
    /**
     * @brief Convert hex string to EC_KEY
     * @param private_key_hex Private key as hex string
     * @return EC_KEY pointer or nullptr if failed
     */
    static void* createECKeyFromPrivateKey(const std::string& private_key_hex);
    
    /**
     * @brief Convert EC_KEY to hex string
     * @param ec_key EC_KEY pointer
     * @param compressed Whether to use compressed format
     * @return Public key as hex string
     */
    static std::string ecKeyToHexString(void* ec_key, bool compressed = true);
    
    /**
     * @brief Perform ECDSA signing
     * @param data Data to sign
     * @param private_key_bytes Private key as bytes
     * @return Signature as binary data
     */
    static std::vector<uint8_t> ecdsaSign(const std::vector<uint8_t>& data, const std::vector<uint8_t>& private_key_bytes);
    
    /**
     * @brief Perform ECDSA verification
     * @param data Original data
     * @param signature Signature to verify
     * @param public_key_bytes Public key as bytes
     * @return True if signature is valid
     */
    static bool ecdsaVerify(const std::vector<uint8_t>& data, 
                           const std::vector<uint8_t>& signature, 
                           const std::vector<uint8_t>& public_key_bytes);
    
    /**
     * @brief Convert hex string to bytes
     * @param hex Hex string
     * @return Binary data
     */
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
    
    /**
     * @brief Convert bytes to hex string
     * @param bytes Binary data
     * @return Hex string
     */
    static std::string bytesToHex(const std::vector<uint8_t>& bytes);
};

} // namespace crypto
} // namespace deo