#include "interpreter/lexer.h"
#include <sstream>
#include <cctype>
#include <algorithm>

namespace visionpipe {

// ============================================================================
// Keyword lookup table
// ============================================================================
const std::unordered_map<std::string, TokenType> Lexer::_keywords = {
    {"pipeline", TokenType::KW_PIPELINE},
    {"end", TokenType::KW_END},
    {"let", TokenType::KW_LET},
    {"global", TokenType::KW_GLOBAL},
    {"cache", TokenType::KW_CACHE},
    {"exec_seq", TokenType::KW_EXEC_SEQ},
    {"exec_multi", TokenType::KW_EXEC_MULTI},
    {"exec_loop", TokenType::KW_EXEC_LOOP},
    {"use", TokenType::KW_USE},
    {"if", TokenType::KW_IF},
    {"else", TokenType::KW_ELSE},
    {"while", TokenType::KW_WHILE},
    {"break", TokenType::KW_BREAK},
    {"continue", TokenType::KW_CONTINUE},
    {"return", TokenType::KW_RETURN},
    {"import", TokenType::KW_IMPORT},
    {"config", TokenType::KW_CONFIG},
    {"true", TokenType::BOOLEAN_LITERAL},
    {"false", TokenType::BOOLEAN_LITERAL},
    {"and", TokenType::OP_AND},
    {"or", TokenType::OP_OR},
    {"not", TokenType::OP_NOT}
};

// ============================================================================
// SourceLocation
// ============================================================================
std::string SourceLocation::toString() const {
    std::ostringstream oss;
    oss << filename << ":" << line << ":" << column;
    return oss.str();
}

// ============================================================================
// Token
// ============================================================================
std::string Token::asString() const {
    if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    return raw;
}

double Token::asNumber() const {
    if (std::holds_alternative<double>(value)) {
        return std::get<double>(value);
    }
    throw std::runtime_error("Token is not a number");
}

bool Token::asBool() const {
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value);
    }
    throw std::runtime_error("Token is not a boolean");
}

bool Token::isKeyword() const {
    return type >= TokenType::KW_PIPELINE && type <= TokenType::KW_CONFIG;
}

bool Token::isOperator() const {
    return type >= TokenType::OP_ARROW && type <= TokenType::OP_NOT;
}

bool Token::isLiteral() const {
    return type >= TokenType::IDENTIFIER && type <= TokenType::BOOLEAN_LITERAL;
}

std::string Token::typeToString(TokenType type) {
    switch (type) {
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::STRING_LITERAL: return "STRING";
        case TokenType::NUMBER_LITERAL: return "NUMBER";
        case TokenType::BOOLEAN_LITERAL: return "BOOLEAN";
        case TokenType::KW_PIPELINE: return "PIPELINE";
        case TokenType::KW_END: return "END";
        case TokenType::KW_LET: return "LET";
        case TokenType::KW_GLOBAL: return "GLOBAL";
        case TokenType::KW_CACHE: return "CACHE";
        case TokenType::KW_EXEC_SEQ: return "EXEC_SEQ";
        case TokenType::KW_EXEC_MULTI: return "EXEC_MULTI";
        case TokenType::KW_EXEC_LOOP: return "EXEC_LOOP";
        case TokenType::KW_USE: return "USE";
        case TokenType::KW_IF: return "IF";
        case TokenType::KW_ELSE: return "ELSE";
        case TokenType::KW_WHILE: return "WHILE";
        case TokenType::KW_BREAK: return "BREAK";
        case TokenType::KW_CONTINUE: return "CONTINUE";
        case TokenType::KW_RETURN: return "RETURN";
        case TokenType::KW_IMPORT: return "IMPORT";
        case TokenType::KW_CONFIG: return "CONFIG";
        case TokenType::OP_ARROW: return "ARROW";
        case TokenType::OP_PIPE: return "PIPE";
        case TokenType::OP_ASSIGN: return "ASSIGN";
        case TokenType::OP_COMMA: return "COMMA";
        case TokenType::OP_DOT: return "DOT";
        case TokenType::OP_COLON: return "COLON";
        case TokenType::OP_EQ: return "EQ";
        case TokenType::OP_NE: return "NE";
        case TokenType::OP_LT: return "LT";
        case TokenType::OP_LE: return "LE";
        case TokenType::OP_GT: return "GT";
        case TokenType::OP_GE: return "GE";
        case TokenType::OP_PLUS: return "PLUS";
        case TokenType::OP_MINUS: return "MINUS";
        case TokenType::OP_MULTIPLY: return "MULTIPLY";
        case TokenType::OP_DIVIDE: return "DIVIDE";
        case TokenType::OP_MODULO: return "MODULO";
        case TokenType::OP_AND: return "AND";
        case TokenType::OP_OR: return "OR";
        case TokenType::OP_NOT: return "NOT";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::COMMENT: return "COMMENT";
        case TokenType::DOC_COMMENT: return "DOC_COMMENT";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string Token::toString() const {
    std::ostringstream oss;
    oss << "Token(" << typeToString(type);
    if (!raw.empty()) {
        oss << ", \"" << raw << "\"";
    }
    oss << ", " << location.toString() << ")";
    return oss.str();
}

// ============================================================================
// LexerError
// ============================================================================
LexerError::LexerError(const std::string& message, const SourceLocation& loc)
    : std::runtime_error(loc.toString() + ": " + message)
    , _location(loc) {}

// ============================================================================
// Lexer
// ============================================================================
Lexer::Lexer(const std::string& source, const std::string& filename)
    : Lexer(source, filename, LexerConfig{}) {}

Lexer::Lexer(const std::string& source, const std::string& filename, LexerConfig config)
    : _source(source)
    , _filename(filename)
    , _config(config)
    , _pos(0)
    , _line(1)
    , _column(1) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!isAtEnd()) {
        Token token = nextToken();
        
        // Filter based on config
        if (token.type == TokenType::COMMENT && !_config.preserveComments) {
            continue;
        }
        if (token.type == TokenType::NEWLINE && !_config.preserveNewlines) {
            continue;
        }
        
        tokens.push_back(token);
        
        if (token.type == TokenType::END_OF_FILE) {
            break;
        }
    }
    
    return tokens;
}

Token Lexer::nextToken() {
    if (_peeked.has_value()) {
        Token token = std::move(*_peeked);
        _peeked.reset();
        return token;
    }
    
    skipWhitespace();
    
    if (isAtEnd()) {
        return makeToken(TokenType::END_OF_FILE);
    }
    
    return scanToken();
}

Token Lexer::peekToken() {
    if (!_peeked.has_value()) {
        _peeked = nextToken();
    }
    return *_peeked;
}

bool Lexer::isAtEnd() const {
    return _pos >= _source.size();
}

SourceLocation Lexer::currentLocation() const {
    return SourceLocation{_filename, _line, _column, _pos};
}

std::string Lexer::getLineContent(size_t lineNumber) const {
    size_t currentLine = 1;
    size_t lineStart = 0;
    
    for (size_t i = 0; i < _source.size(); ++i) {
        if (currentLine == lineNumber) {
            lineStart = i;
            break;
        }
        if (_source[i] == '\n') {
            ++currentLine;
        }
    }
    
    size_t lineEnd = lineStart;
    while (lineEnd < _source.size() && _source[lineEnd] != '\n') {
        ++lineEnd;
    }
    
    return _source.substr(lineStart, lineEnd - lineStart);
}

char Lexer::current() const {
    if (isAtEnd()) return '\0';
    return _source[_pos];
}

char Lexer::peek(size_t ahead) const {
    size_t pos = _pos + ahead;
    if (pos >= _source.size()) return '\0';
    return _source[pos];
}

char Lexer::advance() {
    char c = current();
    ++_pos;
    
    if (c == '\n') {
        ++_line;
        _column = 1;
    } else {
        ++_column;
    }
    
    return c;
}

bool Lexer::match(char expected) {
    if (current() == expected) {
        advance();
        return true;
    }
    return false;
}

bool Lexer::match(const std::string& expected) {
    if (_pos + expected.size() > _source.size()) {
        return false;
    }
    
    for (size_t i = 0; i < expected.size(); ++i) {
        if (_source[_pos + i] != expected[i]) {
            return false;
        }
    }
    
    for (size_t i = 0; i < expected.size(); ++i) {
        advance();
    }
    return true;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = current();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance();
                break;
            case '\n':
                if (_config.preserveNewlines) {
                    return; // Let newline be tokenized
                }
                advance();
                break;
            default:
                return;
        }
    }
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && current() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    // Block comments: /* ... */
    advance(); // consume /
    advance(); // consume *
    
    while (!isAtEnd()) {
        if (current() == '*' && peek() == '/') {
            advance(); // consume *
            advance(); // consume /
            return;
        }
        advance();
    }
    
    throw error("Unterminated block comment");
}

Token Lexer::scanToken() {
    SourceLocation startLoc = currentLocation();
    char c = current();
    
    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return scanIdentifierOrKeyword();
    }
    
    // Numbers
    if (std::isdigit(c)) {
        return scanNumber();
    }
    
    // Negative numbers
    if (c == '-' && std::isdigit(peek())) {
        return scanNumber();
    }
    
    // Strings
    if (c == '"' || c == '\'') {
        return scanString(c);
    }
    
    // Comments
    if (c == '#') {
        return scanComment();
    }
    
    // Block comments
    if (c == '/' && peek() == '*') {
        skipBlockComment();
        return nextToken(); // Get next real token
    }
    
    // Line comment (alternative)
    if (c == '/' && peek() == '/') {
        skipLineComment();
        return nextToken();
    }
    
    // Newline (if preserving)
    if (c == '\n' && _config.preserveNewlines) {
        advance();
        return makeToken(TokenType::NEWLINE, "\n");
    }
    
    // Operators and delimiters
    return scanOperator();
}

Token Lexer::scanIdentifierOrKeyword() {
    SourceLocation startLoc = currentLocation();
    std::string identifier;
    
    while (!isAtEnd()) {
        char c = current();
        if (std::isalnum(c) || c == '_') {
            identifier += advance();
        } else {
            break;
        }
    }
    
    // Check if it's a keyword
    auto it = _keywords.find(identifier);
    if (it != _keywords.end()) {
        TokenType type = it->second;
        
        // Special handling for boolean literals
        if (type == TokenType::BOOLEAN_LITERAL) {
            bool value = (identifier == "true");
            Token token(type, startLoc, identifier);
            token.value = value;
            return token;
        }
        
        return Token(type, startLoc, identifier);
    }
    
    // It's an identifier
    Token token(TokenType::IDENTIFIER, startLoc, identifier);
    token.value = identifier;
    return token;
}

Token Lexer::scanNumber() {
    SourceLocation startLoc = currentLocation();
    std::string number;
    
    // Handle negative sign
    if (current() == '-') {
        number += advance();
    }
    
    // Integer part
    while (!isAtEnd() && std::isdigit(current())) {
        number += advance();
    }
    
    // Fractional part
    if (current() == '.' && std::isdigit(peek())) {
        number += advance(); // consume '.'
        while (!isAtEnd() && std::isdigit(current())) {
            number += advance();
        }
    }
    
    // Exponent part
    if (current() == 'e' || current() == 'E') {
        number += advance();
        if (current() == '+' || current() == '-') {
            number += advance();
        }
        while (!isAtEnd() && std::isdigit(current())) {
            number += advance();
        }
    }
    
    double value = std::stod(number);
    Token token(TokenType::NUMBER_LITERAL, startLoc, number);
    token.value = value;
    return token;
}

Token Lexer::scanString(char quote) {
    SourceLocation startLoc = currentLocation();
    advance(); // consume opening quote
    
    std::string str;
    
    while (!isAtEnd() && current() != quote) {
        if (current() == '\n') {
            throw error("Unterminated string literal");
        }
        
        // Handle escape sequences
        if (current() == '\\') {
            advance(); // consume backslash
            if (isAtEnd()) {
                throw error("Unterminated string literal");
            }
            
            char escaped = advance();
            switch (escaped) {
                case 'n': str += '\n'; break;
                case 'r': str += '\r'; break;
                case 't': str += '\t'; break;
                case '\\': str += '\\'; break;
                case '"': str += '"'; break;
                case '\'': str += '\''; break;
                case '0': str += '\0'; break;
                default:
                    throw error("Unknown escape sequence: \\" + std::string(1, escaped));
            }
        } else {
            str += advance();
        }
    }
    
    if (isAtEnd()) {
        throw error("Unterminated string literal");
    }
    
    advance(); // consume closing quote
    
    Token token(TokenType::STRING_LITERAL, startLoc, str);
    token.value = str;
    return token;
}

Token Lexer::scanOperator() {
    SourceLocation startLoc = currentLocation();
    char c = advance();
    
    switch (c) {
        case '(': return Token(TokenType::LPAREN, startLoc, "(");
        case ')': return Token(TokenType::RPAREN, startLoc, ")");
        case '[': return Token(TokenType::LBRACKET, startLoc, "[");
        case ']': return Token(TokenType::RBRACKET, startLoc, "]");
        case '{': return Token(TokenType::LBRACE, startLoc, "{");
        case '}': return Token(TokenType::RBRACE, startLoc, "}");
        case ',': return Token(TokenType::OP_COMMA, startLoc, ",");
        case '.': return Token(TokenType::OP_DOT, startLoc, ".");
        case ':': return Token(TokenType::OP_COLON, startLoc, ":");
        case '+': return Token(TokenType::OP_PLUS, startLoc, "+");
        case '*': return Token(TokenType::OP_MULTIPLY, startLoc, "*");
        case '/': return Token(TokenType::OP_DIVIDE, startLoc, "/");
        case '%': return Token(TokenType::OP_MODULO, startLoc, "%");
        case '|':
            if (match('|')) return Token(TokenType::OP_OR, startLoc, "||");
            return Token(TokenType::OP_PIPE, startLoc, "|");
        case '&':
            if (match('&')) return Token(TokenType::OP_AND, startLoc, "&&");
            throw error("Unexpected character '&'. Did you mean '&&'?");
        case '!':
            if (match('=')) return Token(TokenType::OP_NE, startLoc, "!=");
            return Token(TokenType::OP_NOT, startLoc, "!");
        case '=':
            if (match('=')) return Token(TokenType::OP_EQ, startLoc, "==");
            return Token(TokenType::OP_ASSIGN, startLoc, "=");
        case '<':
            if (match('=')) return Token(TokenType::OP_LE, startLoc, "<=");
            return Token(TokenType::OP_LT, startLoc, "<");
        case '>':
            if (match('=')) return Token(TokenType::OP_GE, startLoc, ">=");
            return Token(TokenType::OP_GT, startLoc, ">");
        case '-':
            if (match('>')) return Token(TokenType::OP_ARROW, startLoc, "->");
            return Token(TokenType::OP_MINUS, startLoc, "-");
        default:
            throw error("Unexpected character: '" + std::string(1, c) + "'");
    }
}

Token Lexer::scanComment() {
    SourceLocation startLoc = currentLocation();
    advance(); // consume first #
    
    bool isDocComment = (current() == '#');
    if (isDocComment) {
        advance(); // consume second #
    }
    
    std::string comment;
    while (!isAtEnd() && current() != '\n') {
        comment += advance();
    }
    
    // Trim leading whitespace
    size_t start = comment.find_first_not_of(" \t");
    if (start != std::string::npos) {
        comment = comment.substr(start);
    }
    
    TokenType type = isDocComment ? TokenType::DOC_COMMENT : TokenType::COMMENT;
    Token token(type, startLoc, comment);
    token.value = comment;
    return token;
}

Token Lexer::makeToken(TokenType type, const std::string& raw) {
    return Token(type, currentLocation(), raw);
}

Token Lexer::makeToken(TokenType type, TokenValue value, const std::string& raw) {
    Token token(type, currentLocation(), raw);
    token.value = value;
    return token;
}

LexerError Lexer::error(const std::string& message) {
    return LexerError(message, currentLocation());
}

} // namespace visionpipe
