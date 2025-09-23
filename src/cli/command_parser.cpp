/**
 * @file command_parser.cpp
 * @brief Command line interface parser implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "cli/command_parser.h"
#include "utils/logger.h"
#include "utils/error_handler.h"

#include <algorithm>
#include <sstream>
#include <iostream>

namespace deo {
namespace cli {

CommandParser::CommandParser()
    : auto_help_enabled_(true)
    , auto_version_enabled_(true)
    , error_mode_(ErrorMode::EXIT_ON_ERROR) {
    
    program_version_ = "1.0.0";
    program_description_ = "Deo Blockchain - A modern blockchain implementation";
    
    initializeDefaultCommands();
    
    DEO_LOG_DEBUG(CLI, "CommandParser created (default constructor)");
}

CommandParser::CommandParser(int argc, char* argv[])
    : auto_help_enabled_(true)
    , auto_version_enabled_(true)
    , error_mode_(ErrorMode::EXIT_ON_ERROR) {
    
    if (argc > 0) {
        program_name_ = argv[0];
    }
    
    // Store raw arguments
    for (int i = 1; i < argc; ++i) {
        raw_args_.push_back(argv[i]);
    }
    
    program_version_ = "1.0.0";
    program_description_ = "Deo Blockchain - A modern blockchain implementation";
    
    initializeDefaultCommands();
    
    DEO_LOG_DEBUG(CLI, "Command parser created with " + std::to_string(raw_args_.size()) + " arguments");
}

void CommandParser::registerCommand(const Command& command) {
    commands_[command.name] = command;
    
    // Register aliases
    for (const auto& alias : command.aliases) {
        commands_[alias] = command;
    }
    
    DEO_LOG_DEBUG(CLI, "Registered command: " + command.name);
}

void CommandParser::registerCommand(const std::string& name,
                                   const std::string& description,
                                   std::function<int(const std::map<std::string, std::string>&)> handler,
                                   const std::vector<Argument>& arguments) {
    Command command;
    command.name = name;
    command.description = description;
    command.handler = handler;
    command.arguments = arguments;
    
    registerCommand(command);
}

bool CommandParser::parse() {
    DEO_LOG_DEBUG(CLI, "Parsing command line arguments");
    
    if (raw_args_.empty()) {
        if (auto_help_enabled_) {
            showHelp();
            return false;
        }
        return true;
    }
    
    // Check for help flag
    if (std::find(raw_args_.begin(), raw_args_.end(), "--help") != raw_args_.end() ||
        std::find(raw_args_.begin(), raw_args_.end(), "-h") != raw_args_.end()) {
        if (auto_help_enabled_) {
            showHelp();
            return false;
        }
    }
    
    // Check for version flag
    if (std::find(raw_args_.begin(), raw_args_.end(), "--version") != raw_args_.end() ||
        std::find(raw_args_.begin(), raw_args_.end(), "-v") != raw_args_.end()) {
        if (auto_version_enabled_) {
            showVersion();
            return false;
        }
    }
    
    // Parse command and arguments
    if (!parseArguments()) {
        return false;
    }
    
    // Validate arguments
    if (!validateArguments()) {
        return false;
    }
    
    DEO_LOG_DEBUG(CLI, "Command line arguments parsed successfully");
    return true;
}

int CommandParser::execute() {
    // Parse arguments first if not already parsed
    if (command_.empty() && !raw_args_.empty()) {
        if (!parse()) {
            DEO_ERROR(CLI, "Failed to parse command line arguments");
            return 1;
        }
    }
    
    DEO_LOG_DEBUG(CLI, "Executing command: " + command_);
    
    if (command_.empty()) {
        DEO_ERROR(CLI, "No command specified");
        return 1;
    }
    
    const Command* cmd = findCommand(command_);
    if (!cmd) {
        DEO_ERROR(CLI, "Unknown command: " + command_);
        return 1;
    }
    
    try {
        int result = cmd->handler(arguments_);
        DEO_LOG_DEBUG(CLI, "Command executed with result: " + std::to_string(result));
        return result;
    } catch (const std::exception& e) {
        DEO_ERROR(CLI, "Command execution failed: " + std::string(e.what()));
        return 1;
    }
}

std::string CommandParser::getArgument(const std::string& name, const std::string& default_value) const {
    auto it = arguments_.find(name);
    if (it != arguments_.end()) {
        return it->second;
    }
    return default_value;
}

bool CommandParser::hasArgument(const std::string& name) const {
    return arguments_.find(name) != arguments_.end();
}

void CommandParser::showHelp(const std::string& command_name) const {
    if (command_name.empty()) {
        std::cout << generateGeneralHelp() << std::endl;
    } else {
        const Command* cmd = findCommand(command_name);
        if (cmd) {
            std::cout << generateCommandHelp(*cmd) << std::endl;
        } else {
            std::cout << "Unknown command: " << command_name << std::endl;
        }
    }
}

void CommandParser::showVersion() const {
    std::cout << program_name_ << " version " << program_version_ << std::endl;
}

std::vector<std::string> CommandParser::getCommands() const {
    std::vector<std::string> command_names;
    for (const auto& pair : commands_) {
        if (pair.first == pair.second.name) { // Only add primary names, not aliases
            command_names.push_back(pair.first);
        }
    }
    return command_names;
}

const Command* CommandParser::getCommandInfo(const std::string& command_name) const {
    return findCommand(command_name);
}

void CommandParser::initializeDefaultCommands() {
    // Initialize default commands
    DEO_LOG_DEBUG(CLI, "Initializing default commands");
}

bool CommandParser::parseArguments() {
    if (raw_args_.empty()) {
        return true;
    }
    
    // First argument is the command
    command_ = raw_args_[0];
    
    // Parse remaining arguments
    for (size_t i = 1; i < raw_args_.size(); ++i) {
        const std::string& arg = raw_args_[i];
        
        if (arg.substr(0, 2) == "--") {
            // Long argument
            std::string key = arg.substr(2);
            std::string value = "true"; // Default value for flags
            
            // Check if there's a value
            if (i + 1 < raw_args_.size() && raw_args_[i + 1].substr(0, 1) != "-") {
                value = raw_args_[i + 1];
                i++; // Skip the value
            }
            
            arguments_[key] = value;
        } else if (arg.substr(0, 1) == "-") {
            // Short argument
            std::string key = arg.substr(1);
            std::string value = "true"; // Default value for flags
            
            // Check if there's a value
            if (i + 1 < raw_args_.size() && raw_args_[i + 1].substr(0, 1) != "-") {
                value = raw_args_[i + 1];
                i++; // Skip the value
            }
            
            arguments_[key] = value;
        } else {
            // Positional argument
            arguments_["positional_" + std::to_string(i)] = arg;
        }
    }
    
    return true;
}

bool CommandParser::validateArguments() const {
    const Command* cmd = findCommand(command_);
    if (!cmd) {
        return true; // Unknown command, let it fail later
    }
    
    // Validate required arguments
    for (const auto& arg : cmd->arguments) {
        if (arg.required && !hasArgument(arg.name)) {
            DEO_ERROR(CLI, "Required argument missing: " + arg.name);
            return false;
        }
    }
    
    return true;
}

void CommandParser::handleError(const std::string& error_message) const {
    switch (error_mode_) {
        case ErrorMode::EXIT_ON_ERROR:
            std::cerr << "Error: " << error_message << std::endl;
            exit(1);
            break;
        case ErrorMode::THROW_ON_ERROR:
            throw std::runtime_error(error_message);
            break;
        case ErrorMode::CONTINUE_ON_ERROR:
            std::cerr << "Error: " << error_message << std::endl;
            break;
    }
}

const Command* CommandParser::findCommand(const std::string& name) const {
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string CommandParser::generateCommandHelp(const Command& command) const {
    std::stringstream ss;
    
    ss << "Usage: " << program_name_ << " " << command.name;
    
    for (const auto& arg : command.arguments) {
        if (arg.required) {
            ss << " <" << arg.name << ">";
        } else {
            ss << " [" << arg.name << "]";
        }
    }
    
    ss << "\n\n" << command.description << "\n\n";
    
    if (!command.arguments.empty()) {
        ss << "Arguments:\n";
        for (const auto& arg : command.arguments) {
            ss << "  " << arg.name;
            if (!arg.short_name.empty()) {
                ss << ", " << arg.short_name;
            }
            ss << "    " << arg.description;
            if (!arg.required) {
                ss << " (optional)";
            }
            ss << "\n";
        }
    }
    
    return ss.str();
}

std::string CommandParser::generateGeneralHelp() const {
    std::stringstream ss;
    
    ss << program_description_ << "\n\n";
    ss << "Usage: " << program_name_ << " <command> [options]\n\n";
    ss << "Commands:\n";
    
    for (const auto& pair : commands_) {
        if (pair.first == pair.second.name && !pair.second.hidden) {
            ss << "  " << pair.second.name << "    " << pair.second.description << "\n";
        }
    }
    
    ss << "\nUse '" << program_name_ << " <command> --help' for more information about a command.\n";
    
    return ss.str();
}

std::string CommandParser::parseArgumentValue(const std::string& value, ArgumentType type) const {
    // Note: In a real implementation, we would parse the value according to type
    // This is a placeholder implementation
    
    // Suppress unused parameter warning
    (void)type;
    
    return value;
}

std::string CommandParser::argumentTypeToString(ArgumentType type) const {
    switch (type) {
        case ArgumentType::STRING: return "string";
        case ArgumentType::INTEGER: return "integer";
        case ArgumentType::DOUBLE: return "double";
        case ArgumentType::BOOLEAN: return "boolean";
        case ArgumentType::FLAG: return "flag";
        default: return "unknown";
    }
}

bool CommandParser::initialize() {
    DEO_LOG_INFO(CLI, "CommandParser initialized");
    return true;
}

void CommandParser::shutdown() {
    DEO_LOG_INFO(CLI, "CommandParser shutdown");
}

bool CommandParser::parse(const std::string& command) {
    // Simple implementation for testing
    DEO_LOG_DEBUG(CLI, "Parsing command: " + command);
    return true;
}

std::string CommandParser::getHelp() const {
    std::stringstream ss;
    ss << "Available commands:\n";
    for (const auto& cmd : commands_) {
        ss << "  " << cmd.first << " - " << cmd.second.description << "\n";
    }
    return ss.str();
}

} // namespace cli
} // namespace deo
