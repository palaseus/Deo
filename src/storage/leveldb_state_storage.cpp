/**
 * @file leveldb_state_storage.cpp
 * @brief LevelDB-based state storage implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "storage/leveldb_storage.h"
#include "utils/logger.h"
#include "crypto/hash.h"
#include <nlohmann/json.hpp>
#include <set>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace deo {
namespace storage {

LevelDBStateStorage::LevelDBStateStorage(const std::string& data_directory)
    : data_directory_(data_directory) {
    initializeOptions();
}

LevelDBStateStorage::~LevelDBStateStorage() {
    shutdown();
}

bool LevelDBStateStorage::initialize() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Check if already initialized
    if (db_) {
        return true;
    }
    
    if (!createDataDirectory()) {
        return false;
    }
    
    std::string db_path = data_directory_ + "/state";
    
    // Open the database
    leveldb::DB* db_ptr = nullptr;
    leveldb::Status status = leveldb::DB::Open(db_options_, db_path, &db_ptr);
    if (status.ok()) {
        db_.reset(db_ptr);
        return true;
    } else {
        return false;
    }
}

void LevelDBStateStorage::shutdown() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (db_) {
        db_.reset();
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "LevelDB state storage shutdown");
}

bool LevelDBStateStorage::storeAccount(const std::string& address, const AccountState& account) {
    if (address.empty() || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string account_key = createAccountKey(address);
        std::string serialized_account = serializeAccount(account);
        
        // Check if this is a new account
        std::string existing_data;
        bool is_new_account = !db_->Get(read_options_, account_key, &existing_data).ok();
        
        leveldb::Status status = db_->Put(write_options_, account_key, serialized_account);
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to store account: " + status.ToString());
            return false;
        }
        
        // Update account count if this is a new account
        if (is_new_account) {
            updateAccountCount(1);
        }
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Stored account: " + address);
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while storing account: " + std::string(e.what()));
        return false;
    }
}

std::shared_ptr<AccountState> LevelDBStateStorage::getAccount(const std::string& address) {
    if (address.empty() || !db_) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string account_key = createAccountKey(address);
        std::string serialized_account;
        
        leveldb::Status status = db_->Get(read_options_, account_key, &serialized_account);
        if (!status.ok()) {
            if (!status.IsNotFound()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get account: " + status.ToString());
            }
            return nullptr;
        }
        
        return deserializeAccount(serialized_account);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting account: " + std::string(e.what()));
        return nullptr;
    }
}

bool LevelDBStateStorage::hasAccount(const std::string& address) {
    if (address.empty() || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string account_key = createAccountKey(address);
        std::string value;
        
        leveldb::Status status = db_->Get(read_options_, account_key, &value);
        return status.ok();
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while checking account existence: " + std::string(e.what()));
        return false;
    }
}

bool LevelDBStateStorage::deleteAccount(const std::string& address) {
    if (address.empty() || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string account_key = createAccountKey(address);
        
        // Check if account exists before deleting
        std::string existing_data;
        bool account_exists = db_->Get(read_options_, account_key, &existing_data).ok();
        
        leveldb::Status status = db_->Delete(write_options_, account_key);
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to delete account: " + status.ToString());
            return false;
        }
        
        // Update account count if account existed
        if (account_exists) {
            updateAccountCount(-1);
        }
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Deleted account: " + address);
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while deleting account: " + std::string(e.what()));
        return false;
    }
}

bool LevelDBStateStorage::storeContractStorage(const std::string& contract_address, const std::string& key, const std::string& value) {
    if (contract_address.empty() || key.empty() || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string storage_key = createStorageKey(contract_address, key);
        
        // Check if this is a new storage entry
        std::string existing_data;
        bool is_new_entry = !db_->Get(read_options_, storage_key, &existing_data).ok();
        
        leveldb::Status status = db_->Put(write_options_, storage_key, value);
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to store contract storage: " + status.ToString());
            return false;
        }
        
        // Update storage entry count if this is a new entry
        if (is_new_entry) {
            updateStorageEntryCount(1);
        }
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Stored contract storage: " + contract_address + "[" + key + "] = " + value);
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while storing contract storage: " + std::string(e.what()));
        return false;
    }
}

std::string LevelDBStateStorage::getContractStorage(const std::string& contract_address, const std::string& key) {
    if (contract_address.empty() || key.empty() || !db_) {
        return "";
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string storage_key = createStorageKey(contract_address, key);
        std::string value;
        
        leveldb::Status status = db_->Get(read_options_, storage_key, &value);
        if (!status.ok()) {
            if (!status.IsNotFound()) {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to get contract storage: " + status.ToString());
            }
            return "";
        }
        
        return value;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting contract storage: " + std::string(e.what()));
        return "";
    }
}

bool LevelDBStateStorage::hasContractStorage(const std::string& contract_address, const std::string& key) {
    if (contract_address.empty() || key.empty() || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string storage_key = createStorageKey(contract_address, key);
        std::string value;
        
        leveldb::Status status = db_->Get(read_options_, storage_key, &value);
        return status.ok();
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while checking contract storage existence: " + std::string(e.what()));
        return false;
    }
}

bool LevelDBStateStorage::deleteContractStorage(const std::string& contract_address, const std::string& key) {
    if (contract_address.empty() || key.empty() || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string storage_key = createStorageKey(contract_address, key);
        
        // Check if storage entry exists before deleting
        std::string existing_data;
        bool entry_exists = db_->Get(read_options_, storage_key, &existing_data).ok();
        
        leveldb::Status status = db_->Delete(write_options_, storage_key);
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to delete contract storage: " + status.ToString());
            return false;
        }
        
        // Update storage entry count if entry existed
        if (entry_exists) {
            updateStorageEntryCount(-1);
        }
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Deleted contract storage: " + contract_address + "[" + key + "]");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while deleting contract storage: " + std::string(e.what()));
        return false;
    }
}

bool LevelDBStateStorage::storeAccountBatch(const std::unordered_map<std::string, AccountState>& accounts) {
    if (accounts.empty() || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        leveldb::WriteBatch batch;
        int64_t new_accounts = 0;
        
        for (const auto& [address, account] : accounts) {
            std::string account_key = createAccountKey(address);
            std::string serialized_account = serializeAccount(account);
            
            // Check if this is a new account
            std::string existing_data;
            if (!db_->Get(read_options_, account_key, &existing_data).ok()) {
                new_accounts++;
            }
            
            batch.Put(account_key, serialized_account);
        }
        
        leveldb::Status status = db_->Write(write_options_, &batch);
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to store account batch: " + status.ToString());
            return false;
        }
        
        // Update account count
        if (new_accounts > 0) {
            updateAccountCount(new_accounts);
        }
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Stored " + std::to_string(accounts.size()) + " accounts in batch");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while storing account batch: " + std::string(e.what()));
        return false;
    }
}

bool LevelDBStateStorage::storeContractStorageBatch(const std::string& contract_address, const std::map<std::string, std::string>& storage) {
    if (contract_address.empty() || storage.empty() || !db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        leveldb::WriteBatch batch;
        int64_t new_entries = 0;
        
        for (const auto& [key, value] : storage) {
            std::string storage_key = createStorageKey(contract_address, key);
            
            // Check if this is a new storage entry
            std::string existing_data;
            if (!db_->Get(read_options_, storage_key, &existing_data).ok()) {
                new_entries++;
            }
            
            batch.Put(storage_key, value);
        }
        
        leveldb::Status status = db_->Write(write_options_, &batch);
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to store contract storage batch: " + status.ToString());
            return false;
        }
        
        // Update storage entry count
        if (new_entries > 0) {
            updateStorageEntryCount(new_entries);
        }
        
        DEO_LOG_DEBUG(BLOCKCHAIN, "Stored " + std::to_string(storage.size()) + " storage entries for contract: " + contract_address);
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while storing contract storage batch: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> LevelDBStateStorage::getAllAccountAddresses() {
    std::vector<std::string> addresses;
    
    if (!db_) {
        return addresses;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(read_options_));
        
        for (it->Seek(ACCOUNT_PREFIX); it->Valid() && it->key().starts_with(ACCOUNT_PREFIX); it->Next()) {
            std::string key = it->key().ToString();
            std::string address = key.substr(strlen(ACCOUNT_PREFIX));
            addresses.push_back(address);
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting all account addresses: " + std::string(e.what()));
    }
    
    return addresses;
}

std::vector<std::string> LevelDBStateStorage::getContractAddresses() {
    std::vector<std::string> addresses;
    
    if (!db_) {
        return addresses;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(read_options_));
        std::set<std::string> unique_addresses;
        
        for (it->Seek(STORAGE_PREFIX); it->Valid() && it->key().starts_with(STORAGE_PREFIX); it->Next()) {
            std::string key = it->key().ToString();
            // Extract contract address from storage key format: "storage:contract_address:key"
            size_t first_colon = key.find(':', strlen(STORAGE_PREFIX));
            if (first_colon != std::string::npos) {
                size_t second_colon = key.find(':', first_colon + 1);
                if (second_colon != std::string::npos) {
                    std::string contract_address = key.substr(first_colon + 1, second_colon - first_colon - 1);
                    unique_addresses.insert(contract_address);
                }
            }
        }
        
        addresses.assign(unique_addresses.begin(), unique_addresses.end());
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting contract addresses: " + std::string(e.what()));
    }
    
    return addresses;
}

std::map<std::string, std::string> LevelDBStateStorage::getAllContractStorage(const std::string& contract_address) {
    std::map<std::string, std::string> storage;
    
    if (contract_address.empty() || !db_) {
        return storage;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string prefix = std::string(STORAGE_PREFIX) + contract_address + ":";
        std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(read_options_));
        
        for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
            std::string key = it->key().ToString();
            std::string storage_key = key.substr(prefix.length());
            std::string value = it->value().ToString();
            storage[storage_key] = value;
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting all contract storage: " + std::string(e.what()));
    }
    
    return storage;
}

uint64_t LevelDBStateStorage::getAccountCount() {
    if (!db_) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string count_key = std::string(COUNT_PREFIX) + "accounts";
        std::string count_str;
        leveldb::Status status = db_->Get(read_options_, count_key, &count_str);
        if (!status.ok()) {
            return 0;
        }
        
        return std::stoull(count_str);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting account count: " + std::string(e.what()));
        return 0;
    }
}

uint64_t LevelDBStateStorage::getContractCount() {
    if (!db_) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string count_key = std::string(COUNT_PREFIX) + "contracts";
        std::string count_str;
        leveldb::Status status = db_->Get(read_options_, count_key, &count_str);
        if (!status.ok()) {
            return 0;
        }
        
        return std::stoull(count_str);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting contract count: " + std::string(e.what()));
        return 0;
    }
}

uint64_t LevelDBStateStorage::getStorageEntryCount() {
    if (!db_) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string count_key = std::string(COUNT_PREFIX) + "storage_entries";
        std::string count_str;
        leveldb::Status status = db_->Get(read_options_, count_key, &count_str);
        if (!status.ok()) {
            return 0;
        }
        
        return std::stoull(count_str);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting storage entry count: " + std::string(e.what()));
        return 0;
    }
}

std::string LevelDBStateStorage::getStatistics() const {
    if (!db_) {
        return "{}";
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        nlohmann::json stats;
        stats["account_count"] = const_cast<LevelDBStateStorage*>(this)->getAccountCount();
        stats["contract_count"] = const_cast<LevelDBStateStorage*>(this)->getContractCount();
        stats["storage_entry_count"] = const_cast<LevelDBStateStorage*>(this)->getStorageEntryCount();
        stats["data_directory"] = data_directory_;
        stats["database_size"] = "N/A"; // LevelDB doesn't provide direct size info
        
        return stats.dump(2);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting statistics: " + std::string(e.what()));
        return "{}";
    }
}

bool LevelDBStateStorage::compactDatabase() {
    if (!db_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        db_->CompactRange(nullptr, nullptr);
        DEO_LOG_INFO(BLOCKCHAIN, "State database compaction completed");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception during state database compaction: " + std::string(e.what()));
        return false;
    }
}

bool LevelDBStateStorage::repairDatabase() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        std::string db_path = data_directory_ + "/state";
        leveldb::Status status = leveldb::RepairDB(db_path, db_options_);
        
        if (!status.ok()) {
            DEO_LOG_ERROR(BLOCKCHAIN, "Failed to repair state database: " + status.ToString());
            return false;
        }
        
        DEO_LOG_INFO(BLOCKCHAIN, "State database repair completed");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception during state database repair: " + std::string(e.what()));
        return false;
    }
}

std::string LevelDBStateStorage::createAccountKey(const std::string& address) {
    return std::string(ACCOUNT_PREFIX) + address;
}

std::string LevelDBStateStorage::createStorageKey(const std::string& contract_address, const std::string& key) {
    return std::string(STORAGE_PREFIX) + contract_address + ":" + key;
}

std::string LevelDBStateStorage::createContractKey(const std::string& contract_address) {
    return std::string(CONTRACT_PREFIX) + contract_address;
}

std::string LevelDBStateStorage::serializeAccount(const AccountState& account) {
    try {
        nlohmann::json account_json;
        account_json["address"] = account.address;
        account_json["balance"] = account.balance;
        account_json["nonce"] = account.nonce;
        account_json["code_hash"] = account.code_hash;
        account_json["last_updated"] = account.last_updated;
        
        // Serialize storage
        nlohmann::json storage_json;
        for (const auto& [key, value] : account.storage) {
            storage_json[key] = value;
        }
        account_json["storage"] = storage_json;
        
        return account_json.dump();
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while serializing account: " + std::string(e.what()));
        return "";
    }
}

std::shared_ptr<AccountState> LevelDBStateStorage::deserializeAccount(const std::string& data) {
    try {
        nlohmann::json account_json = nlohmann::json::parse(data);
        
        auto account = std::make_shared<AccountState>();
        account->address = account_json["address"];
        account->balance = account_json["balance"];
        account->nonce = account_json["nonce"];
        account->code_hash = account_json["code_hash"];
        account->last_updated = account_json["last_updated"];
        
        // Deserialize storage
        if (account_json.contains("storage")) {
            const auto& storage_json = account_json["storage"];
            for (auto it = storage_json.begin(); it != storage_json.end(); ++it) {
                account->storage[it.key()] = it.value();
            }
        }
        
        return account;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while deserializing account: " + std::string(e.what()));
        return nullptr;
    }
}

void LevelDBStateStorage::initializeOptions() {
    // Create cache for better performance
    state_cache_.reset(leveldb::NewLRUCache(50 * 1024 * 1024)); // 50MB cache
    
    // Create filter policy for better performance
    filter_policy_.reset(leveldb::NewBloomFilterPolicy(10));
    
    // Configure database options
    db_options_.create_if_missing = true;
    db_options_.error_if_exists = false;
    db_options_.paranoid_checks = true;
    db_options_.write_buffer_size = 32 * 1024 * 1024; // 32MB write buffer
    db_options_.max_open_files = 1000;
    db_options_.block_size = 4096;
    db_options_.block_restart_interval = 16;
    db_options_.max_file_size = 2 * 1024 * 1024; // 2MB max file size
    db_options_.compression = leveldb::kSnappyCompression;
    db_options_.block_cache = state_cache_.get();
    db_options_.filter_policy = filter_policy_.get();
    
    // Configure read options
    read_options_.verify_checksums = true;
    read_options_.fill_cache = true;
    
    // Configure write options
    write_options_.sync = false; // For better performance, set to true for durability
}

bool LevelDBStateStorage::createDataDirectory() {
    try {
        std::filesystem::path dir_path(data_directory_);
        if (!std::filesystem::exists(dir_path)) {
            std::filesystem::create_directories(dir_path);
        }
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to create data directory: " + std::string(e.what()));
        return false;
    }
}

void LevelDBStateStorage::updateAccountCount(int64_t delta) {
    if (!db_) {
        return;
    }
    
    try {
        std::string count_key = std::string(COUNT_PREFIX) + "accounts";
        std::string count_str;
        
        leveldb::Status status = db_->Get(read_options_, count_key, &count_str);
        uint64_t current_count = 0;
        
        if (status.ok()) {
            current_count = std::stoull(count_str);
        }
        
        uint64_t new_count = (delta >= 0) ? current_count + delta : 
                           (current_count >= static_cast<uint64_t>(-delta)) ? current_count + delta : 0;
        
        db_->Put(write_options_, count_key, std::to_string(new_count));
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while updating account count: " + std::string(e.what()));
    }
}

void LevelDBStateStorage::updateContractCount(int64_t delta) {
    if (!db_) {
        return;
    }
    
    try {
        std::string count_key = std::string(COUNT_PREFIX) + "contracts";
        std::string count_str;
        
        leveldb::Status status = db_->Get(read_options_, count_key, &count_str);
        uint64_t current_count = 0;
        
        if (status.ok()) {
            current_count = std::stoull(count_str);
        }
        
        uint64_t new_count = (delta >= 0) ? current_count + delta : 
                           (current_count >= static_cast<uint64_t>(-delta)) ? current_count + delta : 0;
        
        db_->Put(write_options_, count_key, std::to_string(new_count));
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while updating contract count: " + std::string(e.what()));
    }
}

void LevelDBStateStorage::updateStorageEntryCount(int64_t delta) {
    if (!db_) {
        return;
    }
    
    try {
        std::string count_key = std::string(COUNT_PREFIX) + "storage_entries";
        std::string count_str;
        
        leveldb::Status status = db_->Get(read_options_, count_key, &count_str);
        uint64_t current_count = 0;
        
        if (status.ok()) {
            current_count = std::stoull(count_str);
        }
        
        uint64_t new_count = (delta >= 0) ? current_count + delta : 
                           (current_count >= static_cast<uint64_t>(-delta)) ? current_count + delta : 0;
        
        db_->Put(write_options_, count_key, std::to_string(new_count));
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while updating storage entry count: " + std::string(e.what()));
    }
}

uint64_t LevelDBStateStorage::pruneState(uint64_t keep_blocks, uint64_t /* current_height */, 
                                         const std::set<std::string>& recent_accounts) {
    if (!db_ || keep_blocks == 0) {
        // Don't prune if keep_blocks is 0 (keep all state)
        return 0;
    }
    
    uint64_t accounts_pruned = 0;
    
    try {
        // Get all account addresses without holding the lock for the entire pruning operation
        std::vector<std::string> account_addresses;
        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            if (db_) {
                try {
                    std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(read_options_));
                    for (it->Seek(ACCOUNT_PREFIX); it->Valid() && it->key().starts_with(ACCOUNT_PREFIX); it->Next()) {
                        std::string key = it->key().ToString();
                        std::string address = key.substr(strlen(ACCOUNT_PREFIX));
                        account_addresses.push_back(address);
                    }
                } catch (const std::exception& e) {
                    DEO_LOG_ERROR(BLOCKCHAIN, "Exception while getting account addresses for pruning: " + std::string(e.what()));
                    return 0;
                }
            }
        }
        
        leveldb::WriteBatch batch;
        
        // Process accounts and build batch (this can call getAccount which uses its own lock)
        for (const auto& address : account_addresses) {
            auto account = getAccount(address);
            if (!account) {
                continue;
            }
            
            // Check if this account is in the recent blocks (must keep)
            if (recent_accounts.find(address) != recent_accounts.end()) {
                DEO_LOG_DEBUG(BLOCKCHAIN, "Keeping account (in recent blocks): " + address);
                continue;
            }
            
            // Only prune accounts that are safe to remove:
            // 1. Not referenced in recent blocks (already checked above)
            // 2. Have zero balance
            // 3. Are not contracts (no code_hash or empty code_hash)
            // 4. Have no contract storage
            // 5. Have zero nonce (no recent activity)
            
            bool is_contract = !account->code_hash.empty();
            bool has_storage = !account->storage.empty();
            bool has_balance = account->balance > 0;
            bool has_nonce = account->nonce > 0;
            
            // Prune if account is truly empty and not in recent blocks
            bool can_prune = !has_balance && !is_contract && !has_storage && !has_nonce;
            
            if (can_prune) {
                // Safe to prune this account
                std::string account_key = createAccountKey(address);
                batch.Delete(account_key);
                updateAccountCount(-1);
                accounts_pruned++;
                
                DEO_LOG_DEBUG(BLOCKCHAIN, "Pruning account: " + address + 
                             " (empty, not in recent " + std::to_string(keep_blocks) + " blocks)");
            } else {
                DEO_LOG_DEBUG(BLOCKCHAIN, "Preserving account: " + address + 
                             " (balance=" + std::to_string(account->balance) +
                             ", contract=" + std::string(is_contract ? "yes" : "no") +
                             ", storage=" + std::to_string(account->storage.size()) +
                             ", nonce=" + std::to_string(account->nonce) + ")");
            }
        }
        
        // Apply batch deletion
        if (accounts_pruned > 0) {
            leveldb::Status status = db_->Write(write_options_, &batch);
            if (status.ok()) {
                DEO_LOG_INFO(BLOCKCHAIN, "Pruned " + std::to_string(accounts_pruned) + 
                            " accounts (preserving state for " + std::to_string(keep_blocks) + 
                            " blocks, " + std::to_string(recent_accounts.size()) + " recent accounts kept)");
            } else {
                DEO_LOG_ERROR(BLOCKCHAIN, "Failed to prune state: " + status.ToString());
                accounts_pruned = 0;
            }
        } else {
            DEO_LOG_DEBUG(BLOCKCHAIN, "No accounts pruned (all accounts preserved or not empty)");
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Exception while pruning state: " + std::string(e.what()));
        accounts_pruned = 0;
    }
    
    return accounts_pruned;
}

} // namespace storage
} // namespace deo
