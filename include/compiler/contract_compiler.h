/**
 * @file contract_compiler.h
 * @brief Contract compilation system for Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <exception>
#include <functional>

#include "vm/virtual_machine.h"
#include "nlohmann/json.hpp"

namespace deo {
namespace compiler {

/**
 * @brief Compilation error exception
 */
class CompilationError : public std::exception {
public:
    explicit CompilationError(const std::string& message) : message_(message) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
    
private:
    std::string message_;
};

/**
 * @brief Token types for the contract language
 */
enum class TokenType {
    // Keywords
    CONTRACT,       ///< contract keyword
    FUNCTION,       ///< function keyword
    RETURN,         ///< return keyword
    IF,             ///< if keyword
    ELSE,           ///< else keyword
    WHILE,          ///< while keyword
    FOR,            ///< for keyword
    VAR,            ///< var keyword
    CONST,          ///< const keyword
    PUBLIC,         ///< public keyword
    PRIVATE,        ///< private keyword
    VIEW,           ///< view keyword
    PURE,           ///< pure keyword
    PAYABLE,        ///< payable keyword
    
    // Types
    UINT8,          ///< uint8 type
    UINT16,         ///< uint16 type
    UINT32,         ///< uint32 type
    UINT64,         ///< uint64 type
    UINT256,        ///< uint256 type
    INT8,           ///< int8 type
    INT16,          ///< int16 type
    INT32,          ///< int32 type
    INT64,          ///< int64 type
    INT256,         ///< int256 type
    BOOL,           ///< bool type
    ADDRESS,        ///< address type
    BYTES,          ///< bytes type
    STRING,         ///< string type
    
    // Literals
    IDENTIFIER,     ///< identifier
    NUMBER,         ///< number literal
    STRING_LITERAL, ///< string literal
    BOOLEAN,        ///< boolean literal
    ADDRESS_LITERAL,///< address literal
    
    // Operators
    ASSIGN,         ///< = assignment
    PLUS,           ///< + addition
    MINUS,          ///< - subtraction
    MULTIPLY,       ///< * multiplication
    DIVIDE,         ///< / division
    MODULO,         ///< % modulo
    EQUAL,          ///< == equality
    NOT_EQUAL,      ///< != inequality
    LESS,           ///< < less than
    LESS_EQUAL,     ///< <= less than or equal
    GREATER,        ///< > greater than
    GREATER_EQUAL,  ///< >= greater than or equal
    AND,            ///< && logical and
    OR,             ///< || logical or
    NOT,            ///< ! logical not
    INCREMENT,      ///< ++ increment
    DECREMENT,      ///< -- decrement
    
    // Punctuation
    SEMICOLON,      ///< ; semicolon
    COMMA,          ///< , comma
    DOT,            ///< . dot
    COLON,          ///< : colon
    QUESTION,       ///< ? question mark
    
    // Brackets
    LEFT_PAREN,     ///< ( left parenthesis
    RIGHT_PAREN,    ///< ) right parenthesis
    LEFT_BRACE,     ///< { left brace
    RIGHT_BRACE,    ///< } right brace
    LEFT_BRACKET,   ///< [ left bracket
    RIGHT_BRACKET,  ///< ] right bracket
    
    // Special
    NEWLINE,        ///< newline
    EOF_TOKEN,      ///< end of file
    UNKNOWN         ///< unknown token
};

/**
 * @brief Token structure
 */
struct Token {
    TokenType type;
    std::string value;
    uint32_t line;
    uint32_t column;
    
    Token(TokenType t, const std::string& v, uint32_t l = 0, uint32_t c = 0)
        : type(t), value(v), line(l), column(c) {}
};

/**
 * @brief AST node types
 */
enum class ASTNodeType {
    CONTRACT_DECLARATION,
    FUNCTION_DECLARATION,
    VARIABLE_DECLARATION,
    ASSIGNMENT,
    BINARY_OPERATION,
    UNARY_OPERATION,
    FUNCTION_CALL,
    IDENTIFIER,
    LITERAL,
    BLOCK,
    IF_STATEMENT,
    WHILE_STATEMENT,
    FOR_STATEMENT,
    RETURN_STATEMENT,
    EXPRESSION_STATEMENT
};

/**
 * @brief Abstract Syntax Tree node
 */
class ASTNode {
public:
    ASTNodeType type;
    std::vector<std::shared_ptr<ASTNode>> children;
    std::string value;
    Token token;
    
    explicit ASTNode(ASTNodeType t, const Token& tok = Token(TokenType::UNKNOWN, ""))
        : type(t), token(tok) {}
    
    virtual ~ASTNode() = default;
    
    /**
     * @brief Add a child node
     * @param child Child node to add
     */
    void addChild(std::shared_ptr<ASTNode> child) {
        children.push_back(child);
    }
    
    /**
     * @brief Get child node by index
     * @param index Child index
     * @return Child node or nullptr
     */
    std::shared_ptr<ASTNode> getChild(size_t index) const {
        if (index < children.size()) {
            return children[index];
        }
        return nullptr;
    }
    
    /**
     * @brief Get number of children
     * @return Number of children
     */
    size_t getChildCount() const {
        return children.size();
    }
};

/**
 * @brief Contract compilation configuration
 */
struct CompilationConfig {
    bool optimize = true;                    ///< Enable optimization
    bool generate_debug_info = true;         ///< Generate debug information
    bool strict_mode = true;                 ///< Enable strict mode
    uint32_t max_contract_size = 24576;      ///< Maximum contract size in bytes
    uint32_t max_function_size = 1024;       ///< Maximum function size in bytes
    uint32_t max_stack_depth = 1024;         ///< Maximum stack depth
    bool enable_gas_estimation = true;       ///< Enable gas estimation
    std::string target_version = "1.0.0";    ///< Target compiler version
};

/**
 * @brief Compilation result
 */
struct CompilationResult {
    bool success = false;                    ///< Compilation success status
    std::vector<uint8_t> bytecode;           ///< Generated bytecode
    std::string abi;                         ///< Generated ABI (JSON)
    std::string metadata;                    ///< Compilation metadata
    std::vector<std::string> warnings;       ///< Compilation warnings
    std::vector<std::string> errors;         ///< Compilation errors
    uint64_t gas_estimate = 0;               ///< Gas usage estimate
    uint32_t contract_size = 0;              ///< Contract size in bytes
    std::string contract_hash;               ///< Contract hash
};

/**
 * @brief Lexical analyzer for contract source code
 */
class Lexer {
public:
    explicit Lexer(const std::string& source);
    
    /**
     * @brief Get next token
     * @return Next token
     */
    Token nextToken();
    
    /**
     * @brief Peek at next token without consuming it
     * @return Next token
     */
    Token peekToken();
    
    /**
     * @brief Check if there are more tokens
     * @return True if more tokens available
     */
    bool hasMoreTokens() const;
    
    /**
     * @brief Get current position
     * @return Current position in source
     */
    size_t getCurrentPosition() const;
    
    /**
     * @brief Reset to beginning
     */
    void reset();

private:
    std::string source_;
    size_t position_;
    uint32_t line_;
    uint32_t column_;
    
    /**
     * @brief Skip whitespace and comments
     */
    void skipWhitespace();
    
    /**
     * @brief Read identifier or keyword
     * @return Token
     */
    Token readIdentifier();
    
    /**
     * @brief Read number literal
     * @return Token
     */
    Token readNumber();
    
    /**
     * @brief Read string literal
     * @return Token
     */
    Token readString();
    
    /**
     * @brief Read address literal
     * @return Token
     */
    Token readAddress();
    
    /**
     * @brief Check if character is letter
     * @param c Character to check
     * @return True if letter
     */
    bool isLetter(char c) const;
    
    /**
     * @brief Check if character is digit
     * @param c Character to check
     * @return True if digit
     */
    bool isDigit(char c) const;
    
    /**
     * @brief Check if character is alphanumeric
     * @param c Character to check
     * @return True if alphanumeric
     */
    bool isAlphanumeric(char c) const;
    
    /**
     * @brief Get keyword token type
     * @param keyword Keyword string
     * @return Token type
     */
    TokenType getKeywordType(const std::string& keyword) const;
};

/**
 * @brief Parser for contract source code
 */
class Parser {
public:
    explicit Parser(const std::string& source);
    
    /**
     * @brief Parse contract source code
     * @return AST root node
     */
    std::shared_ptr<ASTNode> parse();
    
    /**
     * @brief Get parsing errors
     * @return List of parsing errors
     */
    const std::vector<std::string>& getErrors() const;

private:
    std::unique_ptr<Lexer> lexer_;
    std::vector<std::string> errors_;
    Token current_token_;
    
    /**
     * @brief Advance to next token
     */
    void advance();
    
    /**
     * @brief Check if current token matches type
     * @param type Token type to check
     * @return True if matches
     */
    bool match(TokenType type) const;
    
    /**
     * @brief Expect specific token type
     * @param type Expected token type
     * @return True if matches
     */
    bool expect(TokenType type);
    
    /**
     * @brief Add parsing error
     * @param message Error message
     */
    void addError(const std::string& message);
    
    /**
     * @brief Parse contract declaration
     * @return Contract AST node
     */
    std::shared_ptr<ASTNode> parseContract();
    
    /**
     * @brief Parse function declaration
     * @return Function AST node
     */
    std::shared_ptr<ASTNode> parseFunction();
    
    /**
     * @brief Parse variable declaration
     * @return Variable AST node
     */
    std::shared_ptr<ASTNode> parseVariableDeclaration();
    
    /**
     * @brief Parse statement
     * @return Statement AST node
     */
    std::shared_ptr<ASTNode> parseStatement();
    
    /**
     * @brief Parse expression
     * @return Expression AST node
     */
    std::shared_ptr<ASTNode> parseExpression();
    
    /**
     * @brief Parse primary expression
     * @return Primary expression AST node
     */
    std::shared_ptr<ASTNode> parsePrimary();
    
    /**
     * @brief Parse block statement
     * @return Block AST node
     */
    std::shared_ptr<ASTNode> parseBlock();
    
    /**
     * @brief Parse if statement
     * @return If statement AST node
     */
    std::shared_ptr<ASTNode> parseIfStatement();
    
    /**
     * @brief Parse while statement
     * @return While statement AST node
     */
    std::shared_ptr<ASTNode> parseWhileStatement();
    
    /**
     * @brief Parse for statement
     * @return For statement AST node
     */
    std::shared_ptr<ASTNode> parseForStatement();
    
    /**
     * @brief Parse return statement
     * @return Return statement AST node
     */
    std::shared_ptr<ASTNode> parseReturnStatement();
    
    /**
     * @brief Parse function call
     * @return Function call AST node
     */
    std::shared_ptr<ASTNode> parseFunctionCall();
    
    /**
     * @brief Parse parameter list
     * @return Parameter list
     */
    std::vector<std::shared_ptr<ASTNode>> parseParameterList();
    
    /**
     * @brief Parse argument list
     * @return Argument list
     */
    std::vector<std::shared_ptr<ASTNode>> parseArgumentList();
    
    /**
     * @brief Get operator precedence
     * @param op Operator token type
     * @return Precedence value
     */
    int getPrecedence(TokenType op) const;
};

/**
 * @brief Code generator for VM bytecode
 */
class CodeGenerator {
public:
    explicit CodeGenerator(const CompilationConfig& config);
    
    /**
     * @brief Generate bytecode from AST
     * @param ast AST root node
     * @return Generated bytecode
     */
    std::vector<uint8_t> generateBytecode(std::shared_ptr<ASTNode> ast);
    
    /**
     * @brief Generate ABI from AST
     * @param ast AST root node
     * @return Generated ABI (JSON)
     */
    std::string generateABI(std::shared_ptr<ASTNode> ast);
    
    /**
     * @brief Get generation errors
     * @return List of generation errors
     */
    const std::vector<std::string>& getErrors() const;
    
    /**
     * @brief Get gas estimate
     * @return Gas usage estimate
     */
    uint64_t getGasEstimate() const;

private:
    CompilationConfig config_;
    std::vector<uint8_t> bytecode_;
    std::vector<std::string> errors_;
    uint64_t gas_estimate_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::unordered_map<std::string, uint32_t> variables_;
    uint32_t variable_count_;
    
    /**
     * @brief Add bytecode instruction
     * @param opcode Opcode to add
     */
    void addInstruction(vm::Opcode opcode);
    
    /**
     * @brief Add bytecode instruction with operand
     * @param opcode Opcode to add
     * @param operand Operand value
     */
    void addInstruction(vm::Opcode opcode, uint32_t operand);
    
    /**
     * @brief Add bytecode data
     * @param data Data to add
     */
    void addData(const std::vector<uint8_t>& data);
    
    /**
     * @brief Add generation error
     * @param message Error message
     */
    void addError(const std::string& message);
    
    /**
     * @brief Generate code for contract
     * @param contract Contract AST node
     */
    void generateContract(std::shared_ptr<ASTNode> contract);
    
    /**
     * @brief Generate code for function
     * @param function Function AST node
     */
    void generateFunction(std::shared_ptr<ASTNode> function);
    
    /**
     * @brief Generate code for statement
     * @param statement Statement AST node
     */
    void generateStatement(std::shared_ptr<ASTNode> statement);
    
    /**
     * @brief Generate code for expression
     * @param expression Expression AST node
     */
    void generateExpression(std::shared_ptr<ASTNode> expression);
    
    /**
     * @brief Generate code for binary operation
     * @param operation Binary operation AST node
     */
    void generateBinaryOperation(std::shared_ptr<ASTNode> operation);
    
    /**
     * @brief Generate code for unary operation
     * @param operation Unary operation AST node
     */
    void generateUnaryOperation(std::shared_ptr<ASTNode> operation);
    
    /**
     * @brief Generate code for function call
     * @param call Function call AST node
     */
    void generateFunctionCall(std::shared_ptr<ASTNode> call);
    
    /**
     * @brief Generate code for variable access
     * @param variable Variable AST node
     */
    void generateVariableAccess(std::shared_ptr<ASTNode> variable);
    
    /**
     * @brief Generate code for literal
     * @param literal Literal AST node
     */
    void generateLiteral(std::shared_ptr<ASTNode> literal);
    
    /**
     * @brief Generate code for if statement
     * @param if_stmt If statement AST node
     */
    void generateIfStatement(std::shared_ptr<ASTNode> if_stmt);
    
    /**
     * @brief Generate code for while statement
     * @param while_stmt While statement AST node
     */
    void generateWhileStatement(std::shared_ptr<ASTNode> while_stmt);
    
    /**
     * @brief Generate code for for statement
     * @param for_stmt For statement AST node
     */
    void generateForStatement(std::shared_ptr<ASTNode> for_stmt);
    
    /**
     * @brief Generate code for return statement
     * @param return_stmt Return statement AST node
     */
    void generateReturnStatement(std::shared_ptr<ASTNode> return_stmt);
    
    /**
     * @brief Get variable index
     * @param name Variable name
     * @return Variable index
     */
    uint32_t getVariableIndex(const std::string& name);
    
    /**
     * @brief Create label
     * @param name Label name
     * @return Label address
     */
    uint32_t createLabel(const std::string& name);
    
    /**
     * @brief Resolve label
     * @param name Label name
     * @return Label address
     */
    uint32_t resolveLabel(const std::string& name);
    
    /**
     * @brief Get opcode for operator
     * @param op Operator token type
     * @return Corresponding opcode
     */
    vm::Opcode getOpcodeForOperator(TokenType op) const;
    
    /**
     * @brief Get opcode for type
     * @param type Type token type
     * @return Corresponding opcode
     */
    vm::Opcode getOpcodeForType(TokenType type) const;
    
    /**
     * @brief Estimate gas for instruction
     * @param opcode Instruction opcode
     * @return Gas estimate
     */
    uint64_t estimateGasForInstruction(vm::Opcode opcode) const;
};

/**
 * @brief Main contract compiler
 */
class ContractCompiler {
public:
    explicit ContractCompiler(const CompilationConfig& config = CompilationConfig{});
    
    /**
     * @brief Compile contract source code
     * @param source Source code
     * @return Compilation result
     */
    CompilationResult compile(const std::string& source);
    
    /**
     * @brief Compile contract from file
     * @param filename Source file path
     * @return Compilation result
     */
    CompilationResult compileFromFile(const std::string& filename);
    
    /**
     * @brief Set compilation configuration
     * @param config New configuration
     */
    void setConfig(const CompilationConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const CompilationConfig& getConfig() const;
    
    /**
     * @brief Validate source code
     * @param source Source code
     * @return True if valid
     */
    bool validateSource(const std::string& source);
    
    /**
     * @brief Get supported language features
     * @return List of supported features
     */
    std::vector<std::string> getSupportedFeatures() const;
    
    /**
     * @brief Get compiler version
     * @return Compiler version string
     */
    std::string getVersion() const;

private:
    CompilationConfig config_;
    std::unique_ptr<Parser> parser_;
    std::unique_ptr<CodeGenerator> generator_;
    
    /**
     * @brief Read file contents
     * @param filename File path
     * @return File contents
     */
    std::string readFile(const std::string& filename);
    
    /**
     * @brief Calculate contract hash
     * @param bytecode Contract bytecode
     * @return Contract hash
     */
    std::string calculateContractHash(const std::vector<uint8_t>& bytecode);
    
    /**
     * @brief Generate compilation metadata
     * @param result Compilation result
     * @return Metadata JSON
     */
    std::string generateMetadata(const CompilationResult& result);
};

} // namespace compiler
} // namespace deo
