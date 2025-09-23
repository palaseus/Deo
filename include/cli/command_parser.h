/**
 * @file command_parser.h
 * @brief Command line interface parser for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <memory>

#include "commands.h"

namespace deo {
namespace cli {

/**
 * @brief Command line argument types
 */
enum class ArgumentType {
    STRING,     ///< String argument
    INTEGER,    ///< Integer argument
    DOUBLE,     ///< Double argument
    BOOLEAN,    ///< Boolean argument
    FLAG        ///< Flag argument (no value)
};

/**
 * @brief Command line argument definition
 */
struct Argument {
    std::string name;           ///< Argument name
    std::string short_name;     ///< Short argument name (e.g., -v)
    std::string long_name;      ///< Long argument name (e.g., --verbose)
    ArgumentType type;          ///< Argument type
    std::string description;    ///< Argument description
    std::string default_value;  ///< Default value
    bool required;              ///< Whether argument is required
    bool multiple;              ///< Whether argument can have multiple values
    
    Argument() : type(ArgumentType::STRING), required(false), multiple(false) {}
};

/**
 * @brief Command definition
 */
struct Command {
    std::string name;                           ///< Command name
    std::string description;                    ///< Command description
    std::vector<Argument> arguments;            ///< Command arguments
    std::function<int(const std::map<std::string, std::string>&)> handler; ///< Command handler
    std::vector<std::string> aliases;          ///< Command aliases
    bool hidden;                               ///< Whether command is hidden from help
    
    Command() : hidden(false) {}
};

/**
 * @brief Command line interface parser
 * 
 * This class provides comprehensive command line parsing capabilities
 * with support for subcommands, arguments, flags, and help generation.
 */
class CommandParser {
public:
    /**
     * @brief Default constructor for testing
     */
    CommandParser();
    
    /**
     * @brief Constructor
     * @param argc Argument count
     * @param argv Argument vector
     */
    CommandParser(int argc, char* argv[]);
    
    /**
     * @brief Destructor
     */
    ~CommandParser() = default;
    
    // Disable copy and move semantics
    CommandParser(const CommandParser&) = delete;
    CommandParser& operator=(const CommandParser&) = delete;
    CommandParser(CommandParser&&) = delete;
    CommandParser& operator=(CommandParser&&) = delete;

    /**
     * @brief Register a command
     * @param command Command definition
     */
    void registerCommand(const Command& command);
    
    /**
     * @brief Register a command with handler function
     * @param name Command name
     * @param description Command description
     * @param handler Command handler function
     * @param arguments Command arguments
     */
    void registerCommand(const std::string& name,
                        const std::string& description,
                        std::function<int(const std::map<std::string, std::string>&)> handler,
                        const std::vector<Argument>& arguments = {});
    
    /**
     * @brief Initialize the command parser
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * @brief Shutdown the command parser
     */
    void shutdown();
    
    /**
     * @brief Parse command line arguments
     * @return True if parsing was successful
     */
    bool parse();
    
    /**
     * @brief Parse a specific command string
     * @param command Command string to parse
     * @return True if parsing was successful
     */
    bool parse(const std::string& command);
    
    /**
     * @brief Get help text for all commands
     * @return Help text string
     */
    std::string getHelp() const;
    
    /**
     * @brief Execute the parsed command
     * @return Exit code
     */
    int execute();
    
    /**
     * @brief Get the parsed command name
     * @return Command name
     */
    const std::string& getCommand() const { return command_; }
    
    /**
     * @brief Get parsed arguments
     * @return Map of argument names to values
     */
    const std::map<std::string, std::string>& getArguments() const { return arguments_; }
    
    /**
     * @brief Get a specific argument value
     * @param name Argument name
     * @param default_value Default value if argument not found
     * @return Argument value
     */
    std::string getArgument(const std::string& name, const std::string& default_value = "") const;
    
    /**
     * @brief Check if an argument was provided
     * @param name Argument name
     * @return True if argument was provided
     */
    bool hasArgument(const std::string& name) const;
    
    /**
     * @brief Get the program name
     * @return Program name
     */
    const std::string& getProgramName() const { return program_name_; }
    
    /**
     * @brief Get the program version
     * @return Program version
     */
    const std::string& getProgramVersion() const { return program_version_; }
    
    /**
     * @brief Set the program version
     * @param version Program version
     */
    void setProgramVersion(const std::string& version) { program_version_ = version; }
    
    /**
     * @brief Get the program description
     * @return Program description
     */
    const std::string& getProgramDescription() const { return program_description_; }
    
    /**
     * @brief Set the program description
     * @param description Program description
     */
    void setProgramDescription(const std::string& description) { program_description_ = description; }
    
    /**
     * @brief Show help information
     * @param command_name Specific command to show help for (empty for general help)
     */
    void showHelp(const std::string& command_name = "") const;
    
    /**
     * @brief Show version information
     */
    void showVersion() const;
    
    /**
     * @brief Get all registered commands
     * @return List of command names
     */
    std::vector<std::string> getCommands() const;
    
    /**
     * @brief Get command information
     * @param command_name Command name
     * @return Command definition if found, nullptr otherwise
     */
    const Command* getCommandInfo(const std::string& command_name) const;
    
    /**
     * @brief Enable/disable automatic help generation
     * @param enabled Whether to enable automatic help
     */
    void setAutoHelpEnabled(bool enabled) { auto_help_enabled_ = enabled; }
    
    /**
     * @brief Enable/disable automatic version display
     * @param enabled Whether to enable automatic version display
     */
    void setAutoVersionEnabled(bool enabled) { auto_version_enabled_ = enabled; }
    
    /**
     * @brief Set error handling mode
     * @param mode Error handling mode
     */
    enum class ErrorMode {
        EXIT_ON_ERROR,      ///< Exit program on error
        THROW_ON_ERROR,     ///< Throw exception on error
        CONTINUE_ON_ERROR   ///< Continue execution on error
    };
    void setErrorMode(ErrorMode mode) { error_mode_ = mode; }

private:
    std::string program_name_;                    ///< Program name
    std::string program_version_;                 ///< Program version
    std::string program_description_;             ///< Program description
    std::vector<std::string> raw_args_;           ///< Raw command line arguments
    std::map<std::string, Command> commands_;     ///< Registered commands
    std::string command_;                         ///< Parsed command name
    std::map<std::string, std::string> arguments_; ///< Parsed arguments
    bool auto_help_enabled_;                      ///< Whether to show help automatically
    bool auto_version_enabled_;                   ///< Whether to show version automatically
    ErrorMode error_mode_;                        ///< Error handling mode
    
    /**
     * @brief Initialize default commands
     */
    void initializeDefaultCommands();
    
    /**
     * @brief Parse command line arguments
     * @return True if parsing was successful
     */
    bool parseArguments();
    
    /**
     * @brief Validate parsed arguments
     * @return True if arguments are valid
     */
    bool validateArguments() const;
    
    /**
     * @brief Handle parsing errors
     * @param error_message Error message
     */
    void handleError(const std::string& error_message) const;
    
    /**
     * @brief Find command by name or alias
     * @param name Command name or alias
     * @return Command definition if found, nullptr otherwise
     */
    const Command* findCommand(const std::string& name) const;
    
    /**
     * @brief Generate help text for a command
     * @param command Command definition
     * @return Help text
     */
    std::string generateCommandHelp(const Command& command) const;
    
    /**
     * @brief Generate general help text
     * @return Help text
     */
    std::string generateGeneralHelp() const;
    
    /**
     * @brief Parse argument value based on type
     * @param value Raw argument value
     * @param type Argument type
     * @return Parsed value
     */
    std::string parseArgumentValue(const std::string& value, ArgumentType type) const;
    
    /**
     * @brief Convert argument type to string
     * @param type Argument type
     * @return Type name
     */
    std::string argumentTypeToString(ArgumentType type) const;
};

} // namespace cli
} // namespace deo
