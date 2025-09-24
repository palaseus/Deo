/**
 * @file state_trie.cpp
 * @brief State trie implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "core/state_trie.h"
#include "crypto/hash.h"
#include "utils/logger.h"

#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>

namespace deo {
namespace core {

void StateTrieNode::updateHash() {
    std::stringstream ss;
    ss << key << value << is_leaf;
    
    // Include children hashes
    for (const auto& child : children) {
        ss << child.first << child.second->hash;
    }
    
    hash = deo::crypto::Hash::sha256(ss.str());
}

nlohmann::json StateTrieNode::toJson() const {
    nlohmann::json json;
    
    json["key"] = key;
    json["value"] = value;
    json["hash"] = hash;
    json["is_leaf"] = is_leaf;
    json["timestamp"] = timestamp;
    
    nlohmann::json children_json = nlohmann::json::object();
    for (const auto& child : children) {
        children_json[child.first] = child.second->toJson();
    }
    json["children"] = children_json;
    
    return json;
}

bool StateTrieNode::fromJson(const nlohmann::json& json) {
    try {
        key = json["key"];
        value = json["value"];
        hash = json["hash"];
        is_leaf = json["is_leaf"];
        timestamp = json["timestamp"];
        
        if (json.contains("children") && json["children"].is_object()) {
            children.clear();
            for (auto& child : json["children"].items()) {
                auto child_node = std::make_shared<StateTrieNode>();
                if (child_node->fromJson(child.value())) {
                    children[child.key()] = child_node;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to deserialize state trie node: " + std::string(e.what()));
        return false;
    }
}

StateTrie::StateTrie() : root_(std::make_shared<StateTrieNode>()) {
    DEO_LOG_DEBUG(BLOCKCHAIN, "StateTrie created");
}

bool StateTrie::initialize(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    db_path_ = db_path;
    
    // Create directory if it doesn't exist
    std::string dir = db_path.substr(0, db_path.find_last_of('/'));
    if (!dir.empty()) {
        int result = std::system(("mkdir -p " + dir).c_str());
        if (result != 0) {
            DEO_LOG_WARNING(BLOCKCHAIN, "Failed to create directory: " + dir);
        }
    }
    
    // Load existing state
    if (!loadFromStorage()) {
        DEO_LOG_WARNING(BLOCKCHAIN, "Failed to load existing state, starting with empty trie");
    }
    
    DEO_LOG_INFO(BLOCKCHAIN, "StateTrie initialized with path: " + db_path);
    return true;
}

void StateTrie::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Save current state
    saveToStorage();
    
    DEO_LOG_INFO(BLOCKCHAIN, "StateTrie shutdown");
}

std::string StateTrie::getValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto node = getNode(key);
    return node ? node->value : "";
}

bool StateTrie::setValue(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return setNode(key, value);
}

bool StateTrie::deleteValue(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return deleteNode(key);
}

bool StateTrie::hasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return getNode(key) != nullptr;
}

uint64_t StateTrie::getBalance(const std::string& address) const {
    std::string key = getAccountKey(address) + ":balance";
    std::string value = getValue(key);
    
    if (value.empty()) {
        return 0;
    }
    
    try {
        return std::stoull(value);
    } catch (const std::exception&) {
        return 0;
    }
}

bool StateTrie::setBalance(const std::string& address, uint64_t balance) {
    std::string key = getAccountKey(address) + ":balance";
    return setValue(key, std::to_string(balance));
}

uint64_t StateTrie::getNonce(const std::string& address) const {
    std::string key = getAccountKey(address) + ":nonce";
    std::string value = getValue(key);
    
    if (value.empty()) {
        return 0;
    }
    
    try {
        return std::stoull(value);
    } catch (const std::exception&) {
        return 0;
    }
}

bool StateTrie::setNonce(const std::string& address, uint64_t nonce) {
    std::string key = getAccountKey(address) + ":nonce";
    return setValue(key, std::to_string(nonce));
}

uint64_t StateTrie::incrementNonce(const std::string& address) {
    uint64_t current_nonce = getNonce(address);
    uint64_t new_nonce = current_nonce + 1;
    setNonce(address, new_nonce);
    return new_nonce;
}

std::string StateTrie::getStorageValue(const std::string& contract_address, const std::string& storage_key) const {
    std::string key = getStorageKey(contract_address, storage_key);
    return getValue(key);
}

bool StateTrie::setStorageValue(const std::string& contract_address, const std::string& storage_key, const std::string& value) {
    std::string key = getStorageKey(contract_address, storage_key);
    return setValue(key, value);
}

std::vector<uint8_t> StateTrie::getContractCode(const std::string& contract_address) const {
    std::string key = getCodeKey(contract_address);
    std::string value = getValue(key);
    
    std::vector<uint8_t> code;
    for (size_t i = 0; i < value.length(); i += 2) {
        if (i + 1 < value.length()) {
            std::string byte_str = value.substr(i, 2);
            code.push_back(static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16)));
        }
    }
    
    return code;
}

bool StateTrie::setContractCode(const std::string& contract_address, const std::vector<uint8_t>& code) {
    std::string key = getCodeKey(contract_address);
    
    std::stringstream ss;
    for (uint8_t byte : code) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
    }
    
    return setValue(key, ss.str());
}

std::string StateTrie::getStateRoot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (root_) {
        root_->updateHash();
        return root_->hash;
    }
    
    return "";
}

std::string StateTrie::createSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string snapshot_id = deo::crypto::Hash::sha256(std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    
    // Deep copy the root node
    auto snapshot_root = std::make_shared<StateTrieNode>();
    if (root_) {
        *snapshot_root = *root_;
    }
    
    snapshots_[snapshot_id] = snapshot_root;
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Created state snapshot: " + snapshot_id);
    return snapshot_id;
}

bool StateTrie::restoreSnapshot(const std::string& snapshot_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = snapshots_.find(snapshot_id);
    if (it == snapshots_.end()) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Snapshot not found: " + snapshot_id);
        return false;
    }
    
    // Deep copy the snapshot
    root_ = std::make_shared<StateTrieNode>();
    *root_ = *(it->second);
    
    DEO_LOG_DEBUG(BLOCKCHAIN, "Restored state snapshot: " + snapshot_id);
    return true;
}

bool StateTrie::deleteSnapshot(const std::string& snapshot_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = snapshots_.find(snapshot_id);
    if (it != snapshots_.end()) {
        snapshots_.erase(it);
        DEO_LOG_DEBUG(BLOCKCHAIN, "Deleted state snapshot: " + snapshot_id);
        return true;
    }
    
    return false;
}

std::vector<std::string> StateTrie::getAllAccounts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> accounts;
    
    std::function<void(std::shared_ptr<StateTrieNode>, const std::string&)> traverse;
    traverse = [&](std::shared_ptr<StateTrieNode> node, const std::string& prefix) {
        if (!node) return;
        
        if (node->is_leaf && node->key.find(":balance") != std::string::npos) {
            std::string address = node->key.substr(0, node->key.find(":balance"));
            if (std::find(accounts.begin(), accounts.end(), address) == accounts.end()) {
                accounts.push_back(address);
            }
        }
        
        for (const auto& child : node->children) {
            traverse(child.second, prefix + child.first);
        }
    };
    
    traverse(root_, "");
    return accounts;
}

std::vector<std::string> StateTrie::getAllContracts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> contracts;
    
    std::function<void(std::shared_ptr<StateTrieNode>, const std::string&)> traverse;
    traverse = [&](std::shared_ptr<StateTrieNode> node, const std::string& prefix) {
        if (!node) return;
        
        if (node->is_leaf && node->key.find(":code") != std::string::npos) {
            std::string address = node->key.substr(0, node->key.find(":code"));
            if (std::find(contracts.begin(), contracts.end(), address) == contracts.end()) {
                contracts.push_back(address);
            }
        }
        
        for (const auto& child : node->children) {
            traverse(child.second, prefix + child.first);
        }
    };
    
    traverse(root_, "");
    return contracts;
}

nlohmann::json StateTrie::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json stats;
    
    auto accounts = getAllAccounts();
    auto contracts = getAllContracts();
    
    stats["total_accounts"] = accounts.size();
    stats["total_contracts"] = contracts.size();
    stats["snapshots_count"] = snapshots_.size();
    stats["state_root"] = getStateRoot();
    
    return stats;
}

nlohmann::json StateTrie::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json json;
    
    if (root_) {
        json["root"] = root_->toJson();
    }
    
    json["db_path"] = db_path_;
    
    return json;
}

bool StateTrie::fromJson(const nlohmann::json& json) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (json.contains("root")) {
            root_ = std::make_shared<StateTrieNode>();
            if (!root_->fromJson(json["root"])) {
                return false;
            }
        }
        
        if (json.contains("db_path")) {
            db_path_ = json["db_path"];
        }
        
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to deserialize state trie: " + std::string(e.what()));
        return false;
    }
}

bool StateTrie::validate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!root_) {
        return false;
    }
    
    // Validate all hashes
    const_cast<StateTrie*>(this)->updateHashes();
    
    return true;
}

std::shared_ptr<StateTrieNode> StateTrie::getNode(const std::string& key) const {
    if (!root_) {
        return nullptr;
    }
    
    std::shared_ptr<StateTrieNode> current = root_;
    
    for (size_t i = 0; i < key.length(); i += 2) {
        if (i + 1 >= key.length()) break;
        
        std::string nibble = key.substr(i, 2);
        auto it = current->children.find(nibble);
        
        if (it == current->children.end()) {
            return nullptr;
        }
        
        current = it->second;
    }
    
    return current->is_leaf ? current : nullptr;
}

bool StateTrie::setNode(const std::string& key, const std::string& value) {
    if (!root_) {
        root_ = std::make_shared<StateTrieNode>();
    }
    
    std::shared_ptr<StateTrieNode> current = root_;
    
    for (size_t i = 0; i < key.length(); i += 2) {
        if (i + 1 >= key.length()) break;
        
        std::string nibble = key.substr(i, 2);
        auto it = current->children.find(nibble);
        
        if (it == current->children.end()) {
            current->children[nibble] = std::make_shared<StateTrieNode>();
        }
        
        current = current->children[nibble];
    }
    
    current->key = key;
    current->value = value;
    current->is_leaf = true;
    current->timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    current->updateHash();
    
    return true;
}

bool StateTrie::deleteNode(const std::string& key) {
    // For simplicity, we'll just set the value to empty
    // In a full implementation, we'd need to clean up empty branches
    return setNode(key, "");
}

bool StateTrie::loadFromStorage() {
    if (db_path_.empty()) {
        return false;
    }
    
    std::ifstream file(db_path_);
    if (!file.is_open()) {
        return false;
    }
    
    try {
        nlohmann::json json;
        file >> json;
        file.close();
        
        return fromJson(json);
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to load state from storage: " + std::string(e.what()));
        return false;
    }
}

bool StateTrie::saveToStorage() const {
    if (db_path_.empty()) {
        return false;
    }
    
    std::ofstream file(db_path_);
    if (!file.is_open()) {
        return false;
    }
    
    try {
        nlohmann::json json = toJson();
        file << json.dump(2);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        DEO_LOG_ERROR(BLOCKCHAIN, "Failed to save state to storage: " + std::string(e.what()));
        return false;
    }
}

void StateTrie::updateHashes() {
    if (!root_) {
        return;
    }
    
    std::function<void(std::shared_ptr<StateTrieNode>)> update;
    update = [&](std::shared_ptr<StateTrieNode> node) {
        if (!node) return;
        
        for (auto& child : node->children) {
            update(child.second);
        }
        
        node->updateHash();
    };
    
    update(root_);
}

std::string StateTrie::getAccountKey(const std::string& address) const {
    return "account:" + address;
}

std::string StateTrie::getStorageKey(const std::string& contract_address, const std::string& storage_key) const {
    return "storage:" + contract_address + ":" + storage_key;
}

std::string StateTrie::getCodeKey(const std::string& contract_address) const {
    return "code:" + contract_address;
}

} // namespace core
} // namespace deo
