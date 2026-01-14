#ifndef VISIONPIPE_PARSER_H
#define VISIONPIPE_PARSER_H

#include "interpreter/lexer.h"
#include "interpreter/ast.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>

namespace visionpipe {

/**
 * @brief Parser exception for syntax errors
 */
class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, const SourceLocation& loc);
    ParseError(const std::string& message, const Token& token);
    
    const SourceLocation& location() const { return _location; }
    
private:
    SourceLocation _location;
};

/**
 * @brief Parser configuration
 */
struct ParserConfig {
    bool strictMode = false;            // Require explicit types
    bool allowImplicitGlobals = false;  // Allow undefined identifiers as globals
    std::string baseDir = ".";          // Base directory for imports
};

/**
 * @brief Parser for VisionPipe Script (.vsp) files
 * 
 * Parses token stream into AST.
 * 
 * Grammar overview:
 *   program       -> (import | global | config | pipeline | statement)* EOF
 *   pipeline      -> "pipeline" IDENTIFIER params? cacheOutput? statement* "end"
 *   params        -> "(" paramDecl ("," paramDecl)* ")"
 *   paramDecl     -> ("let" | "global")? IDENTIFIER (":" type)?
 *   cacheOutput   -> "->" STRING
 *   statement     -> execSeq | execMulti | execLoop | use | if | while | 
 *                    return | break | continue | exprStmt
 *   execSeq       -> "exec_seq" expression
 *   execMulti     -> "exec_multi" "[" expression ("," expression)* "]"
 *   execLoop      -> "exec_loop" expression
 *   use           -> "use" "(" STRING ")"
 *   expression    -> assignment
 *   assignment    -> IDENTIFIER "=" assignment | logicalOr
 *   logicalOr     -> logicalAnd ("||" | "or" logicalAnd)*
 *   logicalAnd    -> equality ("&&" | "and" equality)*
 *   equality      -> comparison (("==" | "!=") comparison)*
 *   comparison    -> term (("<" | "<=" | ">" | ">=") term)*
 *   term          -> factor (("+" | "-") factor)*
 *   factor        -> unary (("*" | "/" | "%") unary)*
 *   unary         -> ("!" | "-" | "not") unary | postfix
 *   postfix       -> primary cacheOutput?
 *   primary       -> functionCall | IDENTIFIER | literal | "(" expression ")" | array
 *   functionCall  -> IDENTIFIER "(" arguments? ")"
 *   arguments     -> expression ("," expression)*
 *   array         -> "[" (expression ("," expression)*)? "]"
 *   literal       -> NUMBER | STRING | BOOLEAN
 */
class Parser {
public:
    Parser(const std::vector<Token>& tokens, const std::string& filename = "<input>");
    Parser(const std::vector<Token>& tokens, const std::string& filename, ParserConfig config);
    
    /**
     * @brief Parse the entire program
     */
    std::shared_ptr<Program> parse();
    
    /**
     * @brief Parse a single expression (for REPL or testing)
     */
    std::shared_ptr<Expression> parseExpression();
    
    /**
     * @brief Get parsing errors (non-fatal)
     */
    const std::vector<ParseError>& errors() const { return _errors; }
    
    /**
     * @brief Check if parsing had errors
     */
    bool hasErrors() const { return !_errors.empty(); }
    
private:
    std::vector<Token> _tokens;
    std::string _filename;
    ParserConfig _config;
    size_t _current;
    std::vector<ParseError> _errors;
    std::string _lastDocComment;  // Track documentation comments
    
    // Token access
    Token& current();
    Token& previous();
    Token& peek(size_t ahead = 1);
    bool isAtEnd() const;
    bool check(TokenType type) const;
    bool checkAny(std::initializer_list<TokenType> types) const;
    Token advance();
    bool match(TokenType type);
    bool matchAny(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);
    
    // Error handling
    ParseError error(const std::string& message);
    ParseError error(const Token& token, const std::string& message);
    void synchronize();
    
    // Parsing rules
    std::shared_ptr<Program> program();
    std::shared_ptr<ImportStmt> importStatement();
    std::shared_ptr<GlobalStmt> globalStatement();
    std::shared_ptr<ConfigDecl> configDeclaration();
    std::shared_ptr<PipelineDecl> pipelineDeclaration();
    std::vector<ParameterDecl> parameterList();
    ParameterDecl parameterDecl();
    std::optional<CacheOutput> cacheOutputSpec();
    
    // Statements
    std::shared_ptr<Statement> statement();
    std::shared_ptr<Statement> execSeqStatement();
    std::shared_ptr<Statement> execMultiStatement();
    std::shared_ptr<Statement> execLoopStatement();
    std::shared_ptr<Statement> useStatement();
    std::shared_ptr<Statement> cacheStatement();
    std::shared_ptr<Statement> globalPromoteStatement();
    std::shared_ptr<Statement> ifStatement();
    std::shared_ptr<Statement> whileStatement();
    std::shared_ptr<Statement> returnStatement();
    std::shared_ptr<Statement> breakStatement();
    std::shared_ptr<Statement> continueStatement();
    std::shared_ptr<Statement> expressionStatement();
    
    // Expressions (precedence climbing)
    std::shared_ptr<Expression> expression();
    std::shared_ptr<Expression> assignment();
    std::shared_ptr<Expression> logicalOr();
    std::shared_ptr<Expression> logicalAnd();
    std::shared_ptr<Expression> equality();
    std::shared_ptr<Expression> comparison();
    std::shared_ptr<Expression> term();
    std::shared_ptr<Expression> factor();
    std::shared_ptr<Expression> unary();
    std::shared_ptr<Expression> postfix();
    std::shared_ptr<Expression> primary();
    std::shared_ptr<Expression> functionCall(const std::string& name, SourceLocation loc);
    std::shared_ptr<Expression> arrayLiteral();
    std::vector<std::shared_ptr<Expression>> argumentList();
    
    // Helpers
    std::shared_ptr<LiteralExpr> makeLiteral(const Token& token);
    bool isStatementStart() const;
    bool isPipelineBodyEnd() const;
};

/**
 * @brief Parse a VSP file
 */
std::shared_ptr<Program> parseFile(const std::string& filename);

/**
 * @brief Parse VSP source code
 */
std::shared_ptr<Program> parseSource(const std::string& source, const std::string& filename = "<input>");

} // namespace visionpipe

#endif // VISIONPIPE_PARSER_H
