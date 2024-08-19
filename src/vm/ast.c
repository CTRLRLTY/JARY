#include "ast.h"

void ast_free(ASTNode* ast) {
    if (ast == NULL)
        return;
    
    // for (size_t i = 0; i < vecsize(ast->child); ++i) {
    //     ASTNode* v = &ast->child[i];
    //     ast_free(v);
    // }

    // if (vecsize(ast->child)) {
    //     vecfree(ast->child);
    // }

    return;
}

void ast_meta_free(ASTMetadata* m) {
    return;
}
