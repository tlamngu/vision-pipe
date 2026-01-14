#ifndef VISIONPIPE_LEXER_H
#define VISIONPIPE_LEXER_H

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <unordered_map>
#include <stdexcept>

namespace visionpipe {

/**
 * @brief Token types for the VisionPipe Script (.vsp) language
 */
enum class TokenType {
    // Literals
    IDENTIFIER,         // function names, variable names
    STRING_LITERAL,     // "text" or 'text'
    NUMBER_LITERAL,     // 123, 3.14, -5
    BOOLEAN_LITERAL,    // true, false
    
    // Keywords
    KW_PIPELINE,        // pipeline
    KW_END,             // end
    KW_LET,             // let (parameter declaration)
    KW_GLOBAL,          // global (global cache declaration)
    KW_CACHE,           // cache (explicit cache operation)
    KW_EXEC_SEQ,        // exec_seq (execute sequential)
    KW_EXEC_MULTI,      // exec_multi (execute parallel/threaded)
    KW_EXEC_LOOP,       // exec_loop (execute in loop)
    KW_USE,             // use (load from cache)
    KW_IF,              // if (conditional)
    KW_ELSE,            // else
    KW_WHILE,           // while (loop)
    KW_BREAK,           // break
    KW_CONTINUE,        // continue
    KW_RETURN,          // return
    KW_IMPORT,          // import (import other vsp files)
    KW_CONFIG,          // config (configuration block)
    
    // Operators
    OP_ARROW,           // -> (cache output)
    OP_PIPE,            // | (pipe operator, alternative chaining)
    OP_ASSIGN,          // = (assignment)
    OP_COMMA,           // ,
    OP_DOT,             // . (member access)
    OP_COLON,           // : (type annotation)
    
    // Comparison operators
    OP_EQ,              // ==
    OP_NE,              // !=
    OP_LT,              // <
    OP_LE,              // <=
    OP_GT,              // >
    OP_GE,              // >=
    
    // Arithmetic operators
    OP_PLUS,            // +
    OP_MINUS,           // -
    OP_MULTIPLY,        // *
    OP_DIVIDE,          // /
    OP_MODULO,          // %
    
    // Logical operators
    OP_AND,             // && or and
    OP_OR,              // || or or
    OP_NOT,             // ! or not
    
    // Delimiters
    LPAREN,             // (
    RPAREN,             // )
    LBRACKET,           // [
    RBRACKET,           // ]
    LBRACE,             // {
    RBRACE,             // }
    NEWLINE,            // \n (significant in some contexts)
    
    // Comments (kept for documentation)
    COMMENT,            // # single-line comment
    DOC_COMMENT,        // ## documentation comment
    
    // Special
    END_OF_FILE,
    UNKNOWN
};

/**
 * @brief Source location for error reporting
 */
struct SourceLocation {
    std::string filename;
    size_t line;
    size_t column;
    size_t offset;      // byte offset in source
    
    std::string toString() const;
};

/**
 * @brief Token value can be string, number, or boolean
 */
using TokenValue = std::variant<std::monostate, std::string, double, bool>;

/**
 * @brief Token structure holding type, value, and location
 */
struct Token {
    TokenType type;
    TokenValue value;
    SourceLocation location;
    std::string raw;    // Raw text of the token
    
    Token() : type(TokenType::UNKNOWN) {}
    Token(TokenType t, SourceLocation loc, std::string rawText = "")
        : type(t), location(loc), raw(std::move(rawText)) {}
    
    // Helper methods for value access
    bool hasValue() const { return !std::holds_alternative<std::monostate>(value); }
    
    std::string asString() const;
    double asNumber() const;
    bool asBool() const;
    
    // Check token type
    bool is(TokenType t) const { return type == t; }
    bool isKeyword() const;
    bool isOperator() const;
    bool isLiteral() const;
    
    std::string toString() const;
    static std::string typeToString(TokenType type);
};

/**
 * @brief Lexer exception for tokenization errors
 */
class LexerError : public std::runtime_error {
public:
    LexerError(const std::string& message, const SourceLocation& loc);
    
    const SourceLocation& location() const { return _location; }
    
private:
    SourceLocation _location;
};

/**
 * @brief Lexer configuration options
 */
struct LexerConfig {
    bool preserveComments = false;      // Keep comments as tokens
    bool preserveNewlines = false;      // Keep newlines as tokens
    bool allowUnicode = true;           // Allow unicode in identifiers
};

/**
 * @brief Lexer for VisionPipe Script (.vsp) files
 * 
 * Tokenizes VSP source code into a stream of tokens.
 * Handles identifiers, keywords, literals, operators, and comments.
 */
class Lexer {
public:
    explicit Lexer(const std::string& source, const std::string& filename = "<input>");
    Lexer(const std::string& source, const std::string& filename, LexerConfig config);
    
    /**
     * @brief Tokenize entire source and return all tokens
     */
    std::vector<Token> tokenize();
    
    /**
     * @brief Get next token (streaming mode)
     */
    Token nextToken();
    
    /**
     * @brief Peek at next token without consuming it
     */
    Token peekToken();
    
    /**
     * @brief Check if at end of source
     */
    bool isAtEnd() const;
    
    /**
     * @brief Get current source location
     */
    SourceLocation currentLocation() const;
    
    /**
     * @brief Get line content for error display
     */
    std::string getLineContent(size_t lineNumber) const;
    
private:
    std::string _source;
    std::string _filename;
    LexerConfig _config;
    
    size_t _pos;        // Current position in source
    size_t _line;       // Current line (1-based)
    size_t _column;     // Current column (1-based)
    
    std::optional<Token> _peeked;   // Peeked token cache
    
    // Keyword lookup table
    static const std::unordered_map<std::string, TokenType> _keywords;
    
    // Character utilities
    char current() const;
    char peek(size_t ahead = 1) const;
    char advance();
    bool match(char expected);
    bool match(const std::string& expected);
    
    // Skip utilities
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();
    
    // Token scanners
    Token scanToken();
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString(char quote);
    Token scanOperator();
    Token scanComment();
    
    // Token creation helper
    Token makeToken(TokenType type, const std::string& raw = "");
    Token makeToken(TokenType type, TokenValue value, const std::string& raw);
    
    // Error reporting
    LexerError error(const std::string& message);
};

} // namespace visionpipe

#endif // VISIONPIPE_LEXER_H
