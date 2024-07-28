#include "ast.h"

bool ASTProg_init(ASTProg* prog) {
    if (vec_init(&prog->imports, sizeof(ASTImport)) != VEC_SUCCESS)
        return false;

    if (vec_init(&prog->rule, sizeof(ASTRule)) != VEC_SUCCESS)
        return false;
    
    return true;
}

bool ASTProg_free(ASTProg* prog) {
    if (vec_free(&prog->imports) != VEC_SUCCESS)
        return false;

    if (vec_free(&prog->rule) != VEC_SUCCESS)
        return false;

    return true;
}

bool ASTRule_init(ASTRule* rule) {
    if (vec_init(&rule->inputs, sizeof(ASTInput)) != VEC_SUCCESS)
        return false;
    
    if (vec_init(&rule->match, sizeof(ASTMatch)) != VEC_SUCCESS)
        return false;
    
    if (vec_init(&rule->conditions, sizeof(ASTCondition)) != VEC_SUCCESS)
        return false;
    
    if (vec_init(&rule->targets, sizeof(ASTTarget)) != VEC_SUCCESS)
        return false;

    return true;
}

bool ASTRule_free(ASTRule* rule) {
    if (vec_free(&rule->inputs) != VEC_SUCCESS)
        return false;
    
    if (vec_free(&rule->match) != VEC_SUCCESS)
        return false;
    
    if (vec_free(&rule->conditions) != VEC_SUCCESS)
        return false;
    
    if (vec_free(&rule->targets) != VEC_SUCCESS)
        return false;

    return true;
}

bool ASTFunc_init(ASTFunc* fun) {
    if (vec_init(&fun->params, sizeof(ASTExpr)) != VEC_SUCCESS)
        return false;
    
    return true;
}

bool ASTFunc_free(ASTFunc* fun) {
    if (vec_free(&fun->params) != VEC_SUCCESS)
        return false;
    
    return true;
}