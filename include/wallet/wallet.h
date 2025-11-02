/**
 * @file wallet.h
 * @brief Wallet module for managing keys, addresses, and transaction signing
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

#include "crypto/key_pair.h"
#include "core/transaction.h"

namespace deo {
namespace wallet {

/**
 * @brief Wallet account information
 */
struct WalletAccount {
    std::string address;           ///< Account address
    std::string label;              ///< User-friendly label
    std::string public_key;         ///< Public key (for verification)
    bool is_encrypted;              ///< Whether private key is encrypted
    std::chrono::system_clock::time_point created_at; ///< Creation timestamp
    
    WalletAccount()
        : address("")
        , label("")
        , public_key("")
        , is_encrypted(false)
        , created_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief Wallet configuration
 */
struct WalletConfig {
    std::string data_directory;     ///< Directory for wallet files
    bool encrypt_wallet;            ///< Whether to encrypt wallet file
    std::string default_account;    ///< Default account address
    size_t max_accounts;            ///< Maximum number of accounts
    
    WalletConfig()
        : data_directory("./wallet")
        , encrypt_wallet(false)
        , default_account("")
        , max_accounts(100) {}
};

/**
 * @brief Wallet class for managing keys and signing transactions
 * 
 * The wallet manages multiple accounts (key pairs), provides secure
 * storage, and enables transaction signing without exposing private keys.
 */
class Wallet {
public:
    /**
     * @brief Constructor
     * @param config Wallet configuration
     */
    explicit Wallet(const WalletConfig& config);
    
    /**
     * @brief Destructor
     */
    ~Wallet();
    
    // Disable copy and move semantics for security
    Wallet(const Wallet&) = delete;
    Wallet& operator=(const Wallet&) = delete;
    Wallet(Wallet&&) = delete;
    Wallet& operator=(Wallet&&) = delete;
    
    /**
     * @brief Initialize wallet
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Create a new account
     * @param label Optional label for the account
     * @return Address of the new account, or empty string if failed
     */
    std::string createAccount(const std::string& label = "");
    
    /**
     * @brief Import an account from private key
     * @param private_key Private key (hex string)
     * @param label Optional label for the account
     * @param password Password for encryption (if encrypt_wallet is true)
     * @return Address of imported account, or empty string if failed
     */
    std::string importAccount(const std::string& private_key, 
                             const std::string& label = "",
                             const std::string& password = "");
    
    /**
     * @brief Import account from encrypted export
     * @param encrypted_data Encrypted key data (JSON)
     * @param password Decryption password
     * @param label Optional label for the account
     * @return Address of imported account, or empty string if failed
     */
    std::string importAccountFromEncrypted(const nlohmann::json& encrypted_data,
                                          const std::string& password,
                                          const std::string& label = "");
    
    /**
     * @brief Export account to encrypted format
     * @param address Account address
     * @param password Encryption password
     * @return Encrypted account data as JSON
     */
    nlohmann::json exportAccount(const std::string& address, const std::string& password) const;
    
    /**
     * @brief Remove an account
     * @param address Account address
     * @param password Wallet password (if encrypted)
     * @return True if account was removed
     */
    bool removeAccount(const std::string& address, const std::string& password = "");
    
    /**
     * @brief List all accounts
     * @return Vector of account addresses
     */
    std::vector<std::string> listAccounts() const;
    
    /**
     * @brief Get account information
     * @param address Account address
     * @return Account information, or nullptr if not found
     */
    std::shared_ptr<WalletAccount> getAccount(const std::string& address) const;
    
    /**
     * @brief Set account label
     * @param address Account address
     * @param label New label
     * @return True if label was set
     */
    bool setAccountLabel(const std::string& address, const std::string& label);
    
    /**
     * @brief Sign a transaction
     * @param transaction Transaction to sign
     * @param address Address of the account to sign with
     * @param password Wallet password (if encrypted)
     * @return True if transaction was signed successfully
     */
    bool signTransaction(core::Transaction& transaction, 
                        const std::string& address,
                        const std::string& password = "");
    
    /**
     * @brief Sign data with account's private key
     * @param data Data to sign
     * @param address Address of the account
     * @param password Wallet password (if encrypted)
     * @return Signature as hex string, or empty string if failed
     */
    std::string signData(const std::string& data,
                        const std::string& address,
                        const std::string& password = "");
    
    /**
     * @brief Get default account address
     * @return Default account address, or empty string if none
     */
    std::string getDefaultAccount() const;
    
    /**
     * @brief Set default account
     * @param address Account address
     * @return True if default account was set
     */
    bool setDefaultAccount(const std::string& address);
    
    /**
     * @brief Save wallet to disk
     * @param password Wallet password (if encrypted)
     * @return True if wallet was saved
     */
    bool save(const std::string& password = "");
    
    /**
     * @brief Load wallet from disk
     * @param password Wallet password (if encrypted)
     * @return True if wallet was loaded
     */
    bool load(const std::string& password = "");
    
    /**
     * @brief Check if wallet is encrypted
     * @return True if wallet encryption is enabled
     */
    bool isEncrypted() const { return config_.encrypt_wallet; }
    
    /**
     * @brief Check if wallet has accounts
     * @return True if wallet has at least one account
     */
    bool hasAccounts() const;
    
    /**
     * @brief Get wallet statistics
     * @return JSON with wallet statistics
     */
    nlohmann::json getStatistics() const;
    
    /**
     * @brief Get wallet configuration
     * @return Wallet configuration
     */
    WalletConfig getConfig() const;

private:
    WalletConfig config_;                                    ///< Wallet configuration
    mutable std::mutex wallet_mutex_;                        ///< Wallet mutex for thread safety
    std::map<std::string, std::shared_ptr<crypto::KeyPair>> accounts_; ///< Account key pairs
    std::map<std::string, std::shared_ptr<WalletAccount>> account_info_; ///< Account metadata
    std::string default_account_;                           ///< Default account address
    bool is_initialized_;                                   ///< Initialization flag
    
    /**
     * @brief Get key pair for account (with password if encrypted)
     * @param address Account address
     * @param password Wallet password
     * @return Key pair pointer, or nullptr if not found/invalid password
     */
    std::shared_ptr<crypto::KeyPair> getKeyPair(const std::string& address, 
                                                const std::string& password = "") const;
    
    /**
     * @brief Encrypt private key with password
     * @param private_key Private key to encrypt
     * @param password Encryption password
     * @return Encrypted private key data
     */
    nlohmann::json encryptPrivateKey(const std::string& private_key, const std::string& password) const;
    
    /**
     * @brief Decrypt private key with password
     * @param encrypted_data Encrypted key data
     * @param password Decryption password
     * @return Decrypted private key, or empty string if failed
     */
    std::string decryptPrivateKey(const nlohmann::json& encrypted_data, const std::string& password) const;
    
    /**
     * @brief Get wallet file path
     * @return Full path to wallet file
     */
    std::string getWalletFilePath() const;
};

} // namespace wallet
} // namespace deo

