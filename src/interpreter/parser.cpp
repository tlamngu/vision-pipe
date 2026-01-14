#include "interpreter/parser.h"
#include <fstream>
#include <sstream>

namespace visionpipe {

// ============================================================================
// ParseError
// ============================================================================

ParseError::ParseError(const std::string& message, const SourceLocation& loc)
    : std::runtime_error(loc.toString() + ": " + message)
    , _location(loc) {}

ParseError::ParseError(const std::string& message, const Token& token)
    : std::runtime_error(token.location.toString() + ": " + message + " (got '" + token.raw + "')")
    , _location(token.location) {}

// ============================================================================
// Parser
// ============================================================================

Parser::Parser(const std::vector<Token>& tokens, const std::string& filename)
    : Parser(tokens, filename, ParserConfig{}) {}

Parser::Parser(const std::vector<Token>& tokens, const std::string& filename, ParserConfig config)
    : _tokens(tokens)
    , _filename(filename)
    , _config(config)
    , _current(0) {}

// Token access helpers
Token& Parser::current() {
    return _tokens[_current];
}

Token& Parser::previous() {
    return _tokens[_current > 0 ? _current - 1 : 0];
}

Token& Parser::peek(size_t ahead) {
    size_t idx = _current + ahead;
    if (idx >= _tokens.size()) {
        return _tokens.back();
    }
    return _tokens[idx];
}

bool Parser::isAtEnd() const {
    return _current >= _tokens.size() || 
           _tokens[_current].type == TokenType::END_OF_FILE;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return _tokens[_current].type == type;
}

bool Parser::checkAny(std::initializer_list<TokenType> types) const {
    for (auto type : types) {
        if (check(type)) return true;
    }
    return false;
}

Token Parser::advance() {
    if (!isAtEnd()) {
        // Capture doc comments
        if (current().type == TokenType::DOC_COMMENT) {
            _lastDocComment = current().asString();
        }
        ++_current;
    }
    return previous();
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::matchAny(std::initializer_list<TokenType> types) {
    for (auto type : types) {
        if (match(type)) return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    throw error(message);
}

ParseError Parser::error(const std::string& message) {
    return ParseError(message, current());
}

ParseError Parser::error(const Token& token, const std::string& message) {
    return ParseError(message, token);
}

void Parser::synchronize() {
    advance();
    
    while (!isAtEnd()) {
        // Sync at statement boundaries
        switch (current().type) {
            case TokenType::KW_PIPELINE:
            case TokenType::KW_END:
            case TokenType::KW_EXEC_SEQ:
            case TokenType::KW_EXEC_MULTI:
            case TokenType::KW_EXEC_LOOP:
            case TokenType::KW_IF:
            case TokenType::KW_WHILE:
            case TokenType::KW_RETURN:
            case TokenType::KW_IMPORT:
            case TokenType::KW_GLOBAL:
            case TokenType::KW_CONFIG:
                return;
            default:
                break;
        }
        advance();
    }
}

// ============================================================================
// Top-level parsing
// ============================================================================

std::shared_ptr<Program> Parser::parse() {
    return program();
}

std::shared_ptr<Expression> Parser::parseExpression() {
    return expression();
}

std::shared_ptr<Program> Parser::program() {
    auto prog = std::make_shared<Program>();
    prog->sourceFile = _filename;
    prog->location = current().location;
    
    while (!isAtEnd()) {
        try {
            // Skip doc comments at top level but capture them
            if (match(TokenType::DOC_COMMENT)) {
                continue;
            }
            
            if (check(TokenType::KW_IMPORT)) {
                prog->imports.push_back(importStatement());
            } else if (check(TokenType::KW_GLOBAL)) {
                prog->globals.push_back(globalStatement());
            } else if (check(TokenType::KW_CONFIG)) {
                prog->configs.push_back(configDeclaration());
            } else if (check(TokenType::KW_PIPELINE)) {
                prog->pipelines.push_back(pipelineDeclaration());
            } else if (check(TokenType::KW_EXEC_SEQ) || 
                       check(TokenType::KW_EXEC_MULTI) || 
                       check(TokenType::KW_EXEC_LOOP)) {
                prog->topLevelStatements.push_back(statement());
            } else if (check(TokenType::END_OF_FILE)) {
                break;
            } else {
                throw error("Expected pipeline declaration or execution statement");
            }
        } catch (const ParseError& e) {
            _errors.push_back(e);
            synchronize();
        }
    }
    
    return prog;
}

std::shared_ptr<ImportStmt> Parser::importStatement() {
    auto stmt = std::make_shared<ImportStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_IMPORT, "Expected 'import'");
    Token pathToken = consume(TokenType::STRING_LITERAL, "Expected import path string");
    stmt->path = pathToken.asString();
    
    // Optional alias: import "path" as name
    if (match(TokenType::IDENTIFIER) && previous().raw == "as") {
        Token aliasToken = consume(TokenType::IDENTIFIER, "Expected alias name");
        stmt->alias = aliasToken.asString();
    }
    
    return stmt;
}

std::shared_ptr<GlobalStmt> Parser::globalStatement() {
    auto stmt = std::make_shared<GlobalStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_GLOBAL, "Expected 'global'");
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected global cache identifier");
    stmt->cacheId = nameToken.asString();
    
    // Optional initial value: global name = value
    if (match(TokenType::OP_ASSIGN)) {
        stmt->initialValue = expression();
    }
    
    return stmt;
}

std::shared_ptr<ConfigDecl> Parser::configDeclaration() {
    auto config = std::make_shared<ConfigDecl>();
    config->location = current().location;
    
    consume(TokenType::KW_CONFIG, "Expected 'config'");
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected config name");
    config->name = nameToken.asString();
    
    // Parse key-value pairs until 'end'
    while (!check(TokenType::KW_END) && !isAtEnd()) {
        Token keyToken = consume(TokenType::IDENTIFIER, "Expected config key");
        consume(TokenType::OP_ASSIGN, "Expected '=' after config key");
        auto value = expression();
        config->settings[keyToken.asString()] = value;
    }
    
    consume(TokenType::KW_END, "Expected 'end' after config block");
    return config;
}

std::shared_ptr<PipelineDecl> Parser::pipelineDeclaration() {
    auto pipeline = std::make_shared<PipelineDecl>();
    pipeline->location = current().location;
    pipeline->docComment = _lastDocComment;
    _lastDocComment.clear();
    
    consume(TokenType::KW_PIPELINE, "Expected 'pipeline'");
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected pipeline name");
    pipeline->name = nameToken.asString();
    
    // Optional parameters
    if (match(TokenType::LPAREN)) {
        if (!check(TokenType::RPAREN)) {
            pipeline->parameters = parameterList();
        }
        consume(TokenType::RPAREN, "Expected ')' after parameters");
    }
    
    // Optional cache output
    pipeline->cacheOutput = cacheOutputSpec();
    
    // Pipeline body
    while (!check(TokenType::KW_END) && !isAtEnd()) {
        try {
            // Skip doc comments but capture them
            if (match(TokenType::DOC_COMMENT)) {
                continue;
            }
            pipeline->body.push_back(statement());
        } catch (const ParseError& e) {
            _errors.push_back(e);
            synchronize();
            if (check(TokenType::KW_END)) break;
        }
    }
    
    consume(TokenType::KW_END, "Expected 'end' after pipeline body");
    return pipeline;
}

std::vector<ParameterDecl> Parser::parameterList() {
    std::vector<ParameterDecl> params;
    
    do {
        params.push_back(parameterDecl());
    } while (match(TokenType::OP_COMMA));
    
    return params;
}

ParameterDecl Parser::parameterDecl() {
    ParameterDecl param;
    param.location = current().location;
    
    // Optional modifiers
    if (match(TokenType::KW_LET)) {
        param.isLetBinding = true;
    }
    if (match(TokenType::KW_GLOBAL)) {
        param.isGlobal = true;
    }
    
    // Parameter name
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected parameter name");
    param.name = nameToken.asString();
    
    // Optional type annotation
    if (match(TokenType::OP_COLON)) {
        Token typeToken = consume(TokenType::IDENTIFIER, "Expected type name");
        std::string typeName = typeToken.asString();
        
        if (typeName == "int") param.typeHint = ValueType::INTEGER;
        else if (typeName == "float") param.typeHint = ValueType::FLOAT;
        else if (typeName == "string") param.typeHint = ValueType::STRING;
        else if (typeName == "bool") param.typeHint = ValueType::BOOLEAN;
        else if (typeName == "mat") param.typeHint = ValueType::MAT;
        else param.typeHint = ValueType::UNKNOWN;
    }
    
    // Optional default value
    if (match(TokenType::OP_ASSIGN)) {
        param.defaultValue = expression();
    }
    
    return param;
}

std::optional<CacheOutput> Parser::cacheOutputSpec() {
    if (!match(TokenType::OP_ARROW)) {
        return std::nullopt;
    }
    
    CacheOutput output;
    output.location = current().location;
    
    // Check for 'global' modifier
    if (match(TokenType::KW_GLOBAL)) {
        output.isGlobal = true;
    }
    
    // Accept either string literal or identifier for cache id
    if (check(TokenType::STRING_LITERAL)) {
        Token cacheIdToken = advance();
        output.cacheId = cacheIdToken.asString();
    } else if (check(TokenType::IDENTIFIER)) {
        Token cacheIdToken = advance();
        output.cacheId = cacheIdToken.asString();
        output.isDynamic = true;  // Mark as dynamic (variable reference)
    } else {
        throw error("Expected cache id after '->' (got '" + current().raw + "')");
    }
    
    return output;
}

// ============================================================================
// Statements
// ============================================================================

std::shared_ptr<Statement> Parser::statement() {
    if (check(TokenType::KW_EXEC_SEQ)) return execSeqStatement();
    if (check(TokenType::KW_EXEC_MULTI)) return execMultiStatement();
    if (check(TokenType::KW_EXEC_LOOP)) return execLoopStatement();
    if (check(TokenType::KW_USE)) return useStatement();
    if (check(TokenType::KW_CACHE)) return cacheStatement();
    if (check(TokenType::KW_GLOBAL)) return globalPromoteStatement();
    if (check(TokenType::KW_IF)) return ifStatement();
    if (check(TokenType::KW_WHILE)) return whileStatement();
    if (check(TokenType::KW_RETURN)) return returnStatement();
    if (check(TokenType::KW_BREAK)) return breakStatement();
    if (check(TokenType::KW_CONTINUE)) return continueStatement();
    
    return expressionStatement();
}

std::shared_ptr<Statement> Parser::globalPromoteStatement() {
    // global "cache_id" - promotes local cache to global
    auto stmt = std::make_shared<GlobalStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_GLOBAL, "Expected 'global'");
    
    Token cacheIdToken = consume(TokenType::STRING_LITERAL, "Expected cache id string");
    stmt->cacheId = cacheIdToken.asString();
    
    return stmt;
}

std::shared_ptr<Statement> Parser::execSeqStatement() {
    auto stmt = std::make_shared<ExecSeqStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_EXEC_SEQ, "Expected 'exec_seq'");
    stmt->pipelineRef = expression();
    
    return stmt;
}

std::shared_ptr<Statement> Parser::execMultiStatement() {
    auto stmt = std::make_shared<ExecMultiStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_EXEC_MULTI, "Expected 'exec_multi'");
    consume(TokenType::LBRACKET, "Expected '[' after 'exec_multi'");
    
    if (!check(TokenType::RBRACKET)) {
        do {
            stmt->pipelineRefs.push_back(expression());
        } while (match(TokenType::OP_COMMA));
    }
    
    consume(TokenType::RBRACKET, "Expected ']' after pipeline list");
    
    return stmt;
}

std::shared_ptr<Statement> Parser::execLoopStatement() {
    auto stmt = std::make_shared<ExecLoopStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_EXEC_LOOP, "Expected 'exec_loop'");
    stmt->pipelineRef = expression();
    
    // Optional condition: exec_loop main while condition
    if (match(TokenType::KW_WHILE)) {
        stmt->condition = expression();
    }
    
    return stmt;
}

std::shared_ptr<Statement> Parser::useStatement() {
    auto stmt = std::make_shared<UseStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_USE, "Expected 'use'");
    consume(TokenType::LPAREN, "Expected '(' after 'use'");
    
    // Check for 'global' modifier
    if (match(TokenType::KW_GLOBAL)) {
        stmt->isGlobal = true;
    }
    
    Token cacheIdToken = consume(TokenType::STRING_LITERAL, "Expected cache id string");
    stmt->cacheId = cacheIdToken.asString();
    
    consume(TokenType::RPAREN, "Expected ')' after cache id");
    
    // Check for optional arrow to cache immediately: use("a") -> "b"
    if (match(TokenType::OP_ARROW)) {
        CacheOutput output;
        output.location = current().location;
        
        if (match(TokenType::KW_GLOBAL)) {
            output.isGlobal = true;
        }
        
        if (check(TokenType::STRING_LITERAL)) {
            Token outputToken = advance();
            output.cacheId = outputToken.asString();
        } else if (check(TokenType::IDENTIFIER)) {
            Token outputToken = advance();
            output.cacheId = outputToken.asString();
            output.isDynamic = true;
        } else {
            throw error("Expected cache id after '->'");
        }
        
        stmt->cacheOutput = output;
    }
    
    return stmt;
}

std::shared_ptr<Statement> Parser::cacheStatement() {
    auto stmt = std::make_shared<CacheStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_CACHE, "Expected 'cache'");
    consume(TokenType::LPAREN, "Expected '(' after 'cache'");
    
    // Check for 'global' modifier
    if (match(TokenType::KW_GLOBAL)) {
        stmt->isGlobal = true;
    }
    
    Token cacheIdToken = consume(TokenType::STRING_LITERAL, "Expected cache id string");
    stmt->cacheId = cacheIdToken.asString();
    
    // Optional second parameter: cache("id") or cache("id", expression)
    if (match(TokenType::OP_COMMA)) {
        stmt->value = expression();
    }
    // If no comma, value remains nullptr and currentMat will be cached
    
    consume(TokenType::RPAREN, "Expected ')' after cache statement");
    
    return stmt;
}

std::shared_ptr<Statement> Parser::ifStatement() {
    auto stmt = std::make_shared<IfStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_IF, "Expected 'if'");
    stmt->condition = expression();
    
    // Then branch
    while (!check(TokenType::KW_ELSE) && !check(TokenType::KW_END) && !isAtEnd()) {
        stmt->thenBranch.push_back(statement());
    }
    
    // Optional else branch
    if (match(TokenType::KW_ELSE)) {
        while (!check(TokenType::KW_END) && !isAtEnd()) {
            stmt->elseBranch.push_back(statement());
        }
    }
    
    consume(TokenType::KW_END, "Expected 'end' after if statement");
    
    return stmt;
}

std::shared_ptr<Statement> Parser::whileStatement() {
    auto stmt = std::make_shared<WhileStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_WHILE, "Expected 'while'");
    stmt->condition = expression();
    
    while (!check(TokenType::KW_END) && !isAtEnd()) {
        stmt->body.push_back(statement());
    }
    
    consume(TokenType::KW_END, "Expected 'end' after while statement");
    
    return stmt;
}

std::shared_ptr<Statement> Parser::returnStatement() {
    auto stmt = std::make_shared<ReturnStmt>();
    stmt->location = current().location;
    
    consume(TokenType::KW_RETURN, "Expected 'return'");
    
    // Optional return value
    if (!check(TokenType::KW_END) && !isStatementStart()) {
        stmt->value = expression();
    }
    
    return stmt;
}

std::shared_ptr<Statement> Parser::breakStatement() {
    auto stmt = std::make_shared<BreakStmt>();
    stmt->location = current().location;
    consume(TokenType::KW_BREAK, "Expected 'break'");
    return stmt;
}

std::shared_ptr<Statement> Parser::continueStatement() {
    auto stmt = std::make_shared<ContinueStmt>();
    stmt->location = current().location;
    consume(TokenType::KW_CONTINUE, "Expected 'continue'");
    return stmt;
}

std::shared_ptr<Statement> Parser::expressionStatement() {
    auto stmt = std::make_shared<ExpressionStmt>();
    stmt->location = current().location;
    stmt->expression = expression();
    return stmt;
}

// ============================================================================
// Expressions (precedence climbing)
// ============================================================================

std::shared_ptr<Expression> Parser::expression() {
    return assignment();
}

std::shared_ptr<Expression> Parser::assignment() {
    auto expr = logicalOr();
    
    if (match(TokenType::OP_ASSIGN)) {
        // For now, assignment creates a binary expression
        // Could be enhanced to proper assignment node
        auto assignExpr = std::make_shared<BinaryExpr>();
        assignExpr->location = previous().location;
        assignExpr->left = expr;
        assignExpr->op = TokenType::OP_ASSIGN;
        assignExpr->right = assignment();
        return assignExpr;
    }
    
    return expr;
}

std::shared_ptr<Expression> Parser::logicalOr() {
    auto left = logicalAnd();
    
    while (match(TokenType::OP_OR)) {
        auto expr = std::make_shared<BinaryExpr>();
        expr->location = previous().location;
        expr->left = left;
        expr->op = TokenType::OP_OR;
        expr->right = logicalAnd();
        left = expr;
    }
    
    return left;
}

std::shared_ptr<Expression> Parser::logicalAnd() {
    auto left = equality();
    
    while (match(TokenType::OP_AND)) {
        auto expr = std::make_shared<BinaryExpr>();
        expr->location = previous().location;
        expr->left = left;
        expr->op = TokenType::OP_AND;
        expr->right = equality();
        left = expr;
    }
    
    return left;
}

std::shared_ptr<Expression> Parser::equality() {
    auto left = comparison();
    
    while (matchAny({TokenType::OP_EQ, TokenType::OP_NE})) {
        auto expr = std::make_shared<BinaryExpr>();
        expr->location = previous().location;
        expr->left = left;
        expr->op = previous().type;
        expr->right = comparison();
        left = expr;
    }
    
    return left;
}

std::shared_ptr<Expression> Parser::comparison() {
    auto left = term();
    
    while (matchAny({TokenType::OP_LT, TokenType::OP_LE, TokenType::OP_GT, TokenType::OP_GE})) {
        auto expr = std::make_shared<BinaryExpr>();
        expr->location = previous().location;
        expr->left = left;
        expr->op = previous().type;
        expr->right = term();
        left = expr;
    }
    
    return left;
}

std::shared_ptr<Expression> Parser::term() {
    auto left = factor();
    
    while (matchAny({TokenType::OP_PLUS, TokenType::OP_MINUS})) {
        auto expr = std::make_shared<BinaryExpr>();
        expr->location = previous().location;
        expr->left = left;
        expr->op = previous().type;
        expr->right = factor();
        left = expr;
    }
    
    return left;
}

std::shared_ptr<Expression> Parser::factor() {
    auto left = unary();
    
    while (matchAny({TokenType::OP_MULTIPLY, TokenType::OP_DIVIDE, TokenType::OP_MODULO})) {
        auto expr = std::make_shared<BinaryExpr>();
        expr->location = previous().location;
        expr->left = left;
        expr->op = previous().type;
        expr->right = unary();
        left = expr;
    }
    
    return left;
}

std::shared_ptr<Expression> Parser::unary() {
    if (matchAny({TokenType::OP_NOT, TokenType::OP_MINUS})) {
        auto expr = std::make_shared<UnaryExpr>();
        expr->location = previous().location;
        expr->op = previous().type;
        expr->operand = unary();
        return expr;
    }
    
    return postfix();
}

std::shared_ptr<Expression> Parser::postfix() {
    auto expr = primary();
    
    // Check for arrow operator
    if (match(TokenType::OP_ARROW)) {
        // Case 1: function_call() -> "cache_id" (cache output - save result)
        if (auto* funcCall = dynamic_cast<FunctionCallExpr*>(expr.get())) {
            CacheOutput output;
            output.location = current().location;
            
            if (match(TokenType::KW_GLOBAL)) {
                output.isGlobal = true;
            }
            
            // Accept either string literal or identifier for cache id
            if (check(TokenType::STRING_LITERAL)) {
                Token cacheIdToken = advance();
                output.cacheId = cacheIdToken.asString();
            } else if (check(TokenType::IDENTIFIER)) {
                Token cacheIdToken = advance();
                output.cacheId = cacheIdToken.asString();
                output.isDynamic = true;
            } else {
                throw error("Expected cache id after '->' (got '" + current().raw + "')");
            }
            funcCall->cacheOutput = output;
        }
        // Case 2: "cache_id" -> item() (cache load - load and pipe from string literal)
        else if (auto* literal = dynamic_cast<LiteralExpr*>(expr.get())) {
            if (!std::holds_alternative<std::string>(literal->value)) {
                throw error("Cache load operator '->' requires string cache id");
            }
            
            auto loadExpr = std::make_shared<CacheLoadExpr>();
            loadExpr->location = expr->location;
            loadExpr->cacheId = std::get<std::string>(literal->value);
            loadExpr->isDynamic = false;
            
            // Check for global modifier
            if (match(TokenType::KW_GLOBAL)) {
                loadExpr->isGlobal = true;
            }
            
            // Parse the target function call
            auto targetExpr = primary();
            auto* targetCall = dynamic_cast<FunctionCallExpr*>(targetExpr.get());
            if (!targetCall) {
                throw error("Expected function call after cache load '->'");
            }
            loadExpr->targetCall = std::dynamic_pointer_cast<FunctionCallExpr>(targetExpr);
            
            return loadExpr;
        }
        // Case 3: identifier -> item() (variable as cache id)
        else if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.get())) {
            auto loadExpr = std::make_shared<CacheLoadExpr>();
            loadExpr->location = expr->location;
            loadExpr->cacheId = ident->name;
            loadExpr->isDynamic = true;
            
            // Check for global modifier
            if (match(TokenType::KW_GLOBAL)) {
                loadExpr->isGlobal = true;
            }
            
            // Parse the target function call
            auto targetExpr = primary();
            auto* targetCall = dynamic_cast<FunctionCallExpr*>(targetExpr.get());
            if (!targetCall) {
                throw error("Expected function call after cache load '->'");
            }
            loadExpr->targetCall = std::dynamic_pointer_cast<FunctionCallExpr>(targetExpr);
            
            return loadExpr;
        }
        else {
            throw error("Cache operator '->' can only be used with function calls, strings, or identifiers");
        }
    }
    
    return expr;
}

std::shared_ptr<Expression> Parser::primary() {
    // Literals
    if (match(TokenType::NUMBER_LITERAL)) {
        return makeLiteral(previous());
    }
    
    if (match(TokenType::STRING_LITERAL)) {
        return makeLiteral(previous());
    }
    
    if (match(TokenType::BOOLEAN_LITERAL)) {
        return makeLiteral(previous());
    }
    
    // Array literal
    if (check(TokenType::LBRACKET)) {
        return arrayLiteral();
    }
    
    // Parenthesized expression
    if (match(TokenType::LPAREN)) {
        auto expr = expression();
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    // Identifier or function call
    if (match(TokenType::IDENTIFIER)) {
        Token nameToken = previous();
        
        // Check if it's a function call
        if (check(TokenType::LPAREN)) {
            return functionCall(nameToken.asString(), nameToken.location);
        }
        
        // Plain identifier
        auto ident = std::make_shared<IdentifierExpr>();
        ident->location = nameToken.location;
        ident->name = nameToken.asString();
        return ident;
    }
    
    throw error("Expected expression");
}

std::shared_ptr<Expression> Parser::functionCall(const std::string& name, SourceLocation loc) {
    auto call = std::make_shared<FunctionCallExpr>();
    call->location = loc;
    call->functionName = name;
    
    consume(TokenType::LPAREN, "Expected '(' after function name");
    
    if (!check(TokenType::RPAREN)) {
        call->arguments = argumentList();
    }
    
    consume(TokenType::RPAREN, "Expected ')' after arguments");
    
    return call;
}

std::shared_ptr<Expression> Parser::arrayLiteral() {
    auto arr = std::make_shared<ArrayExpr>();
    arr->location = current().location;
    
    consume(TokenType::LBRACKET, "Expected '['");
    
    if (!check(TokenType::RBRACKET)) {
        do {
            arr->elements.push_back(expression());
        } while (match(TokenType::OP_COMMA));
    }
    
    consume(TokenType::RBRACKET, "Expected ']'");
    
    return arr;
}

std::vector<std::shared_ptr<Expression>> Parser::argumentList() {
    std::vector<std::shared_ptr<Expression>> args;
    
    do {
        args.push_back(expression());
    } while (match(TokenType::OP_COMMA));
    
    return args;
}

std::shared_ptr<LiteralExpr> Parser::makeLiteral(const Token& token) {
    auto literal = std::make_shared<LiteralExpr>();
    literal->location = token.location;
    
    switch (token.type) {
        case TokenType::NUMBER_LITERAL:
            literal->value = token.asNumber();
            literal->valueType = ValueType::FLOAT;
            break;
        case TokenType::STRING_LITERAL:
            literal->value = token.asString();
            literal->valueType = ValueType::STRING;
            break;
        case TokenType::BOOLEAN_LITERAL:
            literal->value = token.asBool();
            literal->valueType = ValueType::BOOLEAN;
            break;
        default:
            literal->valueType = ValueType::UNKNOWN;
    }
    
    return literal;
}

bool Parser::isStatementStart() const {
    return checkAny({
        TokenType::KW_EXEC_SEQ,
        TokenType::KW_EXEC_MULTI,
        TokenType::KW_EXEC_LOOP,
        TokenType::KW_USE,
        TokenType::KW_CACHE,
        TokenType::KW_IF,
        TokenType::KW_WHILE,
        TokenType::KW_RETURN,
        TokenType::KW_BREAK,
        TokenType::KW_CONTINUE
    });
}

bool Parser::isPipelineBodyEnd() const {
    return check(TokenType::KW_END) || isAtEnd();
}

// ============================================================================
// Utility functions
// ============================================================================

std::shared_ptr<Program> parseFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return parseSource(buffer.str(), filename);
}

std::shared_ptr<Program> parseSource(const std::string& source, const std::string& filename) {
    Lexer lexer(source, filename);
    auto tokens = lexer.tokenize();
    
    Parser parser(tokens, filename);
    return parser.parse();
}

} // namespace visionpipe
