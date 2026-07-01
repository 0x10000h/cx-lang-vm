#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bytecode.h"

/* ============================================================================
 * TYPE & STRUCTURE FORWARD DECLARATIONS
 * ============================================================================ */

/* Error Handler */
typedef struct {
    char *messages[100];
    int count;
} ErrorHandler;

/* Token types */
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

/* Lexer */
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

/* Parser */
typedef struct {
    Token *tokens;
    size_t token_count;
    size_t pos;
    ErrorHandler *errors;
} Parser;

/* AST Node */
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

/* Semantic Analyzer */
typedef struct Scope {
    void *symbols;
    int symbol_count;
    struct Scope *parent;
} Scope;

typedef struct {
    Scope *global_scope;
    Scope *current_scope;
    ErrorHandler *errors;
    int stack_offset;
    char *current_function;
    char *current_function_return_type;
} SemanticAnalyzer;

/* ============================================================================
 * FORWARD FUNCTION DECLARATIONS
 * ============================================================================ */

ErrorHandler *eh_create(void);
void eh_print_errors(ErrorHandler *eh);
void eh_free(ErrorHandler *eh);

Lexer *lexer_create(const char *source, ErrorHandler *errors);
void lexer_tokenize(Lexer *lex);
void lexer_free(Lexer *lex);

Parser *parser_create(Lexer *lex, ErrorHandler *errors);
ASTNode *parser_parse_program(Parser *p);
void parser_free(Parser *p);

SemanticAnalyzer *semantic_analyzer_create(ErrorHandler *errors);
void semantic_analyze(SemanticAnalyzer *sa, ASTNode *program);
void semantic_analyzer_free(SemanticAnalyzer *sa);

void ast_free(ASTNode *node);

/* ============================================================================
 * MAIN FUNCTION
 * ============================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.cx>\n", argv[0]);
        return 1;
    }
    
    const char *input_file = argv[1];
    
    FILE *f = fopen(input_file, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file %s\n", input_file);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *source = malloc(fsize + 1);
    fread(source, 1, fsize, f);
    source[fsize] = '\0';
    fclose(f);
    
    ErrorHandler *errors = eh_create();
    
    Lexer *lex = lexer_create(source, errors);
    lexer_tokenize(lex);
    
    if (errors->count > 0) {
        eh_print_errors(errors);
        return 1;
    }
    
    Parser *parser = parser_create(lex, errors);
    ASTNode *ast = parser_parse_program(parser);
    
    if (errors->count > 0) {
        eh_print_errors(errors);
        return 1;
    }
    
    SemanticAnalyzer *sa = semantic_analyzer_create(errors);
    semantic_analyze(sa, ast);
    
    if (errors->count > 0) {
        eh_print_errors(errors);
        return 1;
    }
    
    BytecodeModule *module = bytecode_module_create();
    generate_bytecode(module, ast);
    
    VM *vm = vm_create(module);
    int result = vm_execute(vm);
    
    vm_free(vm);
    bytecode_module_free(module);
    semantic_analyzer_free(sa);
    parser_free(parser);
    ast_free(ast);
    lexer_free(lex);
    eh_free(errors);
    free(source);
    
    return result;
}
