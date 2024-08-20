#include "ast.h"

#include "memory.h"

static void free_ast_err(ASTError* err) {
    jary_free(err->lexeme);
    jary_free(err->linestr);
    jary_free(err->msg);
}

void free_ast(ASTNode* ast) {
    if (ast == NULL)
        return;
    
    for (size_t i = 0; i < ast->degree; ++i) {
        ASTNode v = ast->child[i];

        free_ast(&v);
    }
    
    jary_free(ast->child);
    ast->child = NULL;
}

void free_ast_meta(ASTMetadata* m) {
    jary_free(m->src);

    if (m->errors) {
        for (size_t i = 0; i < m->errsz; ++i) {
            free_ast_err(&m->errors[i]);
        }
        jary_free(m->errors);
    }

    for (size_t i = 0; i < m->tknsz; ++i) {
        jary_free(m->tkns[i]);
    }

    jary_free(m->tkns);

    m->src = NULL;
    m->errors = NULL;
    m->tkns = NULL;

    return;
}
