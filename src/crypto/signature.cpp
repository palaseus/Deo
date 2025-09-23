/**
 * @file signature.cpp
 * @brief Digital signature functions implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "crypto/signature.h"
#include "crypto/hash.h"
#include "crypto/openssl_compat.h"
#include "utils/logger.h"
#include "utils/error_handler.h"

#include <openssl/rand.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace deo {
namespace crypto {

// Static initialization flag
static bool ssl_initialized = false;

std::string Signature::sign(const std::string& data, const std::string& private_key) {
    return bytesToHex(signBinary(data, private_key));
}

std::vector<uint8_t> Signature::signBinary(const std::string& data, const std::string& private_key) {
    initializeOpenSSL();
    
    if (!isValidPrivateKey(private_key)) {
        DEO_ERROR(CRYPTOGRAPHY, "Invalid private key format");
        throw std::invalid_argument("Invalid private key format");
    }
    
    std::vector<uint8_t> data_bytes(data.begin(), data.end());
    std::vector<uint8_t> private_key_bytes = hexToBytes(private_key);
    
    return ecdsaSign(data_bytes, private_key_bytes);
}

bool Signature::verify(const std::string& data, const std::string& signature, const std::string& public_key) {
    std::vector<uint8_t> signature_bytes = hexToBytes(signature);
    return verifyBinary(data, signature_bytes, public_key);
}

bool Signature::verifyBinary(const std::string& data, const std::vector<uint8_t>& signature, const std::string& public_key) {
    initializeOpenSSL();
    
    if (!isValidPublicKey(public_key)) {
        DEO_ERROR(CRYPTOGRAPHY, "Invalid public key format");
        return false;
    }
    
    std::vector<uint8_t> data_bytes(data.begin(), data.end());
    std::vector<uint8_t> public_key_bytes = hexToBytes(public_key);
    
    return ecdsaVerify(data_bytes, signature, public_key_bytes);
}

std::string Signature::getPublicKey(const std::string& private_key) {
    initializeOpenSSL();
    
    if (!isValidPrivateKey(private_key)) {
        DEO_ERROR(CRYPTOGRAPHY, "Invalid private key format");
        return "";
    }
    
    EC_KEY* ec_key = static_cast<EC_KEY*>(createECKeyFromPrivateKey(private_key));
    if (!ec_key) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to create EC key from private key");
        return "";
    }
    
    std::string public_key = ecKeyToHexString(ec_key, true); // Use compressed format
    EC_KEY_free(ec_key);
    
    return public_key;
}

bool Signature::generateKeyPair(std::string& private_key, std::string& public_key) {
    initializeOpenSSL();
    
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!ec_key) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to create EC key");
        return false;
    }
    
    if (!EC_KEY_generate_key(ec_key)) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to generate EC key pair");
        EC_KEY_free(ec_key);
        return false;
    }
    
    // Get private key
    const BIGNUM* private_bn = EC_KEY_get0_private_key(ec_key);
    if (!private_bn) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to get private key");
        EC_KEY_free(ec_key);
        return false;
    }
    
    char* private_hex = BN_bn2hex(private_bn);
    if (!private_hex) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to convert private key to hex");
        EC_KEY_free(ec_key);
        return false;
    }
    
    private_key = private_hex;
    OPENSSL_free(private_hex);
    
    // Get public key
    public_key = ecKeyToHexString(ec_key, true); // Use compressed format
    
    EC_KEY_free(ec_key);
    
    DEO_LOG_INFO(CRYPTOGRAPHY, "Key pair generated successfully");
    return true;
}

bool Signature::isValidPrivateKey(const std::string& private_key) {
    if (private_key.length() != 64) { // 32 bytes = 64 hex characters
        return false;
    }
    
    for (char c : private_key) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    
    // Check if private key is not zero
    if (private_key == "0000000000000000000000000000000000000000000000000000000000000000") {
        return false;
    }
    
    return true;
}

bool Signature::isValidPublicKey(const std::string& public_key) {
    if (public_key.length() != 66 && public_key.length() != 130) {
        return false; // Compressed (33 bytes) or uncompressed (65 bytes)
    }
    
    for (char c : public_key) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    
    return true;
}

std::string Signature::publicKeyToAddress(const std::string& public_key) {
    if (!isValidPublicKey(public_key)) {
        DEO_ERROR(CRYPTOGRAPHY, "Invalid public key format");
        return "";
    }
    
    // Hash the public key with SHA-256 and RIPEMD-160
    std::string hash160 = Hash::hash160(public_key);
    
    // Add version byte (0x00 for mainnet)
    std::string versioned_hash = "00" + hash160;
    
    // Calculate checksum (first 4 bytes of double SHA-256)
    std::string checksum = Hash::doubleSha256(versioned_hash).substr(0, 8);
    
    // Combine version + hash + checksum
    std::string address_data = versioned_hash + checksum;
    
    // Convert to Base58 (simplified implementation)
    // Note: In a real implementation, we would use proper Base58Check encoding
    DEO_LOG_WARNING(CRYPTOGRAPHY, "Base58Check encoding not fully implemented yet");
    
    return "1" + hash160.substr(0, 20); // Placeholder address format
}

std::string Signature::compressPublicKey(const std::string& public_key) {
    if (!isValidPublicKey(public_key)) {
        DEO_ERROR(CRYPTOGRAPHY, "Invalid public key format");
        return "";
    }
    
    if (isCompressedPublicKey(public_key)) {
        return public_key; // Already compressed
    }
    
    // Convert uncompressed to compressed
    if (public_key.length() != 130) {
        DEO_ERROR(CRYPTOGRAPHY, "Invalid uncompressed public key length");
        return "";
    }
    
    std::string x_coord = public_key.substr(2, 64); // Skip "04" prefix
    std::string y_coord = public_key.substr(66, 64);
    
    // Determine compression prefix based on y coordinate
    char y_last_char = y_coord.back();
    char prefix = (y_last_char % 2 == 0) ? '2' : '3';
    
    return prefix + x_coord;
}

std::string Signature::uncompressPublicKey(const std::string& public_key) {
    if (!isValidPublicKey(public_key)) {
        DEO_ERROR(CRYPTOGRAPHY, "Invalid public key format");
        return "";
    }
    
    if (!isCompressedPublicKey(public_key)) {
        return public_key; // Already uncompressed
    }
    
    // Note: In a real implementation, we would perform elliptic curve point decompression
    // This is a placeholder implementation
    DEO_LOG_WARNING(CRYPTOGRAPHY, "Public key decompression not fully implemented yet");
    
    return "04" + public_key.substr(1); // Placeholder
}

bool Signature::isCompressedPublicKey(const std::string& public_key) {
    return public_key.length() == 66 && (public_key[0] == '2' || public_key[0] == '3');
}

std::string Signature::recoverPublicKey(const std::string& data, const std::string& signature, int recovery_id) {
    // Note: In a real implementation, we would perform ECDSA public key recovery
    // This is a placeholder implementation
    DEO_LOG_WARNING(CRYPTOGRAPHY, "Public key recovery not fully implemented yet");
    
    (void)data;
    (void)signature;
    (void)recovery_id;
    
    return "";
}

int Signature::getRecoveryId(const std::string& data, const std::string& signature, const std::string& public_key) {
    // Note: In a real implementation, we would calculate the recovery ID
    // This is a placeholder implementation
    DEO_LOG_WARNING(CRYPTOGRAPHY, "Recovery ID calculation not fully implemented yet");
    
    (void)data;
    (void)signature;
    (void)public_key;
    
    return -1;
}

void Signature::initializeOpenSSL() {
    if (!ssl_initialized) {
        OpenSSL_add_all_algorithms();
        ssl_initialized = true;
        DEO_LOG_DEBUG(CRYPTOGRAPHY, "OpenSSL initialized for signatures");
    }
}

void* Signature::getSecp256k1Group() {
    return EC_GROUP_new_by_curve_name(NID_secp256k1);
}

void* Signature::createECKeyFromPrivateKey(const std::string& private_key_hex) {
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!ec_key) {
        return nullptr;
    }
    
    BIGNUM* private_bn = BN_new();
    if (!BN_hex2bn(&private_bn, private_key_hex.c_str())) {
        BN_free(private_bn);
        EC_KEY_free(ec_key);
        return nullptr;
    }
    
    if (!EC_KEY_set_private_key(ec_key, private_bn)) {
        BN_free(private_bn);
        EC_KEY_free(ec_key);
        return nullptr;
    }
    
    // Generate public key from private key
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    EC_POINT* public_point = EC_POINT_new(group);
    
    if (!EC_POINT_mul(group, public_point, private_bn, nullptr, nullptr, nullptr)) {
        EC_POINT_free(public_point);
        BN_free(private_bn);
        EC_KEY_free(ec_key);
        return nullptr;
    }
    
    if (!EC_KEY_set_public_key(ec_key, public_point)) {
        EC_POINT_free(public_point);
        BN_free(private_bn);
        EC_KEY_free(ec_key);
        return nullptr;
    }
    
    EC_POINT_free(public_point);
    BN_free(private_bn);
    
    return ec_key;
}

std::string Signature::ecKeyToHexString(void* ec_key, bool compressed) {
    EC_KEY* key = static_cast<EC_KEY*>(ec_key);
    const EC_POINT* public_point = EC_KEY_get0_public_key(key);
    const EC_GROUP* group = EC_KEY_get0_group(key);
    
    if (!public_point || !group) {
        return "";
    }
    
    point_conversion_form_t form = compressed ? POINT_CONVERSION_COMPRESSED : POINT_CONVERSION_UNCOMPRESSED;
    size_t public_key_length = EC_POINT_point2oct(group, public_point, form, nullptr, 0, nullptr);
    
    if (public_key_length == 0) {
        return "";
    }
    
    std::vector<uint8_t> public_key_bytes(public_key_length);
    if (!EC_POINT_point2oct(group, public_point, form, public_key_bytes.data(), public_key_length, nullptr)) {
        return "";
    }
    
    return bytesToHex(public_key_bytes);
}

std::vector<uint8_t> Signature::ecdsaSign(const std::vector<uint8_t>& data, const std::vector<uint8_t>& private_key_bytes) {
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!ec_key) {
        throw std::runtime_error("Failed to create EC key");
    }
    
    BIGNUM* private_bn = BN_bin2bn(private_key_bytes.data(), static_cast<int>(private_key_bytes.size()), nullptr);
    if (!private_bn) {
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to create BIGNUM from private key");
    }
    
    if (!EC_KEY_set_private_key(ec_key, private_bn)) {
        BN_free(private_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to set private key");
    }
    
    // Generate public key
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    EC_POINT* public_point = EC_POINT_new(group);
    
    if (!EC_POINT_mul(group, public_point, private_bn, nullptr, nullptr, nullptr)) {
        EC_POINT_free(public_point);
        BN_free(private_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to generate public key");
    }
    
    if (!EC_KEY_set_public_key(ec_key, public_point)) {
        EC_POINT_free(public_point);
        BN_free(private_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to set public key");
    }
    
    EC_POINT_free(public_point);
    BN_free(private_bn);
    
    // Sign the data
    ECDSA_SIG* signature = ECDSA_do_sign(data.data(), static_cast<int>(data.size()), ec_key);
    if (!signature) {
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to create ECDSA signature");
    }
    
    // Convert signature to DER format
    unsigned char* der_signature = nullptr;
    int der_length = i2d_ECDSA_SIG(signature, &der_signature);
    
    if (der_length <= 0) {
        ECDSA_SIG_free(signature);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to convert signature to DER format");
    }
    
    std::vector<uint8_t> signature_bytes(der_signature, der_signature + der_length);
    
    OPENSSL_free(der_signature);
    ECDSA_SIG_free(signature);
    EC_KEY_free(ec_key);
    
    return signature_bytes;
}

bool Signature::ecdsaVerify(const std::vector<uint8_t>& data, 
                           const std::vector<uint8_t>& signature, 
                           const std::vector<uint8_t>& public_key_bytes) {
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!ec_key) {
        return false;
    }
    
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    EC_POINT* public_point = EC_POINT_new(group);
    
    if (!EC_POINT_oct2point(group, public_point, public_key_bytes.data(), public_key_bytes.size(), nullptr)) {
        EC_POINT_free(public_point);
        EC_KEY_free(ec_key);
        return false;
    }
    
    if (!EC_KEY_set_public_key(ec_key, public_point)) {
        EC_POINT_free(public_point);
        EC_KEY_free(ec_key);
        return false;
    }
    
    EC_POINT_free(public_point);
    
    // Convert DER signature to ECDSA_SIG
    const unsigned char* der_ptr = signature.data();
    ECDSA_SIG* ecdsa_sig = d2i_ECDSA_SIG(nullptr, &der_ptr, static_cast<long>(signature.size()));
    
    if (!ecdsa_sig) {
        EC_KEY_free(ec_key);
        return false;
    }
    
    // Verify signature
    int result = ECDSA_do_verify(data.data(), static_cast<int>(data.size()), ecdsa_sig, ec_key);
    
    ECDSA_SIG_free(ecdsa_sig);
    EC_KEY_free(ec_key);
    
    return result == 1;
}

std::vector<uint8_t> Signature::hexToBytes(const std::string& hex) {
    return Hash::hexToBytes(hex);
}

std::string Signature::bytesToHex(const std::vector<uint8_t>& bytes) {
    return Hash::bytesToHex(bytes);
}

} // namespace crypto
} // namespace deo