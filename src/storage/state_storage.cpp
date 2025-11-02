/**
 * @file state_storage.cpp
 * @brief State storage system implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "storage/state_storage.h"
#include "utils/logger.h"
#include "utils/error_handler.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <chrono>

namespace deo {
namespace storage {

StateStorage::StateStorage(const std::string& data_directory)
    : data_directory_(data_directory) {
    
    DEO_LOG_DEBUG(STORAGE, "State storage created with directory: " + data_directory);
}

StateStorage::~StateStorage() {
    shutdown();
}

bool StateStorage::initialize() {
    DEO_LOG_INFO(STORAGE, "Initializing state storage");
    
    // Create data directory if it doesn't exist
    if (!std::filesystem::exists(data_directory_)) {
        try {
            std::filesystem::create_directories(data_directory_);
            DEO_LOG_INFO(STORAGE, "Created data directory: " + data_directory_);
        } catch (const std::exception& e) {
            DEO_ERROR(STORAGE, "Failed to create data directory: " + std::string(e.what()));
            return false;
        }
    }
    
    // Load existing state
    if (!loadState()) {
        DEO_LOG_WARNING(STORAGE, "Failed to load existing state");
    }
    
    DEO_LOG_INFO(STORAGE, "State storage initialized successfully");
    return true;
}

void StateStorage::shutdown() {
    DEO_LOG_INFO(STORAGE, "Shutting down state storage");
    
    // Save state to disk
    if (!saveState()) {
        DEO_WARNING(STORAGE, "Failed to save state during shutdown");
    }
    
    DEO_LOG_INFO(STORAGE, "State storage shutdown complete");
}

std::shared_ptr<AccountState> StateStorage::getAccountState(const std::string& address) const {
    DEO_LOG_DEBUG(STORAGE, "Retrieving account state: " + address);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = account_cache_.find(address);
    if (it != account_cache_.end()) {
        return it->second;
    }
    
    DEO_LOG_DEBUG(STORAGE, "Account not found in cache: " + address);
    return nullptr;
}

bool StateStorage::setAccountState(const std::string& address, const AccountState& state) {
    DEO_LOG_DEBUG(STORAGE, "Setting account state: " + address);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Create or update account state
    auto account_state = std::make_shared<AccountState>(state);
    account_cache_[address] = account_state;
    
    // Update index
    updateIndex(address);
    
    DEO_LOG_DEBUG(STORAGE, "Account state set successfully");
    return true;
}

uint64_t StateStorage::getBalance(const std::string& address) const {
    auto account_state = getAccountState(address);
    if (account_state) {
        return account_state->balance;
    }
    return 0;
}

bool StateStorage::setBalance(const std::string& address, uint64_t balance) {
    auto account_state = getAccountState(address);
    if (!account_state) {
        // Create new account state
        AccountState new_state;
        new_state.address = address;
        new_state.balance = balance;
        new_state.nonce = 0;
        new_state.last_updated = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        return setAccountState(address, new_state);
    }
    
    // Update existing account
    account_state->balance = balance;
    account_state->last_updated = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return true;
}

uint64_t StateStorage::getNonce(const std::string& address) const {
    auto account_state = getAccountState(address);
    if (account_state) {
        return account_state->nonce;
    }
    return 0;
}

bool StateStorage::setNonce(const std::string& address, uint64_t nonce) {
    auto account_state = getAccountState(address);
    if (!account_state) {
        // Create new account state
        AccountState new_state;
        new_state.address = address;
        new_state.balance = 0;
        new_state.nonce = nonce;
        new_state.last_updated = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        return setAccountState(address, new_state);
    }
    
    // Update existing account
    account_state->nonce = nonce;
    account_state->last_updated = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return true;
}

uint64_t StateStorage::incrementNonce(const std::string& address) {
    auto account_state = getAccountState(address);
    if (!account_state) {
        // Create new account state
        AccountState new_state;
        new_state.address = address;
        new_state.balance = 0;
        new_state.nonce = 1;
        new_state.last_updated = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        setAccountState(address, new_state);
        return 1;
    }
    
    // Update existing account
    account_state->nonce++;
    account_state->last_updated = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return account_state->nonce;
}

std::string StateStorage::getStorageValue(const std::string& address, const std::string& key) const {
    auto account_state = getAccountState(address);
    if (account_state) {
        auto it = account_state->storage.find(key);
        if (it != account_state->storage.end()) {
            return it->second;
        }
    }
    return "";
}

bool StateStorage::setStorageValue(const std::string& address, const std::string& key, const std::string& value) {
    auto account_state = getAccountState(address);
    if (!account_state) {
        // Create new account state
        AccountState new_state;
        new_state.address = address;
        new_state.balance = 0;
        new_state.nonce = 0;
        new_state.last_updated = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        setAccountState(address, new_state);
        account_state = getAccountState(address);
    }
    
    if (account_state) {
        account_state->storage[key] = value;
        account_state->last_updated = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return true;
    }
    
    return false;
}

std::map<std::string, std::string> StateStorage::getAllStorage(const std::string& address) const {
    auto account_state = getAccountState(address);
    if (account_state) {
        return account_state->storage;
    }
    return {};
}

bool StateStorage::hasAccount(const std::string& address) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return account_cache_.find(address) != account_cache_.end();
}

bool StateStorage::deleteAccount(const std::string& address) {
    DEO_LOG_DEBUG(STORAGE, "Deleting account: " + address);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = account_cache_.find(address);
    if (it != account_cache_.end()) {
        // Remove from index
        removeFromIndex(address);
        
        // Remove from cache
        account_cache_.erase(it);
        
        DEO_LOG_DEBUG(STORAGE, "Account deleted successfully");
        return true;
    }
    
    DEO_LOG_WARNING(STORAGE, "Account not found for deletion: " + address);
    return false;
}

std::vector<std::string> StateStorage::getAllAccounts() const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<std::string> accounts;
    for (const auto& pair : account_cache_) {
        accounts.push_back(pair.first);
    }
    
    return accounts;
}

std::string StateStorage::getStatistics() const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"total_accounts\": " << account_cache_.size() << ",\n";
    ss << "  \"data_directory\": \"" << data_directory_ << "\"\n";
    ss << "}";
    
    return ss.str();
}

bool StateStorage::createSnapshot(const std::string& snapshot_id) {
    DEO_LOG_INFO(STORAGE, "Creating state snapshot: " + snapshot_id);
    
    // Note: In a real implementation, we would create a snapshot
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "State snapshot creation not fully implemented yet");
    
    return true;
}

bool StateStorage::restoreSnapshot(const std::string& snapshot_id) {
    DEO_LOG_INFO(STORAGE, "Restoring state snapshot: " + snapshot_id);
    
    // Note: In a real implementation, we would restore from snapshot
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "State snapshot restore not fully implemented yet");
    
    return true;
}

bool StateStorage::deleteSnapshot(const std::string& snapshot_id) {
    DEO_LOG_INFO(STORAGE, "Deleting state snapshot: " + snapshot_id);
    
    // Note: In a real implementation, we would delete the snapshot
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "State snapshot deletion not fully implemented yet");
    
    return true;
}

std::vector<std::string> StateStorage::getSnapshots() const {
    // Note: In a real implementation, we would return available snapshots
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "Snapshot listing not fully implemented yet");
    
    return {};
}

bool StateStorage::compact() {
    DEO_LOG_INFO(STORAGE, "Compacting state storage");
    
    // Note: In a real implementation, we would compact the storage
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "State storage compaction not fully implemented yet");
    
    return true;
}

bool StateStorage::backup(const std::string& backup_path) const {
    DEO_LOG_INFO(STORAGE, "Backing up state storage to: " + backup_path);
    
    try {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        
        // Create backup directory if it doesn't exist
        std::filesystem::path backup_dir = std::filesystem::path(backup_path).parent_path();
        if (!backup_dir.empty() && !std::filesystem::exists(backup_dir)) {
            std::filesystem::create_directories(backup_dir);
        }
        
        // Serialize all account states to JSON
        nlohmann::json backup_json;
        backup_json["version"] = "1.0";
        backup_json["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        backup_json["accounts"] = nlohmann::json::object();
        
        for (const auto& [address, account] : account_cache_) {
            nlohmann::json account_json;
            account_json["balance"] = account->balance;
            account_json["nonce"] = account->nonce;
            account_json["code_hash"] = account->code_hash;
            account_json["last_updated"] = account->last_updated;
            
            // Serialize storage
            nlohmann::json storage_json;
            for (const auto& [key, value] : account->storage) {
                storage_json[key] = value;
            }
            account_json["storage"] = storage_json;
            
            backup_json["accounts"][address] = account_json;
        }
        
        // Write backup file
        std::ofstream backup_file(backup_path);
        if (!backup_file.is_open()) {
            DEO_LOG_ERROR(STORAGE, "Failed to open backup file: " + backup_path);
            return false;
        }
        
        backup_file << backup_json.dump(2);
        backup_file.close();
        
        DEO_LOG_INFO(STORAGE, "Backup created successfully with " + 
                     std::to_string(account_cache_.size()) + " accounts");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(STORAGE, "Failed to create backup: " + std::string(e.what()));
        return false;
    }
}

bool StateStorage::restore(const std::string& backup_path) {
    DEO_LOG_INFO(STORAGE, "Restoring state storage from: " + backup_path);
    
    try {
        if (!std::filesystem::exists(backup_path)) {
            DEO_LOG_ERROR(STORAGE, "Backup file does not exist: " + backup_path);
            return false;
        }
        
        // Read backup file
        std::ifstream backup_file(backup_path);
        if (!backup_file.is_open()) {
            DEO_LOG_ERROR(STORAGE, "Failed to open backup file: " + backup_path);
            return false;
        }
        
        nlohmann::json backup_json;
        backup_file >> backup_json;
        backup_file.close();
        
        // Clear existing state
        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            account_cache_.clear();
        }
        
        // Restore accounts from backup
        if (!backup_json.contains("accounts") || !backup_json["accounts"].is_object()) {
            DEO_LOG_ERROR(STORAGE, "Invalid backup file format: missing accounts");
            return false;
        }
        
        size_t restored_count = 0;
        for (auto& [address, account_json] : backup_json["accounts"].items()) {
            auto account = std::make_shared<AccountState>();
            account->address = address;
            account->balance = account_json.value("balance", 0ULL);
            account->nonce = account_json.value("nonce", 0ULL);
            account->code_hash = account_json.value("code_hash", "");
            account->last_updated = account_json.value("last_updated", 0ULL);
            
            // Restore storage
            if (account_json.contains("storage") && account_json["storage"].is_object()) {
                for (auto& [key, value] : account_json["storage"].items()) {
                    account->storage[key] = value.get<std::string>();
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(storage_mutex_);
                account_cache_[address] = account;
            }
            
            restored_count++;
        }
        
        DEO_LOG_INFO(STORAGE, "Restored " + std::to_string(restored_count) + " accounts from backup");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(STORAGE, "Failed to restore from backup: " + std::string(e.what()));
        return false;
    }
}

bool StateStorage::loadState() {
    DEO_LOG_DEBUG(STORAGE, "Loading state from disk");
    
    // Note: In a real implementation, we would load state from disk
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "State loading not fully implemented yet");
    
    return true;
}

bool StateStorage::saveState() {
    DEO_LOG_DEBUG(STORAGE, "Saving state to disk");
    
    // Note: In a real implementation, we would save state to disk
    // This is a placeholder implementation
    DEO_LOG_WARNING(STORAGE, "State saving not fully implemented yet");
    
    return true;
}

std::string StateStorage::getAccountFilePath(const std::string& address) const {
    return data_directory_ + "/accounts/" + address + ".account";
}

std::string StateStorage::getStateIndexFilePath() const {
    return data_directory_ + "/state_index.dat";
}

std::string StateStorage::getSnapshotDirectoryPath(const std::string& snapshot_id) const {
    return data_directory_ + "/snapshots/" + snapshot_id;
}

void StateStorage::updateIndex(const std::string& address) {
    // Note: In a real implementation, we would update the state index
    // This is a placeholder implementation
    DEO_LOG_DEBUG(STORAGE, "Updated state index for address: " + address);
}

void StateStorage::removeFromIndex(const std::string& address) {
    // Note: In a real implementation, we would remove from the state index
    // This is a placeholder implementation
    DEO_LOG_DEBUG(STORAGE, "Removed from state index: " + address);
}

} // namespace storage
} // namespace deo
