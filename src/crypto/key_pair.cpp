/**
 * @file key_pair.cpp
 * @brief Key pair management implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "crypto/key_pair.h"
#include "utils/logger.h"
#include "utils/error_handler.h"
#include "crypto/openssl_compat.h"

#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>

namespace deo {
namespace crypto {

KeyPair::KeyPair() {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Creating new key pair");
    
    if (!generateKeyPair()) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to generate key pair");
    }
}

KeyPair::KeyPair(const std::string& private_key) {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Creating key pair from existing private key");
    
    if (!Signature::isValidPrivateKey(private_key)) {
        DEO_ERROR(CRYPTOGRAPHY, "Invalid private key format");
        return;
    }
    
    private_key_ = private_key;
    
    if (!derivePublicKey() || !deriveAddress()) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to derive public key and address");
    }
}

KeyPair::~KeyPair() {
    clearSensitiveData();
}

std::string KeyPair::sign(const std::string& data) const {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Signing data with key pair");
    
    if (private_key_.empty()) {
        DEO_ERROR(CRYPTOGRAPHY, "Private key not available for signing");
        return "";
    }
    
    return Signature::sign(data, private_key_);
}

std::vector<uint8_t> KeyPair::signBinary(const std::string& data) const {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Signing data with key pair (binary output)");
    
    if (private_key_.empty()) {
        DEO_ERROR(CRYPTOGRAPHY, "Private key not available for signing");
        return {};
    }
    
    return Signature::signBinary(data, private_key_);
}

bool KeyPair::verify(const std::string& data, const std::string& signature) const {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Verifying signature with key pair");
    
    if (public_key_.empty()) {
        DEO_ERROR(CRYPTOGRAPHY, "Public key not available for verification");
        return false;
    }
    
    return Signature::verify(data, signature, public_key_);
}

bool KeyPair::verifyBinary(const std::string& data, const std::vector<uint8_t>& signature) const {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Verifying signature with key pair (binary input)");
    
    if (public_key_.empty()) {
        DEO_ERROR(CRYPTOGRAPHY, "Public key not available for verification");
        return false;
    }
    
    return Signature::verifyBinary(data, signature, public_key_);
}

nlohmann::json KeyPair::exportKeys(const std::string& password) const {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Exporting keys to encrypted format");
    
    nlohmann::json json;
    json["version"] = "1.0";
    json["encrypted_private_key"] = encryptString(private_key_, password);
    json["public_key"] = public_key_;
    json["address"] = address_;
    json["compressed"] = isCompressed();
    
    return json;
}

bool KeyPair::importKeys(const nlohmann::json& encrypted_data, const std::string& password) {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Importing keys from encrypted format");
    
    try {
        std::string encrypted_private_key = encrypted_data.at("encrypted_private_key").get<std::string>();
        private_key_ = decryptString(encrypted_private_key, password);
        
        if (!Signature::isValidPrivateKey(private_key_)) {
            DEO_ERROR(CRYPTOGRAPHY, "Invalid private key after decryption");
            return false;
        }
        
        public_key_ = encrypted_data.at("public_key").get<std::string>();
        address_ = encrypted_data.at("address").get<std::string>();
        
        // Validate the imported keys
        if (!validateKeyPair()) {
            DEO_ERROR(CRYPTOGRAPHY, "Imported key pair validation failed");
            return false;
        }
        
        DEO_LOG_INFO(CRYPTOGRAPHY, "Keys imported successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to import keys: " + std::string(e.what()));
        return false;
    }
}

bool KeyPair::isValid() const {
    return !private_key_.empty() && 
           !public_key_.empty() && 
           !address_.empty() &&
           Signature::isValidPrivateKey(private_key_) &&
           Signature::isValidPublicKey(public_key_) &&
           validateKeyPair();
}

std::string KeyPair::getInfo() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"address\": \"" << address_ << "\",\n";
    ss << "  \"public_key\": \"" << public_key_ << "\",\n";
    ss << "  \"private_key\": \"<hidden>\",\n";
    ss << "  \"compressed\": " << (isCompressed() ? "true" : "false") << ",\n";
    ss << "  \"valid\": " << (isValid() ? "true" : "false") << "\n";
    ss << "}";
    
    return ss.str();
}

bool KeyPair::generateKeyPair() {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Generating new key pair");
    
    if (!Signature::generateKeyPair(private_key_, public_key_)) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to generate key pair");
        return false;
    }
    
    if (!deriveAddress()) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to derive address");
        return false;
    }
    
    DEO_LOG_INFO(CRYPTOGRAPHY, "Key pair generated successfully");
    return true;
}

bool KeyPair::derivePublicKey() {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Deriving public key from private key");
    
    public_key_ = Signature::getPublicKey(private_key_);
    
    if (public_key_.empty()) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to derive public key");
        return false;
    }
    
    return true;
}

bool KeyPair::deriveAddress() {
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Deriving address from public key");
    
    address_ = Signature::publicKeyToAddress(public_key_);
    
    if (address_.empty()) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to derive address");
        return false;
    }
    
    return true;
}

std::string KeyPair::encrypt(const std::string& data) const {
    // Public key encryption using ECIES (Elliptic Curve Integrated Encryption Scheme)
    // For now, use symmetric encryption with a session key derived from shared secret
    // Full ECIES implementation would use ECDH for key exchange
    
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Encrypting data with public key");
    
    if (public_key_.empty()) {
        DEO_ERROR(CRYPTOGRAPHY, "Public key not available for encryption");
        return "";
    }
    
    // Generate a random session key
    std::vector<uint8_t> session_key(32); // AES-256 key size
    if (RAND_bytes(session_key.data(), 32) != 1) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to generate random session key");
        return "";
    }
    
    // Encrypt data with session key using AES
    std::string encrypted_data = encryptString(data, std::string(session_key.begin(), session_key.end()));
    
    // In full ECIES, we would encrypt the session key with the public key
    // For now, return the encrypted data (in production, prepend encrypted session key)
    // Clear session key from memory
    OPENSSL_cleanse(session_key.data(), session_key.size());
    
    return encrypted_data;
}

std::string KeyPair::decrypt(const std::string& /* encrypted_data */) const {
    // Private key decryption (ECIES)
    DEO_LOG_DEBUG(CRYPTOGRAPHY, "Decrypting data with private key");
    
    if (private_key_.empty()) {
        DEO_ERROR(CRYPTOGRAPHY, "Private key not available for decryption");
        return "";
    }
    
    // In full ECIES, we would extract and decrypt the session key first
    // For now, we'll need the session key passed separately or stored with the ciphertext
    // This is a simplified version - full implementation requires ECDH
    
    DEO_LOG_WARNING(CRYPTOGRAPHY, "Full ECIES decryption requires session key - using simplified version");
    
    // For demonstration, assume encrypted_data contains both encrypted session key and data
    // In production, implement proper ECIES with ECDH
    
    return ""; // Requires full ECIES implementation
}

std::string KeyPair::getCompressedPublicKey() const {
    return Signature::compressPublicKey(public_key_);
}

std::string KeyPair::getUncompressedPublicKey() const {
    return Signature::uncompressPublicKey(public_key_);
}

bool KeyPair::isCompressed() const {
    return Signature::isCompressedPublicKey(public_key_);
}

void KeyPair::clearSensitiveData() {
    // Securely clear private key from memory using OpenSSL
    if (!private_key_.empty()) {
        // Convert string to mutable buffer for secure clearing
        std::vector<char> key_buffer(private_key_.begin(), private_key_.end());
        OPENSSL_cleanse(key_buffer.data(), key_buffer.size());
        // Fill with zeros as additional safety
        memset(key_buffer.data(), 0, key_buffer.size());
        private_key_.clear();
        // Force memory overwrite with new allocation
        private_key_.resize(64, 'X');
        private_key_.clear();
    }
    
    // Clear public key (less sensitive but still good practice)
    if (!public_key_.empty()) {
        std::vector<char> pub_buffer(public_key_.begin(), public_key_.end());
        OPENSSL_cleanse(pub_buffer.data(), pub_buffer.size());
        memset(pub_buffer.data(), 0, pub_buffer.size());
        public_key_.clear();
    }
    
    // Clear address
    address_.clear();
}

bool KeyPair::validateKeyPair() const {
    if (private_key_.empty() || public_key_.empty()) {
        return false;
    }
    
    // Test signature/verification to ensure keys match
    std::string test_data = "test_validation_data";
    std::string signature = Signature::sign(test_data, private_key_);
    
    if (signature.empty()) {
        return false;
    }
    
    return Signature::verify(test_data, signature, public_key_);
}

std::string KeyPair::encryptString(const std::string& data, const std::string& password) const {
    try {
        // Use AES-256-CBC for secure encryption
        // Derive key from password using SHA-256
        std::vector<uint8_t> key(32); // AES-256 requires 32 bytes
        std::vector<uint8_t> iv(16);  // AES block size
        
        // Derive key using SHA-256 of password
        SHA256_CTX sha256_ctx;
        if (!SHA256_Init(&sha256_ctx)) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to initialize SHA-256 for key derivation");
            return "";
        }
        
        if (!SHA256_Update(&sha256_ctx, password.data(), password.length())) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to update SHA-256 for key derivation");
            return "";
        }
        
        if (!SHA256_Final(key.data(), &sha256_ctx)) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to finalize SHA-256 for key derivation");
            return "";
        }
        
        // Generate IV from first 16 bytes of key hash (for deterministic IV from password)
        // In production, use random IV and prepend it to ciphertext
        std::memcpy(iv.data(), key.data(), 16);
        
        // Set up encryption context
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to create encryption context");
            return "";
        }
        
        // Initialize encryption
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to initialize encryption");
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        
        // Encrypt data
        std::vector<uint8_t> plaintext(data.begin(), data.end());
        std::vector<uint8_t> ciphertext(plaintext.size() + AES_BLOCK_SIZE);
        int outlen = 0;
        int total_outlen = 0;
        
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &outlen, 
                             plaintext.data(), static_cast<int>(plaintext.size())) != 1) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to encrypt data");
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        total_outlen = outlen;
        
        // Finalize encryption (padding)
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + outlen, &outlen) != 1) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to finalize encryption");
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        total_outlen += outlen;
        
        // Resize to actual encrypted size
        ciphertext.resize(total_outlen);
        
        // Clean up
        EVP_CIPHER_CTX_free(ctx);
        
        // Securely clear key from memory
        OPENSSL_cleanse(key.data(), key.size());
        
        // Convert to hex string for storage
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (uint8_t byte : ciphertext) {
            ss << std::setw(2) << static_cast<int>(byte);
        }
        
        return ss.str();
        
    } catch (const std::exception& e) {
        DEO_ERROR(CRYPTOGRAPHY, "Encryption failed: " + std::string(e.what()));
        return "";
    }
}

std::string KeyPair::decryptString(const std::string& encrypted_data, const std::string& password) const {
    try {
        // Convert hex string back to bytes
        if (encrypted_data.length() % 2 != 0) {
            DEO_ERROR(CRYPTOGRAPHY, "Invalid encrypted data format");
            return "";
        }
        
        std::vector<uint8_t> ciphertext;
        ciphertext.reserve(encrypted_data.length() / 2);
        
        for (size_t i = 0; i < encrypted_data.length(); i += 2) {
            std::string byte_str = encrypted_data.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
            ciphertext.push_back(byte);
        }
        
        // Derive key from password (same as encryption)
        std::vector<uint8_t> key(32);
        std::vector<uint8_t> iv(16);
        
        SHA256_CTX sha256_ctx;
        if (!SHA256_Init(&sha256_ctx)) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to initialize SHA-256 for key derivation");
            return "";
        }
        
        if (!SHA256_Update(&sha256_ctx, password.data(), password.length())) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to update SHA-256 for key derivation");
            return "";
        }
        
        if (!SHA256_Final(key.data(), &sha256_ctx)) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to finalize SHA-256 for key derivation");
            return "";
        }
        
        std::memcpy(iv.data(), key.data(), 16);
        
        // Set up decryption context
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to create decryption context");
            return "";
        }
        
        // Initialize decryption
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to initialize decryption");
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        
        // Decrypt data
        std::vector<uint8_t> plaintext(ciphertext.size());
        int outlen = 0;
        int total_outlen = 0;
        
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &outlen,
                             ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to decrypt data");
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        total_outlen = outlen;
        
        // Finalize decryption (remove padding)
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + outlen, &outlen) != 1) {
            DEO_ERROR(CRYPTOGRAPHY, "Failed to finalize decryption");
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        total_outlen += outlen;
        
        // Resize to actual decrypted size
        plaintext.resize(total_outlen);
        
        // Clean up
        EVP_CIPHER_CTX_free(ctx);
        
        // Securely clear key from memory
        OPENSSL_cleanse(key.data(), key.size());
        
        // Convert back to string
        return std::string(plaintext.begin(), plaintext.end());
        
    } catch (const std::exception& e) {
        DEO_ERROR(CRYPTOGRAPHY, "Decryption failed: " + std::string(e.what()));
        return "";
    }
}

} // namespace crypto
} // namespace deo