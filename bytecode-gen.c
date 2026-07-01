#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bytecode.h"

/* ============================================================================
 * BYTECODE MODULE
 * ============================================================================ */

ByteCodeModule *bytecode_module_create(void) {
    BytecodeModule *module = malloc(sizeof(BytecodeModule));
    module->instructions = malloc(sizeof(Instruction) * 4096);
    module->instr_count = 0;
    module->instr_capacity = 4096;
    
    module->string_pool = malloc(sizeof(char*) * 1024);
    module->string_count = 0;
    module->string_capacity = 1024;
    
    module->function_names = malloc(sizeof(char*) * 256);
    module->function_offsets = malloc(sizeof(int64_t) * 256);
    module->function_count = 0;
    module->function_capacity = 256;
    
    return module;
}

void bytecode_module_free(BytecodeModule *module) {
    free(module->instructions);
    
    for (size_t i = 0; i < module->string_count; i++) {
        free(module->string_pool[i]);
    }
    free(module->string_pool);
    
    for (size_t i = 0; i < module->function_count; i++) {
        free(module->function_names[i]);
    }
    free(module->function_names);
    free(module->function_offsets);
    
    free(module);
}

void bytecode_emit(BytecodeModule *module, Opcode op, int64_t arg1, int64_t arg2) {
    if (module->instr_count >= module->instr_capacity) {
        module->instr_capacity *= 2;
        module->instructions = realloc(module->instructions, sizeof(Instruction) * module->instr_capacity);
    }
    
    module->instructions[module->instr_count].op = op;
    module->instructions[module->instr_count].arg1 = arg1;
    module->instructions[module->instr_count].arg2 = arg2;
    module->instructions[module->instr_count].str_arg = NULL;
    module->instr_count++;
}

void bytecode_emit_str(BytecodeModule *module, Opcode op, const char *str) {
    if (module->instr_count >= module->instr_capacity) {
        module->instr_capacity *= 2;
        module->instructions = realloc(module->instructions, sizeof(Instruction) * module->instr_capacity);
    }
    
    module->instructions[module->instr_count].op = op;
    module->instructions[module->instr_count].arg1 = 0;
    module->instructions[module->instr_count].arg2 = 0;
    module->instructions[module->instr_count].str_arg = strdup(str);
    module->instr_count++;
}

int bytecode_add_string(BytecodeModule *module, const char *str) {
    if (module->string_count >= module->string_capacity) {
        module->string_capacity *= 2;
        module->string_pool = realloc(module->string_pool, sizeof(char*) * module->string_capacity);
    }
    
    int index = module->string_count;
    module->string_pool[module->string_count++] = strdup(str);
    return index;
}

int bytecode_add_function(BytecodeModule *module, const char *name, int64_t offset) {
    if (module->function_count >= module->function_capacity) {
        module->function_capacity *= 2;
        module->function_names = realloc(module->function_names, sizeof(char*) * module->function_capacity);
        module->function_offsets = realloc(module->function_offsets, sizeof(int64_t) * module->function_capacity);
    }
    
    int index = module->function_count;
    module->function_names[module->function_count] = strdup(name);
    module->function_offsets[module->function_count] = offset;
    module->function_count++;
    return index;
}

/* ============================================================================
 * BYTECODE GENERATION FROM AST
 * ============================================================================ */

/* Forward declarations from compiler.c */
typedef struct ASTNode ASTNode;

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

struct ASTNode {
    NodeType type;
    char *name;
    char *value;
    char *type_name;
    int line, column;
    struct ASTNode **children;
    size_t child_count;
    size_t child_capacity;
};

/* Variable scope tracking for bytecode generation */
typedef struct {
    char *name;
    int stack_offset;
} VarInfo;

typedef struct GenScope {
    VarInfo *vars;
    int var_count;
    struct GenScope *parent;
} GenScope;

typedef struct {
    BytecodeModule *module;
    GenScope *scope;
} CodeGen;

GenScope *gen_scope_create(GenScope *parent) {
    GenScope *scope = malloc(sizeof(GenScope));
    scope->vars = malloc(sizeof(VarInfo) * 256);
    scope->var_count = 0;
    scope->parent = parent;
    return scope;
}

void gen_scope_free(GenScope *scope) {
    if (!scope) return;
    for (int i = 0; i < scope->var_count; i++) {
        free(scope->vars[i].name);
    }
    free(scope->vars);
    free(scope);
}

int gen_scope_find_var(GenScope *scope, const char *name) {
    while (scope) {
        for (int i = 0; i < scope->var_count; i++) {
            if (strcmp(scope->vars[i].name, name) == 0) {
                return scope->vars[i].stack_offset;
            }
        }
        scope = scope->parent;
    }
    return -1;
}

void gen_scope_define_var(GenScope *scope, const char *name, int offset) {
    if (scope->var_count >= 256) return;
    scope->vars[scope->var_count].name = strdup(name);
    scope->vars[scope->var_count].stack_offset = offset;
    scope->var_count++;
}

void generate_expr(CodeGen *gen, ASTNode *expr);
void generate_stmt(CodeGen *gen, ASTNode *stmt);

void generate_expr(CodeGen *gen, ASTNode *expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case NODE_INT_LIT: {
            int64_t val = strtoll(expr->value, NULL, 10);
            bytecode_emit(gen->module, OP_LOAD_INT, val, 0);
            break;
        }
        case NODE_FLOAT_LIT: {
            double val = strtod(expr->value, NULL);
            int64_t bits = *(int64_t*)&val;
            bytecode_emit(gen->module, OP_LOAD_FLOAT, bits, 0);
            break;
        }
        case NODE_STRING_LIT: {
            int str_idx = bytecode_add_string(gen->module, expr->value);
            bytecode_emit(gen->module, OP_LOAD_STR, str_idx, 0);
            break;
        }
        case NODE_BOOL_LIT: {
            int val = strcmp(expr->value, "true") == 0 ? 1 : 0;
            bytecode_emit(gen->module, OP_LOAD_INT, val, 0);
            break;
        }
        case NODE_IDENT: {
            int offset = gen_scope_find_var(gen->scope, expr->name);
            if (offset >= 0) {
                bytecode_emit(gen->module, OP_LOAD_VAR, offset, 0);
            }
            break;
        }
        case NODE_BINARY_OP: {
            generate_expr(gen, expr->children[0]);
            generate_expr(gen, expr->children[1]);
            
            if (strcmp(expr->value, "+") == 0) {
                bytecode_emit(gen->module, OP_ADD, 0, 0);
            } else if (strcmp(expr->value, "-") == 0) {
                bytecode_emit(gen->module, OP_SUB, 0, 0);
            } else if (strcmp(expr->value, "*") == 0) {
                bytecode_emit(gen->module, OP_MUL, 0, 0);
            } else if (strcmp(expr->value, "/") == 0) {
                bytecode_emit(gen->module, OP_DIV, 0, 0);
            } else if (strcmp(expr->value, "%") == 0) {
                bytecode_emit(gen->module, OP_MOD, 0, 0);
            } else if (strcmp(expr->value, "==") == 0) {
                bytecode_emit(gen->module, OP_EQ, 0, 0);
            } else if (strcmp(expr->value, "!=") == 0) {
                bytecode_emit(gen->module, OP_NEQ, 0, 0);
            } else if (strcmp(expr->value, "<") == 0) {
                bytecode_emit(gen->module, OP_LT, 0, 0);
            } else if (strcmp(expr->value, ">") == 0) {
                bytecode_emit(gen->module, OP_GT, 0, 0);
            } else if (strcmp(expr->value, "<=") == 0) {
                bytecode_emit(gen->module, OP_LTE, 0, 0);
            } else if (strcmp(expr->value, ">=") == 0) {
                bytecode_emit(gen->module, OP_GTE, 0, 0);
            } else if (strcmp(expr->value, "&&") == 0) {
                bytecode_emit(gen->module, OP_AND, 0, 0);
            } else if (strcmp(expr->value, "||") == 0) {
                bytecode_emit(gen->module, OP_OR, 0, 0);
            }
            break;
        }
        case NODE_UNARY_OP: {
            generate_expr(gen, expr->children[0]);
            if (strcmp(expr->value, "!") == 0) {
                bytecode_emit(gen->module, OP_NOT, 0, 0);
            } else if (strcmp(expr->value, "-") == 0) {
                bytecode_emit(gen->module, OP_LOAD_INT, -1, 0);
                bytecode_emit(gen->module, OP_MUL, 0, 0);
            }
            break;
        }
        case NODE_CALL: {
            if (expr->children[0]->type == NODE_IDENT) {
                char *func_name = expr->children[0]->name;
                int arg_count = expr->child_count - 1;
                
                for (int i = 1; i < (int)expr->child_count; i++) {
                    generate_expr(gen, expr->children[i]);
                }
                
                char func_call[256];
                snprintf(func_call, sizeof(func_call), "%s", func_name);
                bytecode_emit_str(gen->module, OP_CALL_NATIVE, func_call);
            }
            break;
        }
        default:
            break;
    }
}

void generate_stmt(CodeGen *gen, ASTNode *stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case NODE_VAR_DECL: {
            static int stack_offset = 0;
            
            if (stmt->child_count > 0) {
                generate_expr(gen, stmt->children[0]);
            } else {
                bytecode_emit(gen->module, OP_LOAD_INT, 0, 0);
            }
            
            gen_scope_define_var(gen->scope, stmt->name, stack_offset);
            bytecode_emit(gen->module, OP_STORE_VAR, stack_offset, 0);
            stack_offset += 8;
            break;
        }
        case NODE_VAR_ASSIGN: {
            if (stmt->child_count >= 2) {
                ASTNode *lhs = stmt->children[0];
                generate_expr(gen, stmt->children[1]);
                
                if (lhs->type == NODE_IDENT) {
                    int offset = gen_scope_find_var(gen->scope, lhs->name);
                    if (offset >= 0) {
                        bytecode_emit(gen->module, OP_STORE_VAR, offset, 0);
                    }
                }
            }
            break;
        }
        case NODE_RETURN: {
            if (stmt->child_count > 0) {
                generate_expr(gen, stmt->children[0]);
            }
            bytecode_emit(gen->module, OP_RET, 0, 0);
            break;
        }
        case NODE_IF: {
            if (stmt->child_count >= 2) {
                generate_expr(gen, stmt->children[0]);
                
                size_t jump_addr = gen->module->instr_count;
                bytecode_emit(gen->module, OP_JUMP_IF_FALSE, 0, 0);
                
                ASTNode *then_block = stmt->children[1];
                if (then_block->type == NODE_BLOCK) {
                    for (size_t i = 0; i < then_block->child_count; i++) {
                        generate_stmt(gen, then_block->children[i]);
                    }
                }
                
                if (stmt->child_count >= 3) {
                    size_t else_jump_addr = gen->module->instr_count;
                    bytecode_emit(gen->module, OP_JUMP, 0, 0);
                    gen->module->instructions[jump_addr].arg1 = gen->module->instr_count;
                    
                    ASTNode *else_block = stmt->children[2];
                    if (else_block->type == NODE_BLOCK) {
                        for (size_t i = 0; i < else_block->child_count; i++) {
                            generate_stmt(gen, else_block->children[i]);
                        }
                    }
                    
                    gen->module->instructions[else_jump_addr].arg1 = gen->module->instr_count;
                } else {
                    gen->module->instructions[jump_addr].arg1 = gen->module->instr_count;
                }
            }
            break;
        }
        case NODE_LOOP: {
            size_t loop_start = gen->module->instr_count;
            
            if (stmt->child_count == 2) {
                generate_expr(gen, stmt->children[0]);
                size_t cond_jump = gen->module->instr_count;
                bytecode_emit(gen->module, OP_JUMP_IF_FALSE, 0, 0);
                
                ASTNode *loop_body = stmt->children[1];
                if (loop_body->type == NODE_BLOCK) {
                    for (size_t i = 0; i < loop_body->child_count; i++) {
                        generate_stmt(gen, loop_body->children[i]);
                    }
                }
                
                bytecode_emit(gen->module, OP_JUMP, loop_start, 0);
                gen->module->instructions[cond_jump].arg1 = gen->module->instr_count;
            } else if (stmt->child_count == 1) {
                ASTNode *loop_body = stmt->children[0];
                if (loop_body->type == NODE_BLOCK) {
                    for (size_t i = 0; i < loop_body->child_count; i++) {
                        generate_stmt(gen, loop_body->children[i]);
                    }
                }
                bytecode_emit(gen->module, OP_JUMP, loop_start, 0);
            }
            break;
        }
        case NODE_BLOCK: {
            GenScope *prev_scope = gen->scope;
            gen->scope = gen_scope_create(prev_scope);
            
            for (size_t i = 0; i < stmt->child_count; i++) {
                generate_stmt(gen, stmt->children[i]);
            }
            
            gen_scope_free(gen->scope);
            gen->scope = prev_scope;
            break;
        }
        case NODE_EXPR_STMT: {
            if (stmt->child_count > 0) {
                generate_expr(gen, stmt->children[0]);
                bytecode_emit(gen->module, OP_POP, 0, 0);
            }
            break;
        }
        default:
            break;
    }
}

void generate_bytecode(BytecodeModule *module, ASTNode *ast) {
    if (!ast || ast->type != NODE_PROGRAM) return;
    
    CodeGen gen;
    gen.module = module;
    gen.scope = gen_scope_create(NULL);
    
    for (size_t i = 0; i < ast->child_count; i++) {
        ASTNode *item = ast->children[i];
        
        if (item->type == NODE_FUNCTION) {
            bytecode_add_function(module, item->name, module->instr_count);
            
            if (item->child_count > 0) {
                ASTNode *body = item->children[item->child_count - 1];
                if (body->type == NODE_BLOCK) {
                    for (size_t j = 0; j < body->child_count; j++) {
                        generate_stmt(&gen, body->children[j]);
                    }
                }
            }
            
            bytecode_emit(module, OP_RET, 0, 0);
        }
    }
    
    bytecode_emit(module, OP_HALT, 0, 0);
    
    gen_scope_free(gen.scope);
}
