/**
 * @file hash.cpp
 * @brief Cryptographic hash functions implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "crypto/hash.h"
#include "crypto/openssl_compat.h"
#include "utils/logger.h"
#include "utils/error_handler.h"

#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace deo {
namespace crypto {

// Static initialization flag
static bool ssl_initialized = false;

std::string Hash::sha256(const std::string& input) {
    return bytesToHex(sha256Binary(input));
}

std::string Hash::sha256(const std::vector<uint8_t>& data) {
    return bytesToHex(sha256Binary(data));
}

std::vector<uint8_t> Hash::sha256Binary(const std::string& input) {
    return sha256Binary(std::vector<uint8_t>(input.begin(), input.end()));
}

std::vector<uint8_t> Hash::sha256Binary(const std::vector<uint8_t>& data) {
    initializeOpenSSL();
    
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    
    SHA256_CTX sha256_ctx;
    if (!SHA256_Init(&sha256_ctx)) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to initialize SHA-256 context");
        throw std::runtime_error("SHA-256 initialization failed");
    }
    
    if (!SHA256_Update(&sha256_ctx, data.data(), data.size())) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to update SHA-256 context");
        throw std::runtime_error("SHA-256 update failed");
    }
    
    if (!SHA256_Final(hash.data(), &sha256_ctx)) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to finalize SHA-256 context");
        throw std::runtime_error("SHA-256 finalization failed");
    }
    
    return hash;
}

std::string Hash::doubleSha256(const std::string& input) {
    std::vector<uint8_t> first_hash = sha256Binary(input);
    return bytesToHex(sha256Binary(first_hash));
}

std::string Hash::doubleSha256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> first_hash = sha256Binary(data);
    return bytesToHex(sha256Binary(first_hash));
}

std::string Hash::ripemd160(const std::string& input) {
    return ripemd160(std::vector<uint8_t>(input.begin(), input.end()));
}

std::string Hash::ripemd160(const std::vector<uint8_t>& data) {
    initializeOpenSSL();
    
    std::vector<uint8_t> hash(RIPEMD160_DIGEST_LENGTH);
    
    RIPEMD160_CTX ripemd_ctx;
    if (!RIPEMD160_Init(&ripemd_ctx)) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to initialize RIPEMD-160 context");
        throw std::runtime_error("RIPEMD-160 initialization failed");
    }
    
    if (!RIPEMD160_Update(&ripemd_ctx, data.data(), data.size())) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to update RIPEMD-160 context");
        throw std::runtime_error("RIPEMD-160 update failed");
    }
    
    if (!RIPEMD160_Final(hash.data(), &ripemd_ctx)) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to finalize RIPEMD-160 context");
        throw std::runtime_error("RIPEMD-160 finalization failed");
    }
    
    return bytesToHex(hash);
}

std::string Hash::hash160(const std::string& input) {
    return hash160(std::vector<uint8_t>(input.begin(), input.end()));
}

std::string Hash::hash160(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> sha256_hash = sha256Binary(data);
    return ripemd160(sha256_hash);
}

std::vector<uint8_t> Hash::hexToBytes(const std::string& hex) {
    if (hex.length() % 2 != 0) {
        std::cout << "DEBUG: Odd length hex string: '" << hex << "' (length: " << hex.length() << ")" << std::endl;
        DEO_ERROR(VALIDATION, "Invalid hex string length");
        throw std::invalid_argument("Hex string length must be even");
    }
    
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.length() / 2);
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }
    
    return bytes;
}

std::string Hash::bytesToHex(const std::vector<uint8_t>& data) {
    return bytesToHex(data.data(), data.size());
}

std::string Hash::bytesToHex(const uint8_t* data, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    
    return ss.str();
}

bool Hash::isValidHash(const std::string& hash) {
    if (hash.length() != 64) { // SHA-256 produces 64 hex characters
        return false;
    }
    
    for (char c : hash) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    
    return true;
}

std::vector<uint8_t> Hash::randomBytes(size_t length) {
    initializeOpenSSL();
    
    std::vector<uint8_t> bytes(length);
    
    if (!RAND_bytes(bytes.data(), static_cast<int>(length))) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to generate random bytes");
        throw std::runtime_error("Random byte generation failed");
    }
    
    return bytes;
}

std::string Hash::hmacSha256(const std::string& key, const std::string& data) {
    return bytesToHex(hmacSha256Binary(key, data));
}

std::vector<uint8_t> Hash::hmacSha256Binary(const std::string& key, const std::string& data) {
    initializeOpenSSL();
    
    std::vector<uint8_t> hmac(SHA256_DIGEST_LENGTH);
    unsigned int hmac_length;
    
    unsigned char* result = HMAC(EVP_sha256(),
                                key.data(), static_cast<int>(key.length()),
                                reinterpret_cast<const unsigned char*>(data.data()), data.length(),
                                hmac.data(), &hmac_length);
    
    if (!result) {
        DEO_ERROR(CRYPTOGRAPHY, "Failed to compute HMAC-SHA256");
        throw std::runtime_error("HMAC-SHA256 computation failed");
    }
    
    return hmac;
}

void Hash::initializeOpenSSL() {
    if (!ssl_initialized) {
        OpenSSL_add_all_algorithms();
        ssl_initialized = true;
        DEO_LOG_DEBUG(CRYPTOGRAPHY, "OpenSSL initialized");
    }
}

bool Hash::isOpenSSLInitialized() {
    return ssl_initialized;
}

} // namespace crypto
} // namespace deo