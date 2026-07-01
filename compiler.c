#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bytecode.h"

/* ============================================================================
 * ERROR HANDLER
 * ============================================================================ */

typedef struct {
    char *messages[100];
    int count;
} ErrorHandler;

ErrorHandler *eh_create(void) {
    ErrorHandler *eh = malloc(sizeof(ErrorHandler));
    eh->count = 0;
    return eh;
}

void eh_add_error(ErrorHandler *eh, const char *msg) {
    if (eh->count < 100) {
        eh->messages[eh->count++] = strdup(msg);
    }
}

void eh_print_errors(ErrorHandler *eh) {
    for (int i = 0; i < eh->count; i++) {
        fprintf(stderr, "Error: %s\n", eh->messages[i]);
    }
}

void eh_free(ErrorHandler *eh) {
    for (int i = 0; i < eh->count; i++) {
        free(eh->messages[i]);
    }
    free(eh);
}

/* ============================================================================
 * LEXER
 * ============================================================================ */

typedef enum {
    TOKEN_INT, TOKEN_FLOAT, TOKEN_STRING, TOKEN_BOOL, TOKEN_CHAR,
    TOKEN_USE, TOKEN_FN, TOKEN_RET, TOKEN_IF, TOKEN_ELSE,
    TOKEN_LOOP, TOKEN_STRUCT, TOKEN_ENUM, TOKEN_MATCH,
    TOKEN_IDENT, TOKEN_TYPE,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_EQ, TOKEN_NEQ, TOKEN_LT, TOKEN_GT, TOKEN_LTE, TOKEN_GTE,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT, TOKEN_AMPERSAND, TOKEN_ASSIGN,
    TOKEN_DOT, TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_COMMA, TOKEN_COLON, TOKEN_SEMICOLON, TOKEN_ARROW,
    TOKEN_EOF, TOKEN_NEWLINE
} TokenType;

typedef struct {
    TokenType type;
    char *value;
    int line;
    int column;
} Token;

typedef struct {
    const char *source;
    size_t pos;
    int line;
    int column;
    Token *tokens;
    size_t token_count;
    size_t token_capacity;
    ErrorHandler *errors;
} Lexer;

Lexer *lexer_create(const char *source, ErrorHandler *errors) {
    Lexer *lex = malloc(sizeof(Lexer));
    lex->source = source;
    lex->pos = 0;
    lex->line = 1;
    lex->column = 1;
    lex->tokens = malloc(sizeof(Token) * 4096);
    lex->token_count = 0;
    lex->token_capacity = 4096;
    lex->errors = errors;
    return lex;
}

void lexer_free(Lexer *lex) {
    for (size_t i = 0; i < lex->token_count; i++) {
        free(lex->tokens[i].value);
    }
    free(lex->tokens);
    free(lex);
}

void lexer_add_token(Lexer *lex, Token tok) {
    if (lex->token_count >= lex->token_capacity) {
        lex->token_capacity *= 2;
        lex->tokens = realloc(lex->tokens, sizeof(Token) * lex->token_capacity);
    }
    lex->tokens[lex->token_count++] = tok;
}

Token lexer_read_number(Lexer *lex) {
    Token tok;
    tok.line = lex->line;
    tok.column = lex->column;
    size_t start = lex->pos;
    
    while (lex->pos < strlen(lex->source) && isdigit(lex->source[lex->pos])) {
        lex->pos++;
        lex->column++;
    }
    
    if (lex->pos < strlen(lex->source) && lex->source[lex->pos] == '.') {
        tok.type = TOKEN_FLOAT;
        lex->pos++;
        lex->column++;
        while (lex->pos < strlen(lex->source) && isdigit(lex->source[lex->pos])) {
            lex->pos++;
            lex->column++;
        }
    } else {
        tok.type = TOKEN_INT;
    }
    
    tok.value = malloc(lex->pos - start + 1);
    strncpy(tok.value, lex->source + start, lex->pos - start);
    tok.value[lex->pos - start] = '\0';
    
    return tok;
}

Token lexer_read_string(Lexer *lex) {
    Token tok;
    tok.type = TOKEN_STRING;
    tok.line = lex->line;
    tok.column = lex->column;
    
    lex->pos++;  // skip opening quote
    lex->column++;
    size_t start = lex->pos;
    
    while (lex->pos < strlen(lex->source) && lex->source[lex->pos] != '"') {
        if (lex->source[lex->pos] == '\n') {
            lex->line++;
            lex->column = 1;
        } else {
            lex->column++;
        }
        lex->pos++;
    }
    
    tok.value = malloc(lex->pos - start + 1);
    strncpy(tok.value, lex->source + start, lex->pos - start);
    tok.value[lex->pos - start] = '\0';
    
    if (lex->pos < strlen(lex->source)) {
        lex->pos++;  // skip closing quote
        lex->column++;
    }
    
    return tok;
}

Token lexer_read_ident(Lexer *lex) {
    Token tok;
    tok.line = lex->line;
    tok.column = lex->column;
    tok.type = TOKEN_IDENT;
    
    size_t start = lex->pos;
    while (lex->pos < strlen(lex->source) && (isalnum(lex->source[lex->pos]) || lex->source[lex->pos] == '_')) {
        lex->pos++;
        lex->column++;
    }
    
    tok.value = malloc(lex->pos - start + 1);
    strncpy(tok.value, lex->source + start, lex->pos - start);
    tok.value[lex->pos - start] = '\0';
    
    // Check for keywords
    if (strcmp(tok.value, "fn") == 0) tok.type = TOKEN_FN;
    else if (strcmp(tok.value, "use") == 0) tok.type = TOKEN_USE;
    else if (strcmp(tok.value, "ret") == 0) tok.type = TOKEN_RET;
    else if (strcmp(tok.value, "if") == 0) tok.type = TOKEN_IF;
    else if (strcmp(tok.value, "else") == 0) tok.type = TOKEN_ELSE;
    else if (strcmp(tok.value, "loop") == 0) tok.type = TOKEN_LOOP;
    else if (strcmp(tok.value, "struct") == 0) tok.type = TOKEN_STRUCT;
    else if (strcmp(tok.value, "enum") == 0) tok.type = TOKEN_ENUM;
    else if (strcmp(tok.value, "match") == 0) tok.type = TOKEN_MATCH;
    else if (strcmp(tok.value, "true") == 0 || strcmp(tok.value, "false") == 0) tok.type = TOKEN_BOOL;
    
    return tok;
}

void lexer_tokenize(Lexer *lex) {
    while (lex->pos < strlen(lex->source)) {
        char ch = lex->source[lex->pos];
        Token tok;
        tok.line = lex->line;
        tok.column = lex->column;
        
        if (isspace(ch)) {
            if (ch == '\n') {
                lex->line++;
                lex->column = 1;
            } else {
                lex->column++;
            }
            lex->pos++;
        } else if (ch == '"') {
            tok = lexer_read_string(lex);
            lexer_add_token(lex, tok);
        } else if (isdigit(ch)) {
            tok = lexer_read_number(lex);
            lexer_add_token(lex, tok);
        } else if (isalpha(ch) || ch == '_') {
            tok = lexer_read_ident(lex);
            lexer_add_token(lex, tok);
        } else if (ch == '+') {
            tok.type = TOKEN_PLUS;
            tok.value = strdup("+");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == '-') {
            if (lex->pos + 1 < strlen(lex->source) && lex->source[lex->pos + 1] == '>') {
                tok.type = TOKEN_ARROW;
                tok.value = strdup("->");
                lexer_add_token(lex, tok);
                lex->pos += 2;
                lex->column += 2;
            } else {
                tok.type = TOKEN_MINUS;
                tok.value = strdup("-");
                lexer_add_token(lex, tok);
                lex->pos++;
                lex->column++;
            }
        } else if (ch == '*') {
            tok.type = TOKEN_STAR;
            tok.value = strdup("*");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == '/') {
            tok.type = TOKEN_SLASH;
            tok.value = strdup("/");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == '%') {
            tok.type = TOKEN_PERCENT;
            tok.value = strdup("%");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == '=') {
            if (lex->pos + 1 < strlen(lex->source) && lex->source[lex->pos + 1] == '=') {
                tok.type = TOKEN_EQ;
                tok.value = strdup("==");
                lexer_add_token(lex, tok);
                lex->pos += 2;
                lex->column += 2;
            } else {
                tok.type = TOKEN_ASSIGN;
                tok.value = strdup("=");
                lexer_add_token(lex, tok);
                lex->pos++;
                lex->column++;
            }
        } else if (ch == '!') {
            if (lex->pos + 1 < strlen(lex->source) && lex->source[lex->pos + 1] == '=') {
                tok.type = TOKEN_NEQ;
                tok.value = strdup("!=");
                lexer_add_token(lex, tok);
                lex->pos += 2;
                lex->column += 2;
            } else {
                tok.type = TOKEN_NOT;
                tok.value = strdup("!");
                lexer_add_token(lex, tok);
                lex->pos++;
                lex->column++;
            }
        } else if (ch == '<') {
            if (lex->pos + 1 < strlen(lex->source) && lex->source[lex->pos + 1] == '=') {
                tok.type = TOKEN_LTE;
                tok.value = strdup("<=");
                lexer_add_token(lex, tok);
                lex->pos += 2;
                lex->column += 2;
            } else {
                tok.type = TOKEN_LT;
                tok.value = strdup("<");
                lexer_add_token(lex, tok);
                lex->pos++;
                lex->column++;
            }
        } else if (ch == '>') {
            if (lex->pos + 1 < strlen(lex->source) && lex->source[lex->pos + 1] == '=') {
                tok.type = TOKEN_GTE;
                tok.value = strdup(">=");
                lexer_add_token(lex, tok);
                lex->pos += 2;
                lex->column += 2;
            } else {
                tok.type = TOKEN_GT;
                tok.value = strdup(">");
                lexer_add_token(lex, tok);
                lex->pos++;
                lex->column++;
            }
        } else if (ch == '&') {
            if (lex->pos + 1 < strlen(lex->source) && lex->source[lex->pos + 1] == '&') {
                tok.type = TOKEN_AND;
                tok.value = strdup("&&");
                lexer_add_token(lex, tok);
                lex->pos += 2;
                lex->column += 2;
            } else {
                tok.type = TOKEN_AMPERSAND;
                tok.value = strdup("&");
                lexer_add_token(lex, tok);
                lex->pos++;
                lex->column++;
            }
        } else if (ch == '|') {
            if (lex->pos + 1 < strlen(lex->source) && lex->source[lex->pos + 1] == '|') {
                tok.type = TOKEN_OR;
                tok.value = strdup("||");
                lexer_add_token(lex, tok);
                lex->pos += 2;
                lex->column += 2;
            } else {
                lex->pos++;
                lex->column++;
            }
        } else if (ch == '(') {
            tok.type = TOKEN_LPAREN;
            tok.value = strdup("(");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == ')') {
            tok.type = TOKEN_RPAREN;
            tok.value = strdup(")");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == '{') {
            tok.type = TOKEN_LBRACE;
            tok.value = strdup("{");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == '}') {
            tok.type = TOKEN_RBRACE;
            tok.value = strdup("}");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == '[') {
            tok.type = TOKEN_LBRACKET;
            tok.value = strdup("[");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == ']') {
            tok.type = TOKEN_RBRACKET;
            tok.value = strdup("]");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == ',') {
            tok.type = TOKEN_COMMA;
            tok.value = strdup(",");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == ':') {
            tok.type = TOKEN_COLON;
            tok.value = strdup(":");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == ';') {
            tok.type = TOKEN_SEMICOLON;
            tok.value = strdup(";");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else if (ch == '.') {
            tok.type = TOKEN_DOT;
            tok.value = strdup(".");
            lexer_add_token(lex, tok);
            lex->pos++;
            lex->column++;
        } else {
            lex->pos++;
            lex->column++;
        }
    }
    
    tok.type = TOKEN_EOF;
    tok.value = strdup("");
    tok.line = lex->line;
    tok.column = lex->column;
    lexer_add_token(lex, tok);
}

/* ============================================================================
 * PARSER
 * ============================================================================ */

typedef struct {
    Token *tokens;
    size_t token_count;
    size_t pos;
    ErrorHandler *errors;
} Parser;

typedef enum {
    NODE_PROGRAM, NODE_IMPORT,
    NODE_FUNCTION, NODE_STRUCT, NODE_ENUM,
    NODE_BLOCK, NODE_VAR_DECL, NODE_VAR_ASSIGN, NODE_RETURN,
    NODE_IF, NODE_LOOP, NODE_MATCH,
    NODE_EXPR_STMT,
    NODE_BINARY_OP, NODE_UNARY_OP, NODE_CALL, NODE_INDEX, NODE_MEMBER,
    NODE_INT_LIT, NODE_FLOAT_LIT, NODE_STRING_LIT, NODE_CHAR_LIT, NODE_BOOL_LIT,
    NODE_IDENT, NODE_ADDRESSOF, NODE_DEREF
} NodeType;

typedef struct ASTNode {
    NodeType type;
    char *name;
    char *value;
    char *type_name;
    int line, column;
    struct ASTNode **children;
    size_t child_count;
    size_t child_capacity;
} ASTNode;

ASTNode *ast_create(NodeType type) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = type;
    node->name = NULL;
    node->value = NULL;
    node->type_name = NULL;
    node->line = 0;
    node->column = 0;
    node->children = malloc(sizeof(ASTNode*) * 16);
    node->child_count = 0;
    node->child_capacity = 16;
    return node;
}

void ast_add_child(ASTNode *parent, ASTNode *child) {
    if (!child) return;
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = realloc(parent->children, sizeof(ASTNode*) * parent->child_capacity);
    }
    parent->children[parent->child_count++] = child;
}

void ast_free(ASTNode *node) {
    if (!node) return;
    free(node->name);
    free(node->value);
    free(node->type_name);
    for (size_t i = 0; i < node->child_count; i++) {
        ast_free(node->children[i]);
    }
    free(node->children);
    free(node);
}

Parser *parser_create(Lexer *lex, ErrorHandler *errors) {
    Parser *p = malloc(sizeof(Parser));
    p->tokens = lex->tokens;
    p->token_count = lex->token_count;
    p->pos = 0;
    p->errors = errors;
    return p;
}

void parser_free(Parser *p) {
    free(p);
}

Token *parser_current(Parser *p) {
    if (p->pos < p->token_count) {
        return &p->tokens[p->pos];
    }
    return &p->tokens[p->token_count - 1];
}

Token *parser_peek(Parser *p, int offset) {
    if (p->pos + offset < p->token_count) {
        return &p->tokens[p->pos + offset];
    }
    return &p->tokens[p->token_count - 1];
}

void parser_advance(Parser *p) {
    if (p->pos < p->token_count - 1) {
        p->pos++;
    }
}

ASTNode *parser_parse_primary(Parser *p);
ASTNode *parser_parse_expression(Parser *p);
ASTNode *parser_parse_statement(Parser *p);
ASTNode *parser_parse_block(Parser *p);

ASTNode *parser_parse_primary(Parser *p) {
    Token *tok = parser_current(p);
    ASTNode *node = NULL;
    
    if (tok->type == TOKEN_INT) {
        node = ast_create(NODE_INT_LIT);
        node->value = strdup(tok->value);
        parser_advance(p);
    } else if (tok->type == TOKEN_FLOAT) {
        node = ast_create(NODE_FLOAT_LIT);
        node->value = strdup(tok->value);
        parser_advance(p);
    } else if (tok->type == TOKEN_STRING) {
        node = ast_create(NODE_STRING_LIT);
        node->value = strdup(tok->value);
        parser_advance(p);
    } else if (tok->type == TOKEN_BOOL) {
        node = ast_create(NODE_BOOL_LIT);
        node->value = strdup(tok->value);
        parser_advance(p);
    } else if (tok->type == TOKEN_IDENT) {
        node = ast_create(NODE_IDENT);
        node->name = strdup(tok->value);
        parser_advance(p);
        
        // Check for function call
        if (parser_current(p)->type == TOKEN_LPAREN) {
            ASTNode *call = ast_create(NODE_CALL);
            ast_add_child(call, node);
            parser_advance(p); // skip (
            
            while (parser_current(p)->type != TOKEN_RPAREN && parser_current(p)->type != TOKEN_EOF) {
                ast_add_child(call, parser_parse_expression(p));
                if (parser_current(p)->type == TOKEN_COMMA) {
                    parser_advance(p);
                }
            }
            if (parser_current(p)->type == TOKEN_RPAREN) {
                parser_advance(p);
            }
            return call;
        }
    } else if (tok->type == TOKEN_LPAREN) {
        parser_advance(p);
        node = parser_parse_expression(p);
        if (parser_current(p)->type == TOKEN_RPAREN) {
            parser_advance(p);
        }
    } else if (tok->type == TOKEN_MINUS) {
        parser_advance(p);
        ASTNode *unary = ast_create(NODE_UNARY_OP);
        unary->value = strdup("-");
        ast_add_child(unary, parser_parse_primary(p));
        return unary;
    } else if (tok->type == TOKEN_NOT) {
        parser_advance(p);
        ASTNode *unary = ast_create(NODE_UNARY_OP);
        unary->value = strdup("!");
        ast_add_child(unary, parser_parse_primary(p));
        return unary;
    } else if (tok->type == TOKEN_AMPERSAND) {
        parser_advance(p);
        ASTNode *addr = ast_create(NODE_ADDRESSOF);
        ast_add_child(addr, parser_parse_primary(p));
        return addr;
    } else if (tok->type == TOKEN_STAR) {
        parser_advance(p);
        ASTNode *deref = ast_create(NODE_DEREF);
        ast_add_child(deref, parser_parse_primary(p));
        return deref;
    }
    
    if (!node) {
        node = ast_create(NODE_IDENT);
        node->name = strdup("null");
    }
    
    return node;
}

ASTNode *parser_parse_expression(Parser *p) {
    ASTNode *left = parser_parse_primary(p);
    
    while (parser_current(p)->type == TOKEN_PLUS || parser_current(p)->type == TOKEN_MINUS ||
           parser_current(p)->type == TOKEN_STAR || parser_current(p)->type == TOKEN_SLASH ||
           parser_current(p)->type == TOKEN_PERCENT || parser_current(p)->type == TOKEN_EQ ||
           parser_current(p)->type == TOKEN_NEQ || parser_current(p)->type == TOKEN_LT ||
           parser_current(p)->type == TOKEN_GT || parser_current(p)->type == TOKEN_LTE ||
           parser_current(p)->type == TOKEN_GTE || parser_current(p)->type == TOKEN_AND ||
           parser_current(p)->type == TOKEN_OR) {
        
        Token *op = parser_current(p);
        parser_advance(p);
        ASTNode *right = parser_parse_primary(p);
        
        ASTNode *binop = ast_create(NODE_BINARY_OP);
        binop->value = strdup(op->value);
        ast_add_child(binop, left);
        ast_add_child(binop, right);
        left = binop;
    }
    
    return left;
}

ASTNode *parser_parse_block(Parser *p) {
    ASTNode *block = ast_create(NODE_BLOCK);
    
    if (parser_current(p)->type == TOKEN_LBRACE) {
        parser_advance(p);
    }
    
    while (parser_current(p)->type != TOKEN_RBRACE && parser_current(p)->type != TOKEN_EOF) {
        ASTNode *stmt = parser_parse_statement(p);
        if (stmt) {
            ast_add_child(block, stmt);
        }
        
        if (parser_current(p)->type == TOKEN_SEMICOLON) {
            parser_advance(p);
        }
    }
    
    if (parser_current(p)->type == TOKEN_RBRACE) {
        parser_advance(p);
    }
    
    return block;
}

ASTNode *parser_parse_statement(Parser *p) {
    Token *tok = parser_current(p);
    
    if (tok->type == TOKEN_RET) {
        parser_advance(p);
        ASTNode *ret = ast_create(NODE_RETURN);
        if (parser_current(p)->type != TOKEN_SEMICOLON && parser_current(p)->type != TOKEN_RBRACE) {
            ast_add_child(ret, parser_parse_expression(p));
        }
        return ret;
    } else if (tok->type == TOKEN_IF) {
        parser_advance(p);
        ASTNode *ifnode = ast_create(NODE_IF);
        
        if (parser_current(p)->type == TOKEN_LPAREN) {
            parser_advance(p);
            ast_add_child(ifnode, parser_parse_expression(p));
            if (parser_current(p)->type == TOKEN_RPAREN) {
                parser_advance(p);
            }
        }
        
        ast_add_child(ifnode, parser_parse_block(p));
        
        if (parser_current(p)->type == TOKEN_ELSE) {
            parser_advance(p);
            ast_add_child(ifnode, parser_parse_block(p));
        }
        
        return ifnode;
    } else if (tok->type == TOKEN_LOOP) {
        parser_advance(p);
        ASTNode *loop = ast_create(NODE_LOOP);
        
        if (parser_current(p)->type == TOKEN_LPAREN) {
            parser_advance(p);
            ast_add_child(loop, parser_parse_expression(p));
            if (parser_current(p)->type == TOKEN_RPAREN) {
                parser_advance(p);
            }
        }
        
        ast_add_child(loop, parser_parse_block(p));
        return loop;
    } else if (tok->type == TOKEN_IDENT && parser_peek(p, 1)->type == TOKEN_COLON) {
        // Variable declaration: x: i = 10
        ASTNode *vardecl = ast_create(NODE_VAR_DECL);
        vardecl->name = strdup(tok->value);
        parser_advance(p); // skip name
        parser_advance(p); // skip :
        
        // Read type
        if (parser_current(p)->type == TOKEN_IDENT) {
            vardecl->type_name = strdup(parser_current(p)->value);
            parser_advance(p);
        }
        
        // Check for assignment
        if (parser_current(p)->type == TOKEN_ASSIGN) {
            parser_advance(p);
            ast_add_child(vardecl, parser_parse_expression(p));
        }
        
        return vardecl;
    } else if (tok->type == TOKEN_IDENT && parser_peek(p, 1)->type == TOKEN_ASSIGN) {
        // Variable assignment: x = 10
        ASTNode *assign = ast_create(NODE_VAR_ASSIGN);
        
        ASTNode *ident = ast_create(NODE_IDENT);
        ident->name = strdup(tok->value);
        ast_add_child(assign, ident);
        
        parser_advance(p); // skip name
        parser_advance(p); // skip =
        
        ast_add_child(assign, parser_parse_expression(p));
        return assign;
    } else {
        // Expression statement
        ASTNode *expr = parser_parse_expression(p);
        if (expr->type != NODE_IDENT || strcmp(expr->name, "null") != 0) {
            ASTNode *exprstmt = ast_create(NODE_EXPR_STMT);
            ast_add_child(exprstmt, expr);
            return exprstmt;
        }
        ast_free(expr);
        return NULL;
    }
}

ASTNode *parser_parse_function(Parser *p) {
    ASTNode *func = ast_create(NODE_FUNCTION);
    
    parser_advance(p); // skip 'fn'
    
    if (parser_current(p)->type == TOKEN_IDENT) {
        func->name = strdup(parser_current(p)->value);
        parser_advance(p);
    }
    
    // Skip parameters for now
    if (parser_current(p)->type == TOKEN_LPAREN) {
        parser_advance(p);
        while (parser_current(p)->type != TOKEN_RPAREN && parser_current(p)->type != TOKEN_EOF) {
            parser_advance(p);
        }
        if (parser_current(p)->type == TOKEN_RPAREN) {
            parser_advance(p);
        }
    }
    
    // Skip return type
    if (parser_current(p)->type == TOKEN_COLON) {
        parser_advance(p);
        while (parser_current(p)->type != TOKEN_LBRACE && parser_current(p)->type != TOKEN_EOF) {
            parser_advance(p);
        }
    }
    
    if (parser_current(p)->type == TOKEN_LBRACE) {
        ast_add_child(func, parser_parse_block(p));
    }
    
    return func;
}

ASTNode *parser_parse_program(Parser *p) {
    ASTNode *program = ast_create(NODE_PROGRAM);
    
    while (parser_current(p)->type != TOKEN_EOF) {
        if (parser_current(p)->type == TOKEN_FN) {
            ast_add_child(program, parser_parse_function(p));
        } else if (parser_current(p)->type == TOKEN_USE) {
            // Skip imports for now
            while (parser_current(p)->type != TOKEN_SEMICOLON && parser_current(p)->type != TOKEN_EOF) {
                parser_advance(p);
            }
            parser_advance(p);
        } else {
            parser_advance(p);
        }
    }
    
    return program;
}

/* ============================================================================
 * SEMANTIC ANALYZER (stub)
 * ============================================================================ */

typedef struct {
    void *symbols;
    ErrorHandler *errors;
} SemanticAnalyzer;

SemanticAnalyzer *semantic_analyzer_create(ErrorHandler *errors) {
    SemanticAnalyzer *sa = malloc(sizeof(SemanticAnalyzer));
    sa->errors = errors;
    return sa;
}

void semantic_analyze(SemanticAnalyzer *sa, ASTNode *program) {
    // Stub - just return
}

void semantic_analyzer_free(SemanticAnalyzer *sa) {
    free(sa);
}
