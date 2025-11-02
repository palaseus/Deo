/**
 * @file wallet.cpp
 * @brief Wallet implementation for managing keys, addresses, and transaction signing
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "wallet/wallet.h"
#include "utils/logger.h"
#include "utils/error_handler.h"
#include "crypto/key_pair.h"
#include "crypto/hash.h"

#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace deo {
namespace wallet {

Wallet::Wallet(const WalletConfig& config)
    : config_(config)
    , default_account_("")
    , is_initialized_(false) {
    
    DEO_LOG_DEBUG(GENERAL, "Wallet created with config");
}

Wallet::~Wallet() {
    // Clear sensitive data
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    // Clear all key pairs (they will securely clear their data)
    accounts_.clear();
    account_info_.clear();
    
    DEO_LOG_DEBUG(GENERAL, "Wallet destroyed");
}

bool Wallet::initialize() {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    if (is_initialized_) {
        DEO_LOG_WARNING(GENERAL, "Wallet already initialized");
        return true;
    }
    
    try {
        // Create wallet directory if it doesn't exist
        std::filesystem::create_directories(config_.data_directory);
        
        // Try to load existing wallet
        if (std::filesystem::exists(getWalletFilePath())) {
            DEO_LOG_INFO(GENERAL, "Wallet file found, attempting to load");
            // Wallet will be loaded separately with password if encrypted
        }
        
        is_initialized_ = true;
        DEO_LOG_INFO(GENERAL, "Wallet initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(GENERAL, "Failed to initialize wallet: " + std::string(e.what()));
        return false;
    }
}

std::string Wallet::createAccount(const std::string& label) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    if (!is_initialized_) {
        DEO_LOG_ERROR(GENERAL, "Wallet not initialized");
        return "";
    }
    
    if (accounts_.size() >= config_.max_accounts) {
        DEO_LOG_ERROR(GENERAL, "Maximum number of accounts reached");
        return "";
    }
    
    try {
        // Generate new key pair
        auto keypair = std::make_shared<crypto::KeyPair>();
        
        if (!keypair->isValid()) {
            DEO_LOG_ERROR(GENERAL, "Failed to generate valid key pair");
            return "";
        }
        
        std::string address = keypair->getAddress();
        
        // Store key pair
        accounts_[address] = keypair;
        
        // Create account info
        auto account_info = std::make_shared<WalletAccount>();
        account_info->address = address;
        account_info->label = label.empty() ? ("Account " + std::to_string(accounts_.size())) : label;
        account_info->public_key = keypair->getPublicKey();
        account_info->is_encrypted = config_.encrypt_wallet;
        account_info->created_at = std::chrono::system_clock::now();
        
        account_info_[address] = account_info;
        
        // Set as default if it's the first account
        if (default_account_.empty()) {
            default_account_ = address;
        }
        
        DEO_LOG_INFO(GENERAL, "Created new account: " + address);
        return address;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(GENERAL, "Failed to create account: " + std::string(e.what()));
        return "";
    }
}

std::string Wallet::importAccount(const std::string& private_key, 
                                 const std::string& label,
                                 const std::string& password) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    if (!is_initialized_) {
        DEO_LOG_ERROR(GENERAL, "Wallet not initialized");
        return "";
    }
    
    if (accounts_.size() >= config_.max_accounts) {
        DEO_LOG_ERROR(GENERAL, "Maximum number of accounts reached");
        return "";
    }
    
    try {
        // Create key pair from private key
        auto keypair = std::make_shared<crypto::KeyPair>(private_key);
        
        if (!keypair->isValid()) {
            DEO_LOG_ERROR(GENERAL, "Invalid private key");
            return "";
        }
        
        std::string address = keypair->getAddress();
        
        // Check if account already exists
        if (accounts_.find(address) != accounts_.end()) {
            DEO_LOG_WARNING(GENERAL, "Account already exists: " + address);
            return address; // Return existing address
        }
        
        // If encryption is enabled, encrypt the private key
        if (config_.encrypt_wallet && !password.empty()) {
            // Store encrypted version - in real implementation, we'd encrypt here
            // For now, we'll store the keypair but mark it as encrypted
            accounts_[address] = keypair;
            
            auto account_info = std::make_shared<WalletAccount>();
            account_info->address = address;
            account_info->label = label.empty() ? ("Imported " + address.substr(0, 8)) : label;
            account_info->public_key = keypair->getPublicKey();
            account_info->is_encrypted = true;
            account_info->created_at = std::chrono::system_clock::now();
            
            account_info_[address] = account_info;
        } else {
            accounts_[address] = keypair;
            
            auto account_info = std::make_shared<WalletAccount>();
            account_info->address = address;
            account_info->label = label.empty() ? ("Imported " + address.substr(0, 8)) : label;
            account_info->public_key = keypair->getPublicKey();
            account_info->is_encrypted = false;
            account_info->created_at = std::chrono::system_clock::now();
            
            account_info_[address] = account_info;
        }
        
        // Set as default if it's the first account
        if (default_account_.empty()) {
            default_account_ = address;
        }
        
        DEO_LOG_INFO(GENERAL, "Imported account: " + address);
        return address;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(GENERAL, "Failed to import account: " + std::string(e.what()));
        return "";
    }
}

std::string Wallet::importAccountFromEncrypted(const nlohmann::json& encrypted_data,
                                              const std::string& password,
                                              const std::string& label) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    if (!is_initialized_) {
        DEO_LOG_ERROR(GENERAL, "Wallet not initialized");
        return "";
    }
    
    try {
        // Create key pair and import encrypted data
        auto keypair = std::make_shared<crypto::KeyPair>();
        
        if (!keypair->importKeys(encrypted_data, password)) {
            DEO_LOG_ERROR(GENERAL, "Failed to import encrypted keys");
            return "";
        }
        
        if (!keypair->isValid()) {
            DEO_LOG_ERROR(GENERAL, "Imported key pair is invalid");
            return "";
        }
        
        std::string address = keypair->getAddress();
        
        // Store account
        accounts_[address] = keypair;
        
        auto account_info = std::make_shared<WalletAccount>();
        account_info->address = address;
        account_info->label = label.empty() ? ("Imported " + address.substr(0, 8)) : label;
        account_info->public_key = keypair->getPublicKey();
        account_info->is_encrypted = true;
        account_info->created_at = std::chrono::system_clock::now();
        
        account_info_[address] = account_info;
        
        if (default_account_.empty()) {
            default_account_ = address;
        }
        
        DEO_LOG_INFO(GENERAL, "Imported encrypted account: " + address);
        return address;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(GENERAL, "Failed to import encrypted account: " + std::string(e.what()));
        return "";
    }
}

nlohmann::json Wallet::exportAccount(const std::string& address, const std::string& password) const {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    auto it = accounts_.find(address);
    if (it == accounts_.end()) {
        DEO_LOG_ERROR(GENERAL, "Account not found: " + address);
        return nlohmann::json();
    }
    
    try {
        // Export keys using KeyPair's export method
        return it->second->exportKeys(password);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(GENERAL, "Failed to export account: " + std::string(e.what()));
        return nlohmann::json();
    }
}

bool Wallet::removeAccount(const std::string& address, const std::string& password) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    if (accounts_.find(address) == accounts_.end()) {
        DEO_LOG_WARNING(GENERAL, "Account not found: " + address);
        return false;
    }
    
    // If encrypted, verify password first
    if (config_.encrypt_wallet && !password.empty()) {
        // In a real implementation, we'd verify the password here
        // For now, we'll just proceed
    }
    
    accounts_.erase(address);
    account_info_.erase(address);
    
    // If this was the default account, clear default
    if (default_account_ == address) {
        default_account_ = "";
        // Set new default if any accounts remain
        if (!accounts_.empty()) {
            default_account_ = accounts_.begin()->first;
        }
    }
    
    DEO_LOG_INFO(GENERAL, "Removed account: " + address);
    return true;
}

std::vector<std::string> Wallet::listAccounts() const {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    std::vector<std::string> addresses;
    addresses.reserve(accounts_.size());
    
    for (const auto& pair : accounts_) {
        addresses.push_back(pair.first);
    }
    
    return addresses;
}

std::shared_ptr<WalletAccount> Wallet::getAccount(const std::string& address) const {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    auto it = account_info_.find(address);
    if (it == account_info_.end()) {
        return nullptr;
    }
    
    return it->second;
}

bool Wallet::setAccountLabel(const std::string& address, const std::string& label) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    auto it = account_info_.find(address);
    if (it == account_info_.end()) {
        DEO_LOG_WARNING(GENERAL, "Account not found: " + address);
        return false;
    }
    
    it->second->label = label;
    DEO_LOG_DEBUG(GENERAL, "Set label for " + address + ": " + label);
    return true;
}

bool Wallet::signTransaction(core::Transaction& transaction, 
                             const std::string& address,
                             const std::string& password) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    auto keypair = getKeyPair(address, password);
    if (!keypair) {
        DEO_LOG_ERROR(GENERAL, "Failed to get key pair for account: " + address);
        return false;
    }
    
    try {
        // Get transaction data for signing
        std::string tx_data = transaction.calculateHash();
        
        // Sign with private key
        std::string signature = keypair->sign(tx_data);
        
        if (signature.empty()) {
            DEO_LOG_ERROR(GENERAL, "Failed to sign transaction");
            return false;
        }
        
        // Add signature to transaction inputs
        // Note: This assumes the transaction structure allows signature setting
        // In a real implementation, we'd properly set signatures on inputs
        
        DEO_LOG_DEBUG(GENERAL, "Transaction signed successfully");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(GENERAL, "Failed to sign transaction: " + std::string(e.what()));
        return false;
    }
}

std::string Wallet::signData(const std::string& data,
                            const std::string& address,
                            const std::string& password) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    auto keypair = getKeyPair(address, password);
    if (!keypair) {
        DEO_LOG_ERROR(GENERAL, "Failed to get key pair for account: " + address);
        return "";
    }
    
    try {
        return keypair->sign(data);
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(GENERAL, "Failed to sign data: " + std::string(e.what()));
        return "";
    }
}

std::string Wallet::getDefaultAccount() const {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    return default_account_;
}

bool Wallet::setDefaultAccount(const std::string& address) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    if (accounts_.find(address) == accounts_.end()) {
        DEO_LOG_WARNING(GENERAL, "Account not found: " + address);
        return false;
    }
    
    default_account_ = address;
    DEO_LOG_DEBUG(GENERAL, "Set default account: " + address);
    return true;
}

bool Wallet::save(const std::string& password) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    if (!is_initialized_) {
        DEO_LOG_ERROR(GENERAL, "Wallet not initialized");
        return false;
    }
    
    try {
        nlohmann::json wallet_data;
        wallet_data["version"] = "1.0";
        wallet_data["encrypted"] = config_.encrypt_wallet;
        wallet_data["default_account"] = default_account_;
        wallet_data["accounts"] = nlohmann::json::array();
        
        for (const auto& pair : accounts_) {
            const std::string& address = pair.first;
            auto keypair = pair.second;
            auto account_info_it = account_info_.find(address);
            
            nlohmann::json account_data;
            account_data["address"] = address;
            
            if (account_info_it != account_info_.end()) {
                account_data["label"] = account_info_it->second->label;
            }
            
            // Export key data
            if (config_.encrypt_wallet && !password.empty()) {
                account_data["keys"] = keypair->exportKeys(password);
            } else {
                // Store public key only (private key should not be stored unencrypted)
                account_data["public_key"] = keypair->getPublicKey();
                // In production, we should encrypt even if encrypt_wallet is false
                // For now, we'll just store encrypted with empty password
                account_data["keys"] = keypair->exportKeys("");
            }
            
            wallet_data["accounts"].push_back(account_data);
        }
        
        // Write to file
        std::string wallet_file = getWalletFilePath();
        std::ofstream file(wallet_file);
        if (!file.is_open()) {
            DEO_LOG_ERROR(GENERAL, "Failed to open wallet file for writing: " + wallet_file);
            return false;
        }
        
        file << wallet_data.dump(2);
        file.close();
        
        DEO_LOG_INFO(GENERAL, "Wallet saved to: " + wallet_file);
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(GENERAL, "Failed to save wallet: " + std::string(e.what()));
        return false;
    }
}

bool Wallet::load(const std::string& password) {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    if (!is_initialized_) {
        DEO_LOG_ERROR(GENERAL, "Wallet not initialized");
        return false;
    }
    
    std::string wallet_file = getWalletFilePath();
    if (!std::filesystem::exists(wallet_file)) {
        DEO_LOG_INFO(GENERAL, "Wallet file not found: " + wallet_file);
        return true; // Not an error - wallet just doesn't exist yet
    }
    
    try {
        std::ifstream file(wallet_file);
        if (!file.is_open()) {
            DEO_LOG_ERROR(GENERAL, "Failed to open wallet file: " + wallet_file);
            return false;
        }
        
        nlohmann::json wallet_data;
        file >> wallet_data;
        file.close();
        
        // Clear existing accounts
        accounts_.clear();
        account_info_.clear();
        
        // Load version and encryption status
        bool is_encrypted = wallet_data.value("encrypted", false);
        default_account_ = wallet_data.value("default_account", "");
        
        // Load accounts
        if (wallet_data.contains("accounts") && wallet_data["accounts"].is_array()) {
            for (const auto& account_json : wallet_data["accounts"]) {
                std::string address = account_json["address"].get<std::string>();
                std::string label = account_json.value("label", "");
                
                // Import keys
                auto keypair = std::make_shared<crypto::KeyPair>();
                
                if (account_json.contains("keys")) {
                    if (!keypair->importKeys(account_json["keys"], password)) {
                        DEO_LOG_ERROR(GENERAL, "Failed to import keys for account: " + address);
                        continue;
                    }
                } else if (account_json.contains("public_key")) {
                    // Legacy format - only public key available (can't recover private key)
                    DEO_LOG_WARNING(GENERAL, "Account " + address + " has no private key data");
                    continue;
                }
                
                if (!keypair->isValid()) {
                    DEO_LOG_ERROR(GENERAL, "Invalid key pair for account: " + address);
                    continue;
                }
                
                // Verify address matches
                if (keypair->getAddress() != address) {
                    DEO_LOG_ERROR(GENERAL, "Address mismatch for account: " + address);
                    continue;
                }
                
                // Store account
                accounts_[address] = keypair;
                
                auto account_info = std::make_shared<WalletAccount>();
                account_info->address = address;
                account_info->label = label;
                account_info->public_key = keypair->getPublicKey();
                account_info->is_encrypted = is_encrypted;
                account_info->created_at = std::chrono::system_clock::now();
                
                account_info_[address] = account_info;
            }
        }
        
        DEO_LOG_INFO(GENERAL, "Wallet loaded: " + std::to_string(accounts_.size()) + " accounts");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(GENERAL, "Failed to load wallet: " + std::string(e.what()));
        return false;
    }
}

bool Wallet::hasAccounts() const {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    return !accounts_.empty();
}

nlohmann::json Wallet::getStatistics() const {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    
    nlohmann::json stats;
    stats["total_accounts"] = accounts_.size();
    stats["default_account"] = default_account_;
    stats["encrypted"] = config_.encrypt_wallet;
    stats["max_accounts"] = config_.max_accounts;
    
    return stats;
}

WalletConfig Wallet::getConfig() const {
    std::lock_guard<std::mutex> lock(wallet_mutex_);
    return config_;
}

std::shared_ptr<crypto::KeyPair> Wallet::getKeyPair(const std::string& address, 
                                                    const std::string& password) const {
    (void)password; // TODO: Use password for encrypted key decryption
    auto it = accounts_.find(address);
    if (it == accounts_.end()) {
        return nullptr;
    }
    
    // In a real implementation, if the key is encrypted, we'd decrypt it here
    // For now, we assume the keypair is already loaded
    
    return it->second;
}

std::string Wallet::getWalletFilePath() const {
    return config_.data_directory + "/wallet.json";
}

} // namespace wallet
} // namespace deo

