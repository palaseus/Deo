/**
 * @file state_store.cpp
 * @brief Implementation of persistent state store
 * @author Deo Blockchain Team
 * @date 2024
 */

#include "vm/state_store.h"
#include "storage/leveldb_storage.h"
#include "storage/state_storage.h"
#include "utils/logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace deo {
namespace vm {

StateStore::StateStore(const std::string& db_path, const std::string& storage_backend) 
    : db_path_(db_path)
    , storage_backend_(storage_backend)
    , db_(nullptr)
    , initialized_(false)
    , use_leveldb_(storage_backend == "leveldb")
    , in_transaction_(false) {
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "StateStore created with path: " + db_path_ + " backend: " + storage_backend_);
}

StateStore::~StateStore() {
    shutdown();
}

bool StateStore::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }
    
    try {
        // Create database directory if it doesn't exist
        std::filesystem::create_directories(db_path_);
        
        if (use_leveldb_) {
            // Initialize LevelDB storage
            leveldb_storage_ = std::make_shared<storage::LevelDBStateStorage>(db_path_);
            if (!leveldb_storage_->initialize()) {
                DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to initialize LevelDB state storage");
                // Fallback to JSON
                DEO_LOG_WARNING(VIRTUAL_MACHINE, "Falling back to JSON state storage");
                use_leveldb_ = false;
            } else {
                DEO_LOG_INFO(VIRTUAL_MACHINE, "StateStore initialized with LevelDB backend");
                initialized_ = true;
                return true;
            }
        }
        
        // Initialize JSON storage (either as primary or fallback)
        if (!use_leveldb_) {
            std::string db_file = db_path_ + "/state.json";
            
            // Check if database file exists, if not create it
            if (!std::filesystem::exists(db_file)) {
                json empty_db;
                empty_db["accounts"] = json::object();
                empty_db["contracts"] = json::object();
                empty_db["storage"] = json::object();
                empty_db["metadata"] = json::object();
                
                std::ofstream file(db_file);
                file << empty_db.dump(4);
                file.close();
            }
            
            DEO_LOG_INFO(VIRTUAL_MACHINE, "StateStore initialized with JSON backend");
        }
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to initialize StateStore: " + std::string(e.what()));
        return false;
    }
}

void StateStore::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    // Commit any pending transaction
    if (in_transaction_) {
        commitTransaction();
    }
    
    if (use_leveldb_ && leveldb_storage_) {
        leveldb_storage_->shutdown();
        leveldb_storage_.reset();
    }
    
    initialized_ = false;
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "StateStore shutdown");
}

std::shared_ptr<AccountState> StateStore::getAccountState(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return nullptr;
    }
    
    try {
        std::string key = getAccountKey(address);
        std::string data = getValue(key);
        
        if (data.empty()) {
            return nullptr;
        }
        
        AccountState state = deserializeAccountState(data);
        return std::make_shared<AccountState>(state);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to get account state: " + std::string(e.what()));
        return nullptr;
    }
}

bool StateStore::setAccountState(const std::string& address, const AccountState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        if (use_leveldb_ && leveldb_storage_) {
            // Convert vm::AccountState to storage::AccountState
            storage::AccountState storage_state = convertToStorageAccountState(state);
            storage_state.address = address;
            return leveldb_storage_->storeAccount(address, storage_state);
        } else {
            // Use JSON storage
            std::string key = getAccountKey(address);
            std::string data = serializeAccountState(state);
            return setValue(key, data);
        }
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to set account state: " + std::string(e.what()));
        return false;
    }
}

uint256_t StateStore::getBalance(const std::string& address) {
    if (use_leveldb_ && leveldb_storage_) {
        auto account = leveldb_storage_->getAccount(address);
        if (!account) {
            return uint256_t(0);
        }
        // Convert uint64_t to uint256_t
        return uint256_t(account->balance);
    }
    
    auto account = getAccountState(address);
    if (!account) {
        return uint256_t(0);
    }
    return account->balance;
}

bool StateStore::setBalance(const std::string& address, const uint256_t& balance) {
    auto account = getAccountState(address);
    if (!account) {
        // Create new account
        AccountState new_account;
        new_account.balance = balance;
        new_account.nonce = 0;
        new_account.is_contract = false;
        return setAccountState(address, new_account);
    } else {
        account->balance = balance;
        return setAccountState(address, *account);
    }
}

uint64_t StateStore::getNonce(const std::string& address) {
    if (use_leveldb_ && leveldb_storage_) {
        auto account = leveldb_storage_->getAccount(address);
        if (!account) {
            return 0;
        }
        return account->nonce;
    }
    
    auto account = getAccountState(address);
    if (!account) {
        return 0;
    }
    return account->nonce;
}

bool StateStore::setNonce(const std::string& address, uint64_t nonce) {
    auto account = getAccountState(address);
    if (!account) {
        // Create new account
        AccountState new_account;
        new_account.balance = uint256_t(0);
        new_account.nonce = nonce;
        new_account.is_contract = false;
        return setAccountState(address, new_account);
    } else {
        account->nonce = nonce;
        return setAccountState(address, *account);
    }
}

uint64_t StateStore::incrementNonce(const std::string& address) {
    uint64_t current_nonce = getNonce(address);
    uint64_t new_nonce = current_nonce + 1;
    setNonce(address, new_nonce);
    return new_nonce;
}

std::shared_ptr<ContractState> StateStore::getContractState(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return nullptr;
    }
    
    try {
        std::string key = getContractKey(address);
        std::string data = getValue(key);
        
        if (data.empty()) {
            return nullptr;
        }
        
        ContractState state = deserializeContractState(data);
        return std::make_shared<ContractState>(state);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to get contract state: " + std::string(e.what()));
        return nullptr;
    }
}

bool StateStore::setContractState(const std::string& address, const ContractState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        std::string key = getContractKey(address);
        std::string data = serializeContractState(state);
        return setValue(key, data);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to set contract state: " + std::string(e.what()));
        return false;
    }
}

bool StateStore::deployContract(const std::string& address, 
                               const std::vector<uint8_t>& bytecode,
                               const std::string& deployer_address,
                               uint64_t deployment_block) {
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "StateStore::deployContract called for address: " + address);
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Bytecode size: " + std::to_string(bytecode.size()));
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Deployer: " + deployer_address);
    
    ContractState state;
    state.bytecode = bytecode;
    state.storage.clear();
    state.balance = uint256_t(0);
    state.nonce = 0;
    state.is_deployed = true;
    state.deployment_block = deployment_block;
    state.deployer_address = deployer_address;
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Calling setContractState for address: " + address);
    bool result = setContractState(address, state);
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "setContractState result: " + std::string(result ? "true" : "false"));
    
    return result;
}

bool StateStore::contractExists(const std::string& address) {
    return getContractState(address) != nullptr;
}

uint256_t StateStore::getStorageValue(const std::string& contract_address, const uint256_t& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return uint256_t(0);
    }
    
    try {
        std::string db_key = getStorageKey(contract_address, key);
        std::string data = getValue(db_key);
        
        if (data.empty()) {
            return uint256_t(0);
        }
        
        return uint256_t(data);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to get storage value: " + std::string(e.what()));
        return uint256_t(0);
    }
}

bool StateStore::setStorageValue(const std::string& contract_address, 
                                const uint256_t& key, 
                                const uint256_t& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        std::string db_key = getStorageKey(contract_address, key);
        std::string data = value.toString();
        return setValue(db_key, data);
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to set storage value: " + std::string(e.what()));
        return false;
    }
}

void StateStore::beginTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    in_transaction_ = true;
    transaction_cache_.clear();
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Transaction begun");
}

bool StateStore::commitTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!in_transaction_) {
        return true;
    }
    
    try {
        // For now, we'll just clear the transaction cache
        // In a real implementation, this would commit to the database
        transaction_cache_.clear();
        in_transaction_ = false;
        
        DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Transaction committed");
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to commit transaction: " + std::string(e.what()));
        return false;
    }
}

void StateStore::rollbackTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!in_transaction_) {
        return;
    }
    
    transaction_cache_.clear();
    in_transaction_ = false;
    
    DEO_LOG_DEBUG(VIRTUAL_MACHINE, "Transaction rolled back");
}

uint64_t StateStore::getAccountCount() const {
    // This would need to be implemented with proper database iteration
    return 0;
}

uint64_t StateStore::getContractCount() const {
    // This would need to be implemented with proper database iteration
    return 0;
}

uint64_t StateStore::getStorageEntryCount() const {
    // This would need to be implemented with proper database iteration
    return 0;
}

// Private helper methods

std::string StateStore::getValue(const std::string& key) {
    try {
        std::string db_file = db_path_ + "/state.json";
        std::ifstream file(db_file);
        
        if (!file.is_open()) {
            return "";
        }
        
        json db;
        file >> db;
        file.close();
        
        // Navigate through the JSON structure
        if (key.find("account:") == 0) {
            std::string address = key.substr(8);
            if (db["accounts"].contains(address)) {
                return db["accounts"][address].dump();
            }
        } else if (key.find("contract:") == 0) {
            std::string address = key.substr(9);
            if (db["contracts"].contains(address)) {
                return db["contracts"][address].dump();
            }
        } else if (key.find("storage:") == 0) {
            std::string storage_key = key.substr(8);
            if (db["storage"].contains(storage_key)) {
                return db["storage"][storage_key].get<std::string>();
            }
        }
        
        return "";
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to get value: " + std::string(e.what()));
        return "";
    }
}

bool StateStore::setValue(const std::string& key, const std::string& value) {
    try {
        std::string db_file = db_path_ + "/state.json";
        
        // Read existing database
        json db;
        std::ifstream file(db_file);
        if (file.is_open()) {
            file >> db;
            file.close();
        }
        
        // Update the appropriate section
        if (key.find("account:") == 0) {
            std::string address = key.substr(8);
            db["accounts"][address] = json::parse(value);
        } else if (key.find("contract:") == 0) {
            std::string address = key.substr(9);
            db["contracts"][address] = json::parse(value);
        } else if (key.find("storage:") == 0) {
            std::string storage_key = key.substr(8);
            db["storage"][storage_key] = value;
        }
        
        // Write back to file
        std::ofstream out_file(db_file);
        out_file << db.dump(4);
        out_file.close();
        
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to set value: " + std::string(e.what()));
        return false;
    }
}

bool StateStore::deleteValue(const std::string& key) {
    try {
        std::string db_file = db_path_ + "/state.json";
        
        // Read existing database
        json db;
        std::ifstream file(db_file);
        if (file.is_open()) {
            file >> db;
            file.close();
        }
        
        // Delete from the appropriate section
        if (key.find("account:") == 0) {
            std::string address = key.substr(8);
            db["accounts"].erase(address);
        } else if (key.find("contract:") == 0) {
            std::string address = key.substr(9);
            db["contracts"].erase(address);
        } else if (key.find("storage:") == 0) {
            std::string storage_key = key.substr(8);
            db["storage"].erase(storage_key);
        }
        
        // Write back to file
        std::ofstream out_file(db_file);
        out_file << db.dump(4);
        out_file.close();
        
        return true;
        
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(VIRTUAL_MACHINE, "Failed to delete value: " + std::string(e.what()));
        return false;
    }
}

std::string StateStore::getAccountKey(const std::string& address) {
    return "account:" + address;
}

std::string StateStore::getContractKey(const std::string& address) {
    return "contract:" + address;
}

std::string StateStore::getStorageKey(const std::string& contract_address, const uint256_t& storage_key) {
    return "storage:" + contract_address + ":" + storage_key.toString();
}

std::string StateStore::serializeAccountState(const AccountState& state) {
    json j;
    j["balance"] = state.balance.toString();
    j["nonce"] = state.nonce;
    j["is_contract"] = state.is_contract;
    return j.dump();
}

AccountState StateStore::deserializeAccountState(const std::string& data) {
    AccountState state;
    json j = json::parse(data);
    
    state.balance = uint256_t(j["balance"].get<std::string>());
    state.nonce = j["nonce"].get<uint64_t>();
    state.is_contract = j["is_contract"].get<bool>();
    
    return state;
}

std::string StateStore::serializeContractState(const ContractState& state) {
    json j;
    j["bytecode"] = state.bytecode;
    j["balance"] = state.balance.toString();
    j["nonce"] = state.nonce;
    j["is_deployed"] = state.is_deployed;
    j["deployment_block"] = state.deployment_block;
    j["deployer_address"] = state.deployer_address;
    
    // Serialize storage
    json storage_obj = json::object();
    for (const auto& entry : state.storage) {
        storage_obj[entry.first.toString()] = entry.second.toString();
    }
    j["storage"] = storage_obj;
    
    return j.dump();
}

ContractState StateStore::deserializeContractState(const std::string& data) {
    ContractState state;
    json j = json::parse(data);
    
    state.bytecode = j["bytecode"].get<std::vector<uint8_t>>();
    state.balance = uint256_t(j["balance"].get<std::string>());
    state.nonce = j["nonce"].get<uint64_t>();
    state.is_deployed = j["is_deployed"].get<bool>();
    state.deployment_block = j["deployment_block"].get<uint64_t>();
    state.deployer_address = j["deployer_address"].get<std::string>();
    
    // Deserialize storage
    state.storage.clear();
    if (j.contains("storage")) {
        for (const auto& entry : j["storage"].items()) {
            uint256_t key(entry.key());
            uint256_t value(entry.value().get<std::string>());
            state.storage[key] = value;
        }
    }
    
    return state;
}

storage::AccountState StateStore::convertToStorageAccountState(const AccountState& vm_state) const {
    storage::AccountState storage_state;
    storage_state.balance = vm_state.balance.toUint64(); // Convert uint256_t to uint64_t (may lose precision for very large values)
    storage_state.nonce = vm_state.nonce;
    storage_state.code_hash = ""; // Not available in vm::AccountState
    storage_state.last_updated = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return storage_state;
}

AccountState StateStore::convertToVMAccountState(const storage::AccountState& storage_state) const {
    AccountState vm_state;
    vm_state.balance = uint256_t(storage_state.balance); // Convert uint64_t to uint256_t
    vm_state.nonce = storage_state.nonce;
    vm_state.is_contract = !storage_state.code_hash.empty(); // Consider it a contract if code_hash is set
    return vm_state;
}

} // namespace vm
} // namespace deo
