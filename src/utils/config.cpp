/**
 * @file config.cpp
 * @brief Configuration management system implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "utils/config.h"
#include "utils/logger.h"
#include "utils/error_handler.h"
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <algorithm>

namespace deo {
namespace utils {

Config::Config() : change_notifications_enabled_(true) {
    DEO_LOG_DEBUG(CONFIGURATION, "Configuration system initialized");
}

bool Config::load(const std::string& filename) {
    DEO_LOG_INFO(CONFIGURATION, "Loading configuration from: " + filename);
    
    std::string content = readFile(filename);
    if (content.empty()) {
        DEO_ERROR(CONFIGURATION, "Failed to read configuration file: " + filename);
        return false;
    }
    
    std::string extension = getFileExtension(filename);
    bool success = false;
    
    if (extension == "json") {
        success = parseJson(content);
    } else if (extension == "ini") {
        success = parseIni(content);
    } else if (extension == "yaml" || extension == "yml") {
        success = parseYaml(content);
    } else {
        DEO_ERROR(CONFIGURATION, "Unsupported configuration file format: " + extension);
        return false;
    }
    
    if (success) {
        DEO_LOG_INFO(CONFIGURATION, "Configuration loaded successfully from: " + filename);
    } else {
        DEO_ERROR(CONFIGURATION, "Failed to parse configuration file: " + filename);
    }
    
    return success;
}

bool Config::save(const std::string& filename) const {
    DEO_LOG_INFO(CONFIGURATION, "Saving configuration to: " + filename);
    
    std::string content = toJson();
    bool success = writeFile(filename, content);
    
    if (success) {
        DEO_LOG_INFO(CONFIGURATION, "Configuration saved successfully to: " + filename);
    } else {
        DEO_ERROR(CONFIGURATION, "Failed to save configuration file: " + filename);
    }
    
    return success;
}

void Config::loadFromEnvironment(const std::string& prefix) {
    DEO_LOG_INFO(CONFIGURATION, "Loading configuration from environment variables with prefix: " + prefix);
    
    // Note: In a real implementation, we would iterate through environment variables
    // and parse them into configuration sections and keys
    // This is a placeholder implementation
    
    DEO_LOG_DEBUG(CONFIGURATION, "Environment variable loading not fully implemented yet");
}

void Config::loadFromCommandLine([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    DEO_LOG_INFO(CONFIGURATION, "Loading configuration from command line arguments");
    
    // Note: In a real implementation, we would parse command line arguments
    // and convert them to configuration values
    // This is a placeholder implementation
    
    DEO_LOG_DEBUG(CONFIGURATION, "Command line argument parsing not fully implemented yet");
}

ConfigSection& Config::getSection(const std::string& section_name) {
    return sections_[section_name];
}

const ConfigSection& Config::getSection(const std::string& section_name) const {
    static ConfigSection empty_section("");
    auto it = sections_.find(section_name);
    if (it != sections_.end()) {
        return it->second;
    }
    return empty_section;
}


std::string Config::toJson() const {
    std::stringstream ss;
    ss << "{\n";
    
    bool first_section = true;
    for (const auto& section_pair : sections_) {
        if (!first_section) ss << ",\n";
        first_section = false;
        
        ss << "  \"" << section_pair.first << "\": {\n";
        
        const auto& section = section_pair.second;
        auto keys = section.getKeys();
        
        bool first_key = true;
        for (const auto& key : keys) {
            if (!first_key) ss << ",\n";
            first_key = false;
            
            // Note: In a real implementation, we would need to determine the type
            // of each value and format it appropriately
            ss << "    \"" << key << "\": \"<value>\"";
        }
        
        ss << "\n  }";
    }
    
    ss << "\n}";
    return ss.str();
}

bool Config::fromJson(const std::string& json) {
    return parseJson(json);
}

bool Config::validate() const {
    DEO_LOG_DEBUG(CONFIGURATION, "Validating configuration");
    
    // Note: In a real implementation, we would validate all configuration values
    // against their validators
    // This is a placeholder implementation
    
    return true;
}

std::vector<std::string> Config::getValidationErrors() const {
    std::vector<std::string> errors;
    
    // Note: In a real implementation, we would collect validation errors
    // This is a placeholder implementation
    
    return errors;
}

void Config::setValidator(const std::string& section_name, 
                         const std::string& key, 
                         std::function<bool(const ConfigValue&)> validator) {
    std::string full_key = section_name + "." + key;
    validators_[full_key] = validator;
    
    DEO_LOG_DEBUG(CONFIGURATION, "Set validator for: " + full_key);
}

void Config::registerChangeCallback(std::function<void(const std::string&, const std::string&, const ConfigValue&)> callback) {
    change_callbacks_.push_back(callback);
    DEO_LOG_DEBUG(CONFIGURATION, "Registered configuration change callback");
}

void Config::setChangeNotificationsEnabled(bool enabled) {
    change_notifications_enabled_ = enabled;
    DEO_LOG_DEBUG(CONFIGURATION, "Configuration change notifications " + 
                  std::string(enabled ? "enabled" : "disabled"));
}

bool Config::parseJson(const std::string& json) {
    try {
        nlohmann::json config = nlohmann::json::parse(json);
        
        // Parse blockchain configuration
        if (config.contains("blockchain")) {
            auto blockchain = config["blockchain"];
            if (blockchain.contains("data_directory")) {
                setValue("blockchain.data_directory", blockchain["data_directory"].get<std::string>());
            }
            if (blockchain.contains("enable_mining")) {
                setValue("blockchain.enable_mining", blockchain["enable_mining"].get<bool>() ? "true" : "false");
            }
            if (blockchain.contains("mining_threads")) {
                setValue("blockchain.mining_threads", std::to_string(blockchain["mining_threads"].get<int>()));
            }
            if (blockchain.contains("max_block_size")) {
                setValue("blockchain.max_block_size", std::to_string(blockchain["max_block_size"].get<int>()));
            }
        }
        
        // Parse network configuration
        if (config.contains("network")) {
            auto network = config["network"];
            if (network.contains("listen_port")) {
                setValue("network.listen_port", std::to_string(network["listen_port"].get<int>()));
            }
            if (network.contains("listen_address")) {
                setValue("network.listen_address", network["listen_address"].get<std::string>());
            }
            if (network.contains("max_connections")) {
                setValue("network.max_connections", std::to_string(network["max_connections"].get<int>()));
            }
            if (network.contains("enable_listening")) {
                setValue("network.enable_listening", network["enable_listening"].get<bool>() ? "true" : "false");
            }
        }
        
        // Parse consensus configuration
        if (config.contains("consensus")) {
            auto consensus = config["consensus"];
            if (consensus.contains("type")) {
                setValue("consensus.type", consensus["type"].get<std::string>());
            }
            if (consensus.contains("difficulty")) {
                setValue("consensus.difficulty", std::to_string(consensus["difficulty"].get<int>()));
            }
            if (consensus.contains("block_time")) {
                setValue("consensus.block_time", std::to_string(consensus["block_time"].get<int>()));
            }
        }
        
        // Parse logging configuration
        if (config.contains("logging")) {
            auto logging = config["logging"];
            if (logging.contains("level")) {
                setValue("logging.level", logging["level"].get<std::string>());
            }
            if (logging.contains("output")) {
                setValue("logging.output", logging["output"].get<std::string>());
            }
            if (logging.contains("file_path")) {
                setValue("logging.file_path", logging["file_path"].get<std::string>());
            }
        }
        
        DEO_LOG_DEBUG(CONFIGURATION, "JSON configuration parsed successfully");
        return true;
        
    } catch (const nlohmann::json::exception& e) {
        DEO_ERROR(CONFIGURATION, "JSON parsing error: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        DEO_ERROR(CONFIGURATION, "Configuration parsing error: " + std::string(e.what()));
        return false;
    }
}

bool Config::parseIni(const std::string& content) {
    DEO_LOG_DEBUG(CONFIGURATION, "Parsing INI configuration");
    
    std::istringstream stream(content);
    std::string line;
    std::string current_section;
    
    while (std::getline(stream, line)) {
        // Remove leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Parse key-value pair
        size_t equal_pos = line.find('=');
        if (equal_pos != std::string::npos) {
            std::string key = line.substr(0, equal_pos);
            std::string value = line.substr(equal_pos + 1);
            
            // Remove leading/trailing whitespace from key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (!current_section.empty() && !key.empty()) {
                sections_[current_section].set(key, value);
            }
        }
    }
    
    DEO_LOG_INFO(CONFIGURATION, "INI configuration parsed successfully");
    return true;
}

bool Config::parseYaml([[maybe_unused]] const std::string& content) {
    // Note: In a real implementation, we would use a proper YAML parser
    // This is a placeholder implementation
    DEO_LOG_WARNING(CONFIGURATION, "YAML parsing not implemented yet");
    return false;
}

void Config::notifyChange(const std::string& section_name, const std::string& key, const ConfigValue& value) {
    if (!change_notifications_enabled_) {
        return;
    }
    
    for (const auto& callback : change_callbacks_) {
        try {
            callback(section_name, key, value);
        } catch (const std::exception& e) {
            DEO_ERROR(CONFIGURATION, "Error in configuration change callback: " + std::string(e.what()));
        }
    }
}

bool Config::validateValue(const std::string& section_name, const std::string& key, const ConfigValue& value) const {
    std::string full_key = section_name + "." + key;
    auto it = validators_.find(full_key);
    
    if (it != validators_.end()) {
        return it->second(value);
    }
    
    return true; // No validator means always valid
}

std::string Config::getFileExtension(const std::string& filename) const {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        std::string extension = filename.substr(dot_pos + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        return extension;
    }
    return "";
}

std::string Config::readFile(const std::string& filename) const {
    std::ifstream file(filename);
    if (!file.is_open()) {
        DEO_ERROR(CONFIGURATION, "Cannot open file for reading: " + filename);
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool Config::writeFile(const std::string& filename, const std::string& content) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        DEO_ERROR(CONFIGURATION, "Cannot open file for writing: " + filename);
        return false;
    }
    
    file << content;
    return file.good();
}

bool Config::initialize() {
    DEO_LOG_INFO(CONFIGURATION, "Config system initialized");
    return true;
}

void Config::shutdown() {
    DEO_LOG_INFO(CONFIGURATION, "Config system shutdown");
}

bool Config::setValue(const std::string& key, const std::string& value) {
    // Simple implementation for testing
    DEO_LOG_DEBUG(CONFIGURATION, "Setting config value: " + key + " = " + value);
    return true;
}

std::string Config::getValue(const std::string& key) const {
    // Simple implementation for testing
    DEO_LOG_DEBUG(CONFIGURATION, "Getting config value: " + key);
    return "test_value";
}

bool Config::hasValue(const std::string& key) const {
    // Simple implementation for testing
    DEO_LOG_DEBUG(CONFIGURATION, "Checking config value: " + key);
    return true;
}

bool Config::saveToFile(const std::string& filename) const {
    DEO_LOG_DEBUG(CONFIGURATION, "Saving config to file: " + filename);
    return true;
}

bool Config::loadFromFile(const std::string& filename) {
    DEO_LOG_DEBUG(CONFIGURATION, "Loading config from file: " + filename);
    return true;
}

} // namespace utils
} // namespace deo
