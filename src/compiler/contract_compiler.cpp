/**
 * @file contract_compiler.cpp
 * @brief Contract compilation system implementation
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#include "compiler/contract_compiler.h"
#include "utils/logger.h"
#include "crypto/hash.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace deo {
namespace compiler {

// CompilationError implementation is in header file

// Lexer implementation
Lexer::Lexer(const std::string& source) 
    : source_(source), position_(0), line_(1), column_(1) {
}

Token Lexer::nextToken() {
    skipWhitespace();
    
    if (position_ >= source_.length()) {
        return Token(TokenType::EOF_TOKEN, "", line_, column_);
    }
    
    char current = source_[position_];
    
    // Identifiers and keywords
    if (isLetter(current) || current == '_') {
        return readIdentifier();
    }
    
    // Address literals (must be checked before numbers)
    if (current == '0' && position_ + 1 < source_.length() && source_[position_ + 1] == 'x') {
        return readAddress();
    }
    
    // Numbers
    if (isDigit(current)) {
        return readNumber();
    }
    
    // Strings
    if (current == '"') {
        return readString();
    }
    
    // Operators and punctuation
    switch (current) {
        case '=':
            if (position_ + 1 < source_.length() && source_[position_ + 1] == '=') {
                position_ += 2;
                column_ += 2;
                return Token(TokenType::EQUAL, "==", line_, column_ - 1);
            }
            position_++;
            column_++;
            return Token(TokenType::ASSIGN, "=", line_, column_ - 1);
            
        case '!':
            if (position_ + 1 < source_.length() && source_[position_ + 1] == '=') {
                position_ += 2;
                column_ += 2;
                return Token(TokenType::NOT_EQUAL, "!=", line_, column_ - 1);
            }
            position_++;
            column_++;
            return Token(TokenType::NOT, "!", line_, column_ - 1);
            
        case '<':
            if (position_ + 1 < source_.length() && source_[position_ + 1] == '=') {
                position_ += 2;
                column_ += 2;
                return Token(TokenType::LESS_EQUAL, "<=", line_, column_ - 1);
            }
            position_++;
            column_++;
            return Token(TokenType::LESS, "<", line_, column_ - 1);
            
        case '>':
            if (position_ + 1 < source_.length() && source_[position_ + 1] == '=') {
                position_ += 2;
                column_ += 2;
                return Token(TokenType::GREATER_EQUAL, ">=", line_, column_ - 1);
            }
            position_++;
            column_++;
            return Token(TokenType::GREATER, ">", line_, column_ - 1);
            
        case '&':
            if (position_ + 1 < source_.length() && source_[position_ + 1] == '&') {
                position_ += 2;
                column_ += 2;
                return Token(TokenType::AND, "&&", line_, column_ - 1);
            }
            break;
            
        case '|':
            if (position_ + 1 < source_.length() && source_[position_ + 1] == '|') {
                position_ += 2;
                column_ += 2;
                return Token(TokenType::OR, "||", line_, column_ - 1);
            }
            break;
            
        case '+':
            if (position_ + 1 < source_.length() && source_[position_ + 1] == '+') {
                position_ += 2;
                column_ += 2;
                return Token(TokenType::INCREMENT, "++", line_, column_ - 1);
            }
            position_++;
            column_++;
            return Token(TokenType::PLUS, "+", line_, column_ - 1);
            
        case '-':
            if (position_ + 1 < source_.length() && source_[position_ + 1] == '-') {
                position_ += 2;
                column_ += 2;
                return Token(TokenType::DECREMENT, "--", line_, column_ - 1);
            }
            position_++;
            column_++;
            return Token(TokenType::MINUS, "-", line_, column_ - 1);
            
        case '*':
            position_++;
            column_++;
            return Token(TokenType::MULTIPLY, "*", line_, column_ - 1);
            
        case '/':
            position_++;
            column_++;
            return Token(TokenType::DIVIDE, "/", line_, column_ - 1);
            
        case '%':
            position_++;
            column_++;
            return Token(TokenType::MODULO, "%", line_, column_ - 1);
            
        case ';':
            position_++;
            column_++;
            return Token(TokenType::SEMICOLON, ";", line_, column_ - 1);
            
        case ',':
            position_++;
            column_++;
            return Token(TokenType::COMMA, ",", line_, column_ - 1);
            
        case '.':
            position_++;
            column_++;
            return Token(TokenType::DOT, ".", line_, column_ - 1);
            
        case ':':
            position_++;
            column_++;
            return Token(TokenType::COLON, ":", line_, column_ - 1);
            
        case '?':
            position_++;
            column_++;
            return Token(TokenType::QUESTION, "?", line_, column_ - 1);
            
        case '(':
            position_++;
            column_++;
            return Token(TokenType::LEFT_PAREN, "(", line_, column_ - 1);
            
        case ')':
            position_++;
            column_++;
            return Token(TokenType::RIGHT_PAREN, ")", line_, column_ - 1);
            
        case '{':
            position_++;
            column_++;
            return Token(TokenType::LEFT_BRACE, "{", line_, column_ - 1);
            
        case '}':
            position_++;
            column_++;
            return Token(TokenType::RIGHT_BRACE, "}", line_, column_ - 1);
            
        case '[':
            position_++;
            column_++;
            return Token(TokenType::LEFT_BRACKET, "[", line_, column_ - 1);
            
        case ']':
            position_++;
            column_++;
            return Token(TokenType::RIGHT_BRACKET, "]", line_, column_ - 1);
            
        default:
            position_++;
            column_++;
            return Token(TokenType::UNKNOWN, std::string(1, current), line_, column_ - 1);
    }
    
    position_++;
    column_++;
    return Token(TokenType::UNKNOWN, std::string(1, current), line_, column_ - 1);
}

Token Lexer::peekToken() {
    size_t saved_position = position_;
    uint32_t saved_line = line_;
    uint32_t saved_column = column_;
    
    Token token = nextToken();
    
    position_ = saved_position;
    line_ = saved_line;
    column_ = saved_column;
    
    return token;
}

bool Lexer::hasMoreTokens() const {
    return position_ < source_.length();
}

size_t Lexer::getCurrentPosition() const {
    return position_;
}

void Lexer::reset() {
    position_ = 0;
    line_ = 1;
    column_ = 1;
}

void Lexer::skipWhitespace() {
    while (position_ < source_.length()) {
        char current = source_[position_];
        
        if (current == ' ' || current == '\t') {
            position_++;
            column_++;
        } else if (current == '\n') {
            position_++;
            line_++;
            column_ = 1;
        } else if (current == '\r') {
            position_++;
            if (position_ < source_.length() && source_[position_] == '\n') {
                position_++;
            }
            line_++;
            column_ = 1;
        } else if (current == '/' && position_ + 1 < source_.length() && source_[position_ + 1] == '/') {
            // Single line comment
            while (position_ < source_.length() && source_[position_] != '\n') {
                position_++;
            }
        } else if (current == '/' && position_ + 1 < source_.length() && source_[position_ + 1] == '*') {
            // Multi-line comment
            position_ += 2;
            while (position_ + 1 < source_.length()) {
                if (source_[position_] == '*' && source_[position_ + 1] == '/') {
                    position_ += 2;
                    break;
                }
                if (source_[position_] == '\n') {
                    line_++;
                    column_ = 1;
                } else {
                    column_++;
                }
                position_++;
            }
        } else {
            break;
        }
    }
}

Token Lexer::readIdentifier() {
    size_t start = position_;
    
    while (position_ < source_.length() && isAlphanumeric(source_[position_])) {
        position_++;
        column_++;
    }
    
    std::string value = source_.substr(start, position_ - start);
    TokenType type = getKeywordType(value);
    
    if (type == TokenType::UNKNOWN) {
        type = TokenType::IDENTIFIER;
    }
    
    return Token(type, value, line_, column_ - value.length());
}

Token Lexer::readNumber() {
    size_t start = position_;
    
    while (position_ < source_.length() && isDigit(source_[position_])) {
        position_++;
        column_++;
    }
    
    std::string value = source_.substr(start, position_ - start);
    return Token(TokenType::NUMBER, value, line_, column_ - value.length());
}

Token Lexer::readString() {
    position_++; // Skip opening quote
    column_++;
    
    size_t start = position_;
    
    while (position_ < source_.length() && source_[position_] != '"') {
        if (source_[position_] == '\\' && position_ + 1 < source_.length()) {
            position_ += 2; // Skip escape sequence
            column_ += 2;
        } else {
            position_++;
            column_++;
        }
    }
    
    std::string value = source_.substr(start, position_ - start);
    
    if (position_ < source_.length()) {
        position_++; // Skip closing quote
        column_++;
    }
    
    return Token(TokenType::STRING_LITERAL, value, line_, column_ - value.length() - 2);
}

Token Lexer::readAddress() {
    size_t start = position_;
    position_ += 2; // Skip "0x"
    column_ += 2;
    
    while (position_ < source_.length() && 
           (isDigit(source_[position_]) || 
            (source_[position_] >= 'a' && source_[position_] <= 'f') ||
            (source_[position_] >= 'A' && source_[position_] <= 'F'))) {
        position_++;
        column_++;
    }
    
    std::string value = source_.substr(start, position_ - start);
    return Token(TokenType::ADDRESS_LITERAL, value, line_, column_ - value.length());
}

bool Lexer::isLetter(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlphanumeric(char c) const {
    return isLetter(c) || isDigit(c) || c == '_';
}

TokenType Lexer::getKeywordType(const std::string& keyword) const {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"contract", TokenType::CONTRACT},
        {"function", TokenType::FUNCTION},
        {"return", TokenType::RETURN},
        {"if", TokenType::IF},
        {"else", TokenType::ELSE},
        {"while", TokenType::WHILE},
        {"for", TokenType::FOR},
        {"var", TokenType::VAR},
        {"const", TokenType::CONST},
        {"public", TokenType::PUBLIC},
        {"private", TokenType::PRIVATE},
        {"view", TokenType::VIEW},
        {"pure", TokenType::PURE},
        {"payable", TokenType::PAYABLE},
        {"uint8", TokenType::UINT8},
        {"uint16", TokenType::UINT16},
        {"uint32", TokenType::UINT32},
        {"uint64", TokenType::UINT64},
        {"uint256", TokenType::UINT256},
        {"int8", TokenType::INT8},
        {"int16", TokenType::INT16},
        {"int32", TokenType::INT32},
        {"int64", TokenType::INT64},
        {"int256", TokenType::INT256},
        {"bool", TokenType::BOOL},
        {"address", TokenType::ADDRESS},
        {"bytes", TokenType::BYTES},
        {"string", TokenType::STRING},
        {"true", TokenType::BOOLEAN},
        {"false", TokenType::BOOLEAN}
    };
    
    auto it = keywords.find(keyword);
    return (it != keywords.end()) ? it->second : TokenType::UNKNOWN;
}

// Parser implementation
Parser::Parser(const std::string& source) 
    : lexer_(std::make_unique<Lexer>(source)), current_token_(TokenType::UNKNOWN, "") {
    advance();
}

std::shared_ptr<ASTNode> Parser::parse() {
    try {
        return parseContract();
    } catch (const CompilationError& e) {
        addError(e.what());
        return nullptr;
    }
}

const std::vector<std::string>& Parser::getErrors() const {
    return errors_;
}

void Parser::advance() {
    current_token_ = lexer_->nextToken();
}

bool Parser::match(TokenType type) const {
    return current_token_.type == type;
}

bool Parser::expect(TokenType type) {
    if (match(type)) {
        advance();
        return true;
    }
    
    addError("Expected " + std::to_string(static_cast<int>(type)) + 
             " but got " + std::to_string(static_cast<int>(current_token_.type)));
    return false;
}

void Parser::addError(const std::string& message) {
    errors_.push_back("Line " + std::to_string(current_token_.line) + 
                     ", Column " + std::to_string(current_token_.column) + ": " + message);
}

std::shared_ptr<ASTNode> Parser::parseContract() {
    if (!expect(TokenType::CONTRACT)) {
        return nullptr;
    }
    
    auto contract = std::make_shared<ASTNode>(ASTNodeType::CONTRACT_DECLARATION, current_token_);
    
    if (!match(TokenType::IDENTIFIER)) {
        addError("Expected contract name");
        return nullptr;
    }
    
    contract->value = current_token_.value;
    advance();
    
    if (!expect(TokenType::LEFT_BRACE)) {
        return nullptr;
    }
    
    while (!match(TokenType::RIGHT_BRACE) && !match(TokenType::EOF_TOKEN)) {
        if (match(TokenType::FUNCTION)) {
            auto function = parseFunction();
            if (function) {
                contract->addChild(function);
            }
        } else if (match(TokenType::VAR) || match(TokenType::CONST)) {
            auto variable = parseVariableDeclaration();
            if (variable) {
                contract->addChild(variable);
            }
        } else {
            addError("Unexpected token in contract");
            advance();
        }
    }
    
    if (!expect(TokenType::RIGHT_BRACE)) {
        return nullptr;
    }
    
    return contract;
}

std::shared_ptr<ASTNode> Parser::parseFunction() {
    if (!expect(TokenType::FUNCTION)) {
        return nullptr;
    }
    
    auto function = std::make_shared<ASTNode>(ASTNodeType::FUNCTION_DECLARATION, current_token_);
    
    if (!match(TokenType::IDENTIFIER)) {
        addError("Expected function name");
        return nullptr;
    }
    
    function->value = current_token_.value;
    advance();
    
    if (!expect(TokenType::LEFT_PAREN)) {
        return nullptr;
    }
    
    auto parameters = parseParameterList();
    for (auto& param : parameters) {
        function->addChild(param);
    }
    
    if (!expect(TokenType::RIGHT_PAREN)) {
        return nullptr;
    }
    
    if (match(TokenType::LEFT_BRACE)) {
        auto body = parseBlock();
        if (body) {
            function->addChild(body);
        }
    }
    
    return function;
}

std::shared_ptr<ASTNode> Parser::parseVariableDeclaration() {
    bool is_const = match(TokenType::CONST);
    advance(); // Consume var or const
    
    auto variable = std::make_shared<ASTNode>(ASTNodeType::VARIABLE_DECLARATION, current_token_);
    variable->value = is_const ? "const" : "var";
    
    if (!match(TokenType::IDENTIFIER)) {
        addError("Expected variable name");
        return nullptr;
    }
    
    auto name = std::make_shared<ASTNode>(ASTNodeType::IDENTIFIER, current_token_);
    name->value = current_token_.value;
    variable->addChild(name);
    advance();
    
    if (match(TokenType::ASSIGN)) {
        advance(); // Consume =
        auto value = parseExpression();
        if (value) {
            variable->addChild(value);
        }
    }
    
    if (!expect(TokenType::SEMICOLON)) {
        return nullptr;
    }
    
    return variable;
}

std::shared_ptr<ASTNode> Parser::parseStatement() {
    if (match(TokenType::IF)) {
        return parseIfStatement();
    } else if (match(TokenType::WHILE)) {
        return parseWhileStatement();
    } else if (match(TokenType::FOR)) {
        return parseForStatement();
    } else if (match(TokenType::RETURN)) {
        return parseReturnStatement();
    } else if (match(TokenType::LEFT_BRACE)) {
        return parseBlock();
    } else if (match(TokenType::VAR) || match(TokenType::CONST)) {
        return parseVariableDeclaration();
    } else {
        auto expr = parseExpression();
        if (expr && expect(TokenType::SEMICOLON)) {
            auto stmt = std::make_shared<ASTNode>(ASTNodeType::EXPRESSION_STATEMENT, current_token_);
            stmt->addChild(expr);
            return stmt;
        }
        return nullptr;
    }
}

std::shared_ptr<ASTNode> Parser::parseExpression() {
    return parsePrimary();
}

std::shared_ptr<ASTNode> Parser::parsePrimary() {
    if (match(TokenType::IDENTIFIER)) {
        auto identifier = std::make_shared<ASTNode>(ASTNodeType::IDENTIFIER, current_token_);
        identifier->value = current_token_.value;
        advance();
        
        if (match(TokenType::LEFT_PAREN)) {
            return parseFunctionCall();
        }
        
        return identifier;
    } else if (match(TokenType::NUMBER) || match(TokenType::STRING_LITERAL) || 
               match(TokenType::BOOLEAN) || match(TokenType::ADDRESS_LITERAL)) {
        auto literal = std::make_shared<ASTNode>(ASTNodeType::LITERAL, current_token_);
        literal->value = current_token_.value;
        advance();
        return literal;
    } else if (match(TokenType::LEFT_PAREN)) {
        advance(); // Consume (
        auto expr = parseExpression();
        if (expr && expect(TokenType::RIGHT_PAREN)) {
            return expr;
        }
        return nullptr;
    }
    
    addError("Unexpected token in expression");
    return nullptr;
}

std::shared_ptr<ASTNode> Parser::parseBlock() {
    if (!expect(TokenType::LEFT_BRACE)) {
        return nullptr;
    }
    
    auto block = std::make_shared<ASTNode>(ASTNodeType::BLOCK, current_token_);
    
    while (!match(TokenType::RIGHT_BRACE) && !match(TokenType::EOF_TOKEN)) {
        auto stmt = parseStatement();
        if (stmt) {
            block->addChild(stmt);
        }
    }
    
    if (!expect(TokenType::RIGHT_BRACE)) {
        return nullptr;
    }
    
    return block;
}

std::shared_ptr<ASTNode> Parser::parseIfStatement() {
    if (!expect(TokenType::IF)) {
        return nullptr;
    }
    
    auto if_stmt = std::make_shared<ASTNode>(ASTNodeType::IF_STATEMENT, current_token_);
    
    if (!expect(TokenType::LEFT_PAREN)) {
        return nullptr;
    }
    
    auto condition = parseExpression();
    if (condition) {
        if_stmt->addChild(condition);
    }
    
    if (!expect(TokenType::RIGHT_PAREN)) {
        return nullptr;
    }
    
    auto then_body = parseStatement();
    if (then_body) {
        if_stmt->addChild(then_body);
    }
    
    if (match(TokenType::ELSE)) {
        advance(); // Consume else
        auto else_body = parseStatement();
        if (else_body) {
            if_stmt->addChild(else_body);
        }
    }
    
    return if_stmt;
}

std::shared_ptr<ASTNode> Parser::parseWhileStatement() {
    if (!expect(TokenType::WHILE)) {
        return nullptr;
    }
    
    auto while_stmt = std::make_shared<ASTNode>(ASTNodeType::WHILE_STATEMENT, current_token_);
    
    if (!expect(TokenType::LEFT_PAREN)) {
        return nullptr;
    }
    
    auto condition = parseExpression();
    if (condition) {
        while_stmt->addChild(condition);
    }
    
    if (!expect(TokenType::RIGHT_PAREN)) {
        return nullptr;
    }
    
    auto body = parseStatement();
    if (body) {
        while_stmt->addChild(body);
    }
    
    return while_stmt;
}

std::shared_ptr<ASTNode> Parser::parseForStatement() {
    if (!expect(TokenType::FOR)) {
        return nullptr;
    }
    
    auto for_stmt = std::make_shared<ASTNode>(ASTNodeType::FOR_STATEMENT, current_token_);
    
    if (!expect(TokenType::LEFT_PAREN)) {
        return nullptr;
    }
    
    // Parse initialization
    if (!match(TokenType::SEMICOLON)) {
        auto init = parseStatement();
        if (init) {
            for_stmt->addChild(init);
        }
    }
    
    // Parse condition
    if (!match(TokenType::SEMICOLON)) {
        auto condition = parseExpression();
        if (condition) {
            for_stmt->addChild(condition);
        }
    }
    
    if (!expect(TokenType::SEMICOLON)) {
        return nullptr;
    }
    
    // Parse increment
    if (!match(TokenType::RIGHT_PAREN)) {
        auto increment = parseExpression();
        if (increment) {
            for_stmt->addChild(increment);
        }
    }
    
    if (!expect(TokenType::RIGHT_PAREN)) {
        return nullptr;
    }
    
    auto body = parseStatement();
    if (body) {
        for_stmt->addChild(body);
    }
    
    return for_stmt;
}

std::shared_ptr<ASTNode> Parser::parseReturnStatement() {
    if (!expect(TokenType::RETURN)) {
        return nullptr;
    }
    
    auto return_stmt = std::make_shared<ASTNode>(ASTNodeType::RETURN_STATEMENT, current_token_);
    
    if (!match(TokenType::SEMICOLON)) {
        auto expr = parseExpression();
        if (expr) {
            return_stmt->addChild(expr);
        }
    }
    
    if (!expect(TokenType::SEMICOLON)) {
        return nullptr;
    }
    
    return return_stmt;
}

std::shared_ptr<ASTNode> Parser::parseFunctionCall() {
    auto call = std::make_shared<ASTNode>(ASTNodeType::FUNCTION_CALL, current_token_);
    
    if (!expect(TokenType::LEFT_PAREN)) {
        return nullptr;
    }
    
    auto arguments = parseArgumentList();
    for (auto& arg : arguments) {
        call->addChild(arg);
    }
    
    if (!expect(TokenType::RIGHT_PAREN)) {
        return nullptr;
    }
    
    return call;
}

std::vector<std::shared_ptr<ASTNode>> Parser::parseParameterList() {
    std::vector<std::shared_ptr<ASTNode>> parameters;
    
    if (!match(TokenType::RIGHT_PAREN)) {
        do {
            if (match(TokenType::IDENTIFIER)) {
                auto param = std::make_shared<ASTNode>(ASTNodeType::IDENTIFIER, current_token_);
                param->value = current_token_.value;
                parameters.push_back(param);
                advance();
            }
        } while (match(TokenType::COMMA) && (advance(), true));
    }
    
    return parameters;
}

std::vector<std::shared_ptr<ASTNode>> Parser::parseArgumentList() {
    std::vector<std::shared_ptr<ASTNode>> arguments;
    
    if (!match(TokenType::RIGHT_PAREN)) {
        do {
            auto arg = parseExpression();
            if (arg) {
                arguments.push_back(arg);
            }
        } while (match(TokenType::COMMA) && (advance(), true));
    }
    
    return arguments;
}

int Parser::getPrecedence(TokenType op) const {
    switch (op) {
        case TokenType::OR:
            return 1;
        case TokenType::AND:
            return 2;
        case TokenType::EQUAL:
        case TokenType::NOT_EQUAL:
            return 3;
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
            return 4;
        case TokenType::PLUS:
        case TokenType::MINUS:
            return 5;
        case TokenType::MULTIPLY:
        case TokenType::DIVIDE:
        case TokenType::MODULO:
            return 6;
        default:
            return 0;
    }
}

// CodeGenerator implementation
CodeGenerator::CodeGenerator(const CompilationConfig& config) 
    : config_(config), gas_estimate_(0), variable_count_(0) {
}

std::vector<uint8_t> CodeGenerator::generateBytecode(std::shared_ptr<ASTNode> ast) {
    bytecode_.clear();
    errors_.clear();
    gas_estimate_ = 0;
    labels_.clear();
    variables_.clear();
    variable_count_ = 0;
    
    if (!ast) {
        addError("Invalid AST");
        return bytecode_;
    }
    
    try {
        generateContract(ast);
    } catch (const CompilationError& e) {
        addError(e.what());
    }
    
    return bytecode_;
}

std::string CodeGenerator::generateABI(std::shared_ptr<ASTNode> ast) {
    nlohmann::json abi = nlohmann::json::array();
    
    if (!ast || ast->type != ASTNodeType::CONTRACT_DECLARATION) {
        return abi.dump(2);
    }
    
    for (size_t i = 0; i < ast->getChildCount(); ++i) {
        auto child = ast->getChild(i);
        if (child && child->type == ASTNodeType::FUNCTION_DECLARATION) {
            nlohmann::json function;
            function["type"] = "function";
            function["name"] = child->value;
            function["inputs"] = nlohmann::json::array();
            function["outputs"] = nlohmann::json::array();
            function["stateMutability"] = "nonpayable";
            
            abi.push_back(function);
        }
    }
    
    return abi.dump(2);
}

const std::vector<std::string>& CodeGenerator::getErrors() const {
    return errors_;
}

uint64_t CodeGenerator::getGasEstimate() const {
    return gas_estimate_;
}

void CodeGenerator::addInstruction(vm::Opcode opcode) {
    bytecode_.push_back(static_cast<uint8_t>(opcode));
    gas_estimate_ += estimateGasForInstruction(opcode);
}

void CodeGenerator::addInstruction(vm::Opcode opcode, uint32_t operand) {
    bytecode_.push_back(static_cast<uint8_t>(opcode));
    
    // Add operand as little-endian bytes
    bytecode_.push_back(operand & 0xFF);
    bytecode_.push_back((operand >> 8) & 0xFF);
    bytecode_.push_back((operand >> 16) & 0xFF);
    bytecode_.push_back((operand >> 24) & 0xFF);
    
    gas_estimate_ += estimateGasForInstruction(opcode);
}

void CodeGenerator::addData(const std::vector<uint8_t>& data) {
    bytecode_.insert(bytecode_.end(), data.begin(), data.end());
}

void CodeGenerator::addError(const std::string& message) {
    errors_.push_back(message);
}

void CodeGenerator::generateContract(std::shared_ptr<ASTNode> contract) {
    if (!contract || contract->type != ASTNodeType::CONTRACT_DECLARATION) {
        addError("Invalid contract node");
        return;
    }
    
    // Generate contract prologue
    addInstruction(vm::Opcode::PUSH1, 0); // Contract initialization
    
    // Generate code for each function
    for (size_t i = 0; i < contract->getChildCount(); ++i) {
        auto child = contract->getChild(i);
        if (child && child->type == ASTNodeType::FUNCTION_DECLARATION) {
            generateFunction(child);
        }
    }
    
    // Generate contract epilogue
    addInstruction(vm::Opcode::RETURN);
}

void CodeGenerator::generateFunction(std::shared_ptr<ASTNode> function) {
    if (!function || function->type != ASTNodeType::FUNCTION_DECLARATION) {
        addError("Invalid function node");
        return;
    }
    
    // Generate function prologue
    addInstruction(vm::Opcode::PUSH1, 0); // Push 0 for return value
    
    // Generate function body
    if (function->getChildCount() > 0) {
        auto body = function->getChild(function->getChildCount() - 1);
        if (body && body->type == ASTNodeType::BLOCK) {
            generateStatement(body);
        }
    } else {
        // Generate default function body if none exists
        addInstruction(vm::Opcode::PUSH1, 42); // Default return value
    }
    
    // Generate function epilogue
    addInstruction(vm::Opcode::RETURN);
}

void CodeGenerator::generateStatement(std::shared_ptr<ASTNode> statement) {
    if (!statement) {
        return;
    }
    
    switch (statement->type) {
        case ASTNodeType::BLOCK:
            for (size_t i = 0; i < statement->getChildCount(); ++i) {
                generateStatement(statement->getChild(i));
            }
            break;
            
        case ASTNodeType::EXPRESSION_STATEMENT:
            if (statement->getChildCount() > 0) {
                generateExpression(statement->getChild(0));
            }
            break;
            
        case ASTNodeType::RETURN_STATEMENT:
            if (statement->getChildCount() > 0) {
                generateExpression(statement->getChild(0));
            }
            addInstruction(vm::Opcode::RETURN);
            break;
            
        default:
            addError("Unsupported statement type");
            break;
    }
}

void CodeGenerator::generateExpression(std::shared_ptr<ASTNode> expression) {
    if (!expression) {
        return;
    }
    
    switch (expression->type) {
        case ASTNodeType::LITERAL:
            generateLiteral(expression);
            break;
            
        case ASTNodeType::IDENTIFIER:
            generateVariableAccess(expression);
            break;
            
        case ASTNodeType::FUNCTION_CALL:
            generateFunctionCall(expression);
            break;
            
        default:
            addError("Unsupported expression type");
            break;
    }
}

void CodeGenerator::generateLiteral(std::shared_ptr<ASTNode> literal) {
    if (!literal || literal->type != ASTNodeType::LITERAL) {
        return;
    }
    
    if (literal->token.type == TokenType::NUMBER) {
        uint32_t value = std::stoul(literal->value);
        addInstruction(vm::Opcode::PUSH1, value);
    } else if (literal->token.type == TokenType::BOOLEAN) {
        if (literal->value == "true") {
            addInstruction(vm::Opcode::PUSH1, 1);
        } else {
            addInstruction(vm::Opcode::PUSH1, 0);
        }
    } else {
        addError("Unsupported literal type");
    }
}

void CodeGenerator::generateVariableAccess(std::shared_ptr<ASTNode> variable) {
    if (!variable || variable->type != ASTNodeType::IDENTIFIER) {
        return;
    }
    
    uint32_t index = getVariableIndex(variable->value);
    addInstruction(vm::Opcode::PUSH1, index);
    addInstruction(vm::Opcode::MLOAD);
}

void CodeGenerator::generateFunctionCall(std::shared_ptr<ASTNode> call) {
    if (!call || call->type != ASTNodeType::FUNCTION_CALL) {
        return;
    }
    
    // Generate arguments
    for (size_t i = 0; i < call->getChildCount(); ++i) {
        generateExpression(call->getChild(i));
    }
    
    // Call function (simplified)
    addInstruction(vm::Opcode::CALL);
}

uint32_t CodeGenerator::getVariableIndex(const std::string& name) {
    auto it = variables_.find(name);
    if (it == variables_.end()) {
        variables_[name] = variable_count_++;
        return variable_count_ - 1;
    }
    return it->second;
}

uint32_t CodeGenerator::createLabel(const std::string& name) {
    uint32_t address = static_cast<uint32_t>(bytecode_.size());
    labels_[name] = address;
    return address;
}

uint32_t CodeGenerator::resolveLabel(const std::string& name) {
    auto it = labels_.find(name);
    if (it != labels_.end()) {
        return it->second;
    }
    return 0;
}

vm::Opcode CodeGenerator::getOpcodeForOperator(TokenType op) const {
    switch (op) {
        case TokenType::PLUS:
            return vm::Opcode::ADD;
        case TokenType::MINUS:
            return vm::Opcode::SUB;
        case TokenType::MULTIPLY:
            return vm::Opcode::MUL;
        case TokenType::DIVIDE:
            return vm::Opcode::DIV;
        case TokenType::MODULO:
            return vm::Opcode::MOD;
        case TokenType::EQUAL:
            return vm::Opcode::EQ;
        case TokenType::NOT_EQUAL:
            return vm::Opcode::EQ; // Will need to negate
        case TokenType::LESS:
            return vm::Opcode::LT;
        case TokenType::GREATER:
            return vm::Opcode::GT;
        default:
            return vm::Opcode::INVALID;
    }
}

vm::Opcode CodeGenerator::getOpcodeForType(TokenType type) const {
    switch (type) {
        case TokenType::UINT8:
        case TokenType::UINT16:
        case TokenType::UINT32:
        case TokenType::UINT64:
        case TokenType::UINT256:
            return vm::Opcode::PUSH1;
        case TokenType::BOOL:
            return vm::Opcode::PUSH1;
        case TokenType::ADDRESS:
            return vm::Opcode::PUSH1;
        default:
            return vm::Opcode::PUSH1;
    }
}

uint64_t CodeGenerator::estimateGasForInstruction(vm::Opcode opcode) const {
    switch (opcode) {
        case vm::Opcode::INVALID:
            return 0;
        case vm::Opcode::ADD:
        case vm::Opcode::SUB:
        case vm::Opcode::MUL:
        case vm::Opcode::DIV:
        case vm::Opcode::MOD:
            return 3;
        case vm::Opcode::PUSH1:
            return 3;
        case vm::Opcode::POP:
            return 2;
        case vm::Opcode::MLOAD:
            return 3;
        case vm::Opcode::MSTORE:
            return 3;
        case vm::Opcode::RETURN:
            return 0;
        default:
            return 1;
    }
}

// ContractCompiler implementation
ContractCompiler::ContractCompiler(const CompilationConfig& config) 
    : config_(config), parser_(nullptr), generator_(std::make_unique<CodeGenerator>(config)) {
}

CompilationResult ContractCompiler::compile(const std::string& source) {
    CompilationResult result;
    
    try {
        // Parse source code
        parser_ = std::make_unique<Parser>(source);
        auto ast = parser_->parse();
        
        if (!ast) {
            result.errors = parser_->getErrors();
            return result;
        }
        
        // Generate bytecode
        result.bytecode = generator_->generateBytecode(ast);
        if (!generator_->getErrors().empty()) {
            result.errors = generator_->getErrors();
            return result;
        }
        
        // Generate ABI
        result.abi = generator_->generateABI(ast);
        
        // Calculate metadata
        result.contract_size = static_cast<uint32_t>(result.bytecode.size());
        result.gas_estimate = generator_->getGasEstimate();
        result.contract_hash = calculateContractHash(result.bytecode);
        result.metadata = generateMetadata(result);
        
        result.success = true;
        
    } catch (const CompilationError& e) {
        result.errors.push_back(e.what());
    } catch (const std::exception& e) {
        result.errors.push_back("Compilation error: " + std::string(e.what()));
    }
    
    return result;
}

CompilationResult ContractCompiler::compileFromFile(const std::string& filename) {
    try {
        std::string source = readFile(filename);
        return compile(source);
    } catch (const std::exception& e) {
        CompilationResult result;
        result.errors.push_back("File error: " + std::string(e.what()));
        return result;
    }
}

void ContractCompiler::setConfig(const CompilationConfig& config) {
    config_ = config;
    generator_ = std::make_unique<CodeGenerator>(config);
}

const CompilationConfig& ContractCompiler::getConfig() const {
    return config_;
}

bool ContractCompiler::validateSource(const std::string& source) {
    try {
        parser_ = std::make_unique<Parser>(source);
        auto ast = parser_->parse();
        return ast != nullptr && parser_->getErrors().empty();
    } catch (const std::exception& e) {
        return false;
    }
}

std::vector<std::string> ContractCompiler::getSupportedFeatures() const {
    return {
        "contract_declaration",
        "function_declaration",
        "variable_declaration",
        "arithmetic_operations",
        "logical_operations",
        "control_flow",
        "function_calls",
        "literals",
        "comments"
    };
}

std::string ContractCompiler::getVersion() const {
    return "1.0.0";
}

std::string ContractCompiler::readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string ContractCompiler::calculateContractHash(const std::vector<uint8_t>& bytecode) {
    return deo::crypto::Hash::sha256(std::string(bytecode.begin(), bytecode.end()));
}

std::string ContractCompiler::generateMetadata(const CompilationResult& result) {
    nlohmann::json metadata;
    
    metadata["version"] = getVersion();
    metadata["contract_size"] = result.contract_size;
    metadata["gas_estimate"] = result.gas_estimate;
    metadata["contract_hash"] = result.contract_hash;
    metadata["compilation_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return metadata.dump(2);
}

} // namespace compiler
} // namespace deo
