#include "ast.h"

#include "vector.h"

size_t ast_degree(ASTNode* ast) {
    size_t sz = vecsize(ast->child);

    return sz;
}

void ast_free(ASTNode* ast) {
    return;
}

void ast_meta_free(ASTMetadata* m) {
    return;
}


BasicBlock* find_basic_block(size_t** bbkey, BasicBlock** bbval, size_t block) {
    jary_assert(vecsize(*bbkey) == vecsize(*bbval));

    size_t* keys = *bbkey;
    BasicBlock* vals = *bbval;
    size_t length = vecsize(keys);

    for (size_t i = 0; i < length; ++i) {
        if (block == keys[i])
            return &vals[0];
    }

    return NULL;
}