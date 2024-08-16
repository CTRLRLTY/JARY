#include "ast.h"

#include "vector.h"

size_t ast_degree(ASTNode* ast) {
    size_t sz = jary_vec_size(ast->child);

    return sz;
}

void ast_free(ASTNode* ast) {
    return;
}

void ast_meta_free(ASTMetadata* m) {
    return;
}


BasicBlock* find_basic_block(size_t** bbkey, BasicBlock** bbval, size_t block) {
    jary_assert(jary_vec_size(*bbkey) == jary_vec_size(*bbval));

    size_t* keys = *bbkey;
    BasicBlock* vals = *bbval;
    size_t length = jary_vec_size(keys);

    for (size_t i = 0; i < length; ++i) {
        if (block == keys[i])
            return &vals[0];
    }

    return NULL;
}