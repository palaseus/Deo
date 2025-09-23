/**
 * @file state_snapshot_manager.cpp
 * @brief State snapshot manager implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "storage/block_pruning.h"
#include "utils/logger.h"
#include "crypto/hash.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace deo {
namespace storage {

StateSnapshotManager::StateSnapshotManager(std::shared_ptr<LevelDBStateStorage> state_storage)
    : state_storage_(state_storage) {
}

StateSnapshotManager::~StateSnapshotManager() {
    shutdown();
}

bool StateSnapshotManager::initialize(const std::string& snapshot_directory) {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    
    snapshot_directory_ = snapshot_directory;
    
    if (!createSnapshotDirectory()) {
        return false;
    }
    
    return true;
}

void StateSnapshotManager::shutdown() {
    // Nothing to do for shutdown
}

bool StateSnapshotManager::createSnapshot(uint64_t block_height) {
    if (!state_storage_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    
    try {
        std::string snapshot_file = getSnapshotPath(block_height);
        
        nlohmann::json snapshot_data;
        snapshot_data["block_height"] = block_height;
        snapshot_data["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        snapshot_data["version"] = "1.0";
        
        // Get all accounts
        auto account_addresses = state_storage_->getAllAccountAddresses();
        nlohmann::json accounts_json = nlohmann::json::array();
        
        for (const auto& address : account_addresses) {
            auto account = state_storage_->getAccount(address);
            if (account) {
                nlohmann::json account_json;
                account_json["address"] = account->address;
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
                
                accounts_json.push_back(account_json);
            }
        }
        
        snapshot_data["accounts"] = accounts_json;
        
        // Get contract storage
        auto contract_addresses = state_storage_->getContractAddresses();
        nlohmann::json contracts_json = nlohmann::json::array();
        
        for (const auto& contract_address : contract_addresses) {
            nlohmann::json contract_json;
            contract_json["address"] = contract_address;
            
            auto storage = state_storage_->getAllContractStorage(contract_address);
            nlohmann::json storage_json;
            for (const auto& [key, value] : storage) {
                storage_json[key] = value;
            }
            contract_json["storage"] = storage_json;
            
            contracts_json.push_back(contract_json);
        }
        
        snapshot_data["contracts"] = contracts_json;
        
        // Calculate snapshot hash for integrity verification
        std::string snapshot_string = snapshot_data.dump();
        std::string snapshot_hash = deo::crypto::Hash::sha256(snapshot_string);
        snapshot_data["snapshot_hash"] = snapshot_hash;
        
        // Write snapshot to file
        std::ofstream file(snapshot_file);
        if (file.is_open()) {
            file << snapshot_data.dump(2);
            file.close();
            return true;
        }
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return false;
}

bool StateSnapshotManager::restoreFromSnapshot(uint64_t block_height) {
    if (!state_storage_) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    
    try {
        std::string snapshot_file = getSnapshotPath(block_height);
        
        if (!std::filesystem::exists(snapshot_file)) {
            return false;
        }
        
        // Read snapshot file
        std::ifstream file(snapshot_file);
        if (!file.is_open()) {
            return false;
        }
        
        nlohmann::json snapshot_data;
        file >> snapshot_data;
        file.close();
        
        // Verify snapshot integrity
        if (snapshot_data.contains("snapshot_hash")) {
            std::string expected_hash = snapshot_data["snapshot_hash"];
            snapshot_data.erase("snapshot_hash"); // Remove hash before verification
            
            std::string snapshot_string = snapshot_data.dump();
            std::string actual_hash = deo::crypto::Hash::sha256(snapshot_string);
            
            if (actual_hash != expected_hash) {
                return false; // Hash mismatch
            }
        }
        
        // Clear existing state (this is a simplified approach)
        // In practice, you'd want to be more careful about this
        
        // Restore accounts
        if (snapshot_data.contains("accounts")) {
            for (const auto& account_json : snapshot_data["accounts"]) {
                AccountState account;
                account.address = account_json["address"];
                account.balance = account_json["balance"];
                account.nonce = account_json["nonce"];
                account.code_hash = account_json["code_hash"];
                account.last_updated = account_json["last_updated"];
                
                // Restore storage
                if (account_json.contains("storage")) {
                    for (auto it = account_json["storage"].begin(); 
                         it != account_json["storage"].end(); ++it) {
                        account.storage[it.key()] = it.value();
                    }
                }
                
                state_storage_->storeAccount(account.address, account);
            }
        }
        
        // Restore contract storage
        if (snapshot_data.contains("contracts")) {
            for (const auto& contract_json : snapshot_data["contracts"]) {
                std::string contract_address = contract_json["address"];
                
                if (contract_json.contains("storage")) {
                    std::map<std::string, std::string> storage;
                    for (auto it = contract_json["storage"].begin(); 
                         it != contract_json["storage"].end(); ++it) {
                        storage[it.key()] = it.value();
                    }
                    
                    state_storage_->storeContractStorageBatch(contract_address, storage);
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return false;
}

bool StateSnapshotManager::deleteSnapshot(uint64_t block_height) {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    
    try {
        std::string snapshot_file = getSnapshotPath(block_height);
        
        if (std::filesystem::exists(snapshot_file)) {
            return std::filesystem::remove(snapshot_file);
        }
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return false;
}

std::vector<uint64_t> StateSnapshotManager::listSnapshots() const {
    std::vector<uint64_t> snapshots;
    
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    
    try {
        if (std::filesystem::exists(snapshot_directory_)) {
            for (const auto& entry : std::filesystem::directory_iterator(snapshot_directory_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string filename = entry.path().filename().string();
                    if (filename.substr(0, 9) == "snapshot_") {
                        std::string height_str = filename.substr(9, filename.length() - 14); // Remove "snapshot_" and ".json"
                        try {
                            uint64_t height = std::stoull(height_str);
                            snapshots.push_back(height);
                        } catch (const std::exception&) {
                            // Skip invalid filenames
                        }
                    }
                }
            }
        }
        
        std::sort(snapshots.begin(), snapshots.end());
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return snapshots;
}

std::string StateSnapshotManager::getSnapshotInfo(uint64_t block_height) const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    
    try {
        std::string snapshot_file = getSnapshotPath(block_height);
        
        if (!std::filesystem::exists(snapshot_file)) {
            return "{}";
        }
        
        std::ifstream file(snapshot_file);
        if (!file.is_open()) {
            return "{}";
        }
        
        nlohmann::json snapshot_data;
        file >> snapshot_data;
        file.close();
        
        // Create info object with metadata
        nlohmann::json info;
        info["block_height"] = snapshot_data["block_height"];
        info["timestamp"] = snapshot_data["timestamp"];
        info["version"] = snapshot_data.contains("version") ? snapshot_data["version"] : "unknown";
        
        if (snapshot_data.contains("accounts")) {
            info["account_count"] = snapshot_data["accounts"].size();
        }
        
        if (snapshot_data.contains("contracts")) {
            info["contract_count"] = snapshot_data["contracts"].size();
        }
        
        // Get file size
        auto file_size = std::filesystem::file_size(snapshot_file);
        info["file_size_bytes"] = file_size;
        
        return info.dump(2);
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return "{}";
}

uint64_t StateSnapshotManager::cleanupOldSnapshots(uint64_t keep_count) {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    
    uint64_t deleted_count = 0;
    
    try {
        auto snapshots = listSnapshots();
        
        if (snapshots.size() > keep_count) {
            uint64_t to_delete = snapshots.size() - keep_count;
            
            // Delete oldest snapshots
            for (uint64_t i = 0; i < to_delete; ++i) {
                if (deleteSnapshot(snapshots[i])) {
                    deleted_count++;
                }
            }
        }
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return deleted_count;
}

std::string StateSnapshotManager::getSnapshotPath(uint64_t block_height) const {
    return snapshot_directory_ + "/snapshot_" + std::to_string(block_height) + ".json";
}

bool StateSnapshotManager::createSnapshotDirectory() const {
    try {
        std::filesystem::create_directories(snapshot_directory_);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace storage
} // namespace deo
