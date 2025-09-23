/**
 * @file key_pair.cpp
 * @brief Key pair management implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "crypto/key_pair.h"
#include "utils/logger.h"
#include "utils/error_handler.h"

#include <random>
#include <sstream>
#include <iomanip>

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
    // Note: In a real implementation, we would use proper public key encryption
    // This is a placeholder implementation
    DEO_LOG_WARNING(CRYPTOGRAPHY, "Public key encryption not fully implemented yet");
    
    // Simple XOR encryption (not secure, just for demonstration)
    std::string encrypted = data;
    for (size_t i = 0; i < encrypted.length(); ++i) {
        encrypted[i] ^= public_key_[i % public_key_.length()];
    }
    
    return encrypted;
}

std::string KeyPair::decrypt(const std::string& encrypted_data) const {
    // Note: In a real implementation, we would use proper private key decryption
    // This is a placeholder implementation
    DEO_LOG_WARNING(CRYPTOGRAPHY, "Private key decryption not fully implemented yet");
    
    // Simple XOR decryption (not secure, just for demonstration)
    std::string decrypted = encrypted_data;
    for (size_t i = 0; i < decrypted.length(); ++i) {
        decrypted[i] ^= public_key_[i % public_key_.length()];
    }
    
    return decrypted;
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
    // Clear private key from memory
    private_key_.clear();
    private_key_.resize(64, '0'); // Fill with zeros
    private_key_.clear();
    
    // Clear public key
    public_key_.clear();
    
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
    // Note: In a real implementation, we would use proper encryption (AES, etc.)
    // This is a placeholder implementation
    DEO_LOG_WARNING(CRYPTOGRAPHY, "String encryption not fully implemented yet");
    
    // Simple XOR encryption (not secure, just for demonstration)
    std::string encrypted = data;
    for (size_t i = 0; i < encrypted.length(); ++i) {
        encrypted[i] ^= password[i % password.length()];
    }
    
    return encrypted;
}

std::string KeyPair::decryptString(const std::string& encrypted_data, const std::string& password) const {
    // Note: In a real implementation, we would use proper decryption
    // This is a placeholder implementation
    DEO_LOG_WARNING(CRYPTOGRAPHY, "String decryption not fully implemented yet");
    
    // Simple XOR decryption (not secure, just for demonstration)
    std::string decrypted = encrypted_data;
    for (size_t i = 0; i < decrypted.length(); ++i) {
        decrypted[i] ^= password[i % password.length()];
    }
    
    return decrypted;
}

} // namespace crypto
} // namespace deo