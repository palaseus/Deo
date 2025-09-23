/**
 * @file config.h
 * @brief Configuration management system for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <variant>
#include <fstream>
#include <any>
#include <functional>

namespace deo {
namespace utils {

/**
 * @brief Configuration value types
 */
using ConfigValue = std::variant<bool, int, double, std::string, std::vector<std::string>>;

/**
 * @brief Configuration section
 */
class ConfigSection {
public:
    /**
     * @brief Default constructor
     */
    ConfigSection() : name_("") {}
    
    /**
     * @brief Constructor
     * @param name Section name
     */
    explicit ConfigSection(const std::string& name) : name_(name) {}
    
    /**
     * @brief Get a configuration value
     * @param key Configuration key
     * @param default_value Default value if key not found
     * @return Configuration value
     */
    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        auto it = values_.find(key);
        if (it != values_.end()) {
            try {
                return std::get<T>(it->second);
            } catch (const std::bad_variant_access&) {
                return default_value;
            }
        }
        return default_value;
    }
    
    /**
     * @brief Set a configuration value
     * @param key Configuration key
     * @param value Configuration value
     */
    template<typename T>
    void set(const std::string& key, const T& value) {
        values_[key] = value;
    }
    
    /**
     * @brief Check if a key exists
     * @param key Configuration key
     * @return True if key exists
     */
    bool has(const std::string& key) const {
        return values_.find(key) != values_.end();
    }
    
    /**
     * @brief Remove a configuration key
     * @param key Configuration key to remove
     * @return True if key was removed
     */
    bool remove(const std::string& key) {
        auto it = values_.find(key);
        if (it != values_.end()) {
            values_.erase(it);
            return true;
        }
        return false;
    }
    
    /**
     * @brief Get all configuration keys
     * @return List of configuration keys
     */
    std::vector<std::string> getKeys() const {
        std::vector<std::string> keys;
        for (const auto& pair : values_) {
            keys.push_back(pair.first);
        }
        return keys;
    }
    
    /**
     * @brief Get the section name
     * @return Section name
     */
    const std::string& getName() const { return name_; }
    
    /**
     * @brief Clear all configuration values
     */
    void clear() {
        values_.clear();
    }

private:
    std::string name_;
    std::map<std::string, ConfigValue> values_;
};

/**
 * @brief Configuration management system
 * 
 * This class provides comprehensive configuration management with support for
 * multiple file formats, environment variables, command line arguments, and
 * runtime configuration updates.
 */
class Config {
public:
    /**
     * @brief Constructor
     */
    Config();
    
    /**
     * @brief Initialize the configuration system
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the configuration system
     */
    void shutdown();
    
    /**
     * @brief Set a configuration value
     * @param key Configuration key
     * @param value Configuration value
     * @return True if value was set successfully
     */
    bool setValue(const std::string& key, const std::string& value);
    
    /**
     * @brief Get a configuration value
     * @param key Configuration key
     * @return Configuration value or empty string if not found
     */
    std::string getValue(const std::string& key) const;
    
    /**
     * @brief Check if a configuration value exists
     * @param key Configuration key
     * @return True if value exists
     */
    bool hasValue(const std::string& key) const;
    
    /**
     * @brief Save configuration to file
     * @param filename File to save to
     * @return True if save was successful
     */
    bool saveToFile(const std::string& filename) const;
    
    /**
     * @brief Load configuration from file
     * @param filename File to load from
     * @return True if load was successful
     */
    bool loadFromFile(const std::string& filename);
    
    /**
     * @brief Destructor
     */
    ~Config() = default;
    
    // Disable copy and move semantics
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;

    /**
     * @brief Load configuration from a file
     * @param filename Configuration file path
     * @return True if loading was successful
     */
    bool load(const std::string& filename);
    
    /**
     * @brief Save configuration to a file
     * @param filename Configuration file path
     * @return True if saving was successful
     */
    bool save(const std::string& filename) const;
    
    /**
     * @brief Load configuration from environment variables
     * @param prefix Environment variable prefix
     */
    void loadFromEnvironment(const std::string& prefix = "DEO_");
    
    /**
     * @brief Load configuration from command line arguments
     * @param argc Argument count
     * @param argv Argument vector
     */
    void loadFromCommandLine(int argc, char* argv[]);
    
    /**
     * @brief Get a configuration section
     * @param section_name Section name
     * @return Reference to the configuration section
     */
    ConfigSection& getSection(const std::string& section_name);
    
    /**
     * @brief Get a configuration section (const)
     * @param section_name Section name
     * @return Const reference to the configuration section
     */
    const ConfigSection& getSection(const std::string& section_name) const;
    
    /**
     * @brief Get a configuration value
     * @param section_name Section name
     * @param key Configuration key
     * @param default_value Default value if key not found
     * @return Configuration value
     */
    template<typename T>
    T get(const std::string& section_name, const std::string& key, const T& default_value = T{}) const {
        auto it = sections_.find(section_name);
        if (it != sections_.end()) {
            return it->second.get<T>(key, default_value);
        }
        return default_value;
    }
    
    /**
     * @brief Set a configuration value
     * @param section_name Section name
     * @param key Configuration key
     * @param value Configuration value
     */
    template<typename T>
    void set(const std::string& section_name, const std::string& key, const T& value) {
        sections_[section_name].set(key, value);
    }
    
    /**
     * @brief Check if a section exists
     * @param section_name Section name
     * @return True if section exists
     */
    bool hasSection(const std::string& section_name) const {
        return sections_.find(section_name) != sections_.end();
    }
    
    /**
     * @brief Check if a configuration key exists
     * @param section_name Section name
     * @param key Configuration key
     * @return True if key exists
     */
    bool hasKey(const std::string& section_name, const std::string& key) const {
        auto it = sections_.find(section_name);
        if (it != sections_.end()) {
            return it->second.has(key);
        }
        return false;
    }
    
    /**
     * @brief Get all section names
     * @return List of section names
     */
    std::vector<std::string> getSectionNames() const {
        std::vector<std::string> names;
        for (const auto& pair : sections_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    /**
     * @brief Remove a configuration section
     * @param section_name Section name to remove
     * @return True if section was removed
     */
    bool removeSection(const std::string& section_name) {
        auto it = sections_.find(section_name);
        if (it != sections_.end()) {
            sections_.erase(it);
            return true;
        }
        return false;
    }
    
    /**
     * @brief Remove a configuration key
     * @param section_name Section name
     * @param key Configuration key to remove
     * @return True if key was removed
     */
    bool removeKey(const std::string& section_name, const std::string& key) {
        auto it = sections_.find(section_name);
        if (it != sections_.end()) {
            return it->second.remove(key);
        }
        return false;
    }
    
    /**
     * @brief Clear all configuration
     */
    void clear() {
        sections_.clear();
    }
    
    /**
     * @brief Get configuration as JSON string
     * @return JSON representation of configuration
     */
    std::string toJson() const;
    
    /**
     * @brief Load configuration from JSON string
     * @param json JSON string representation
     * @return True if loading was successful
     */
    bool fromJson(const std::string& json);
    
    /**
     * @brief Validate configuration
     * @return True if configuration is valid
     */
    bool validate() const;
    
    /**
     * @brief Get configuration validation errors
     * @return List of validation errors
     */
    std::vector<std::string> getValidationErrors() const;
    
    /**
     * @brief Set configuration validation rules
     * @param section_name Section name
     * @param key Configuration key
     * @param validator Validation function
     */
    void setValidator(const std::string& section_name, 
                     const std::string& key, 
                     std::function<bool(const ConfigValue&)> validator);
    
    /**
     * @brief Register a configuration change callback
     * @param callback Callback function
     */
    void registerChangeCallback(std::function<void(const std::string&, const std::string&, const ConfigValue&)> callback);
    
    /**
     * @brief Enable/disable configuration change notifications
     * @param enabled Whether to enable notifications
     */
    void setChangeNotificationsEnabled(bool enabled);

private:
    mutable std::mutex mutex_;
    std::map<std::string, ConfigSection> sections_;
    std::map<std::string, std::function<bool(const ConfigValue&)>> validators_;
    std::vector<std::function<void(const std::string&, const std::string&, const ConfigValue&)>> change_callbacks_;
    bool change_notifications_enabled_;
    
    /**
     * @brief Parse JSON configuration
     * @param json JSON string
     * @return True if parsing was successful
     */
    bool parseJson(const std::string& json);
    
    /**
     * @brief Parse INI configuration
     * @param content INI content
     * @return True if parsing was successful
     */
    bool parseIni(const std::string& content);
    
    /**
     * @brief Parse YAML configuration
     * @param content YAML content
     * @return True if parsing was successful
     */
    bool parseYaml(const std::string& content);
    
    /**
     * @brief Notify configuration change callbacks
     * @param section_name Section name
     * @param key Configuration key
     * @param value New value
     */
    void notifyChange(const std::string& section_name, const std::string& key, const ConfigValue& value);
    
    /**
     * @brief Validate a configuration value
     * @param section_name Section name
     * @param key Configuration key
     * @param value Value to validate
     * @return True if value is valid
     */
    bool validateValue(const std::string& section_name, const std::string& key, const ConfigValue& value) const;
    
    /**
     * @brief Get file extension
     * @param filename File path
     * @return File extension
     */
    std::string getFileExtension(const std::string& filename) const;
    
    /**
     * @brief Read file content
     * @param filename File path
     * @return File content
     */
    std::string readFile(const std::string& filename) const;
    
    /**
     * @brief Write file content
     * @param filename File path
     * @param content Content to write
     * @return True if writing was successful
     */
    bool writeFile(const std::string& filename, const std::string& content) const;
};

} // namespace utils
} // namespace deo
