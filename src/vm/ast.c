#include "ast.h"

#include "vector.h"

size_t ast_degree(ASTNode* ast) {
    size_t sz = jary_vec_size(ast->child);

    return sz;
}

void ast_free(ASTNode* ast) {
    return;
}