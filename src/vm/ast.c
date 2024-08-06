#include "ast.h"

void ASTProg_init(ASTProg* prog) {
    jary_vec_init(prog->imports, 10);
    jary_vec_init(prog->rule, 10);
}

void ASTProg_free(ASTProg* prog) {
    jary_vec_free(prog->imports);
    jary_vec_free(prog->rule);
}

void ASTRule_init(ASTRule* rule) {
    jary_vec_init(rule->inputs, 10);
    jary_vec_init(rule->match, 10);
    jary_vec_init(rule->conditions, 10);
    jary_vec_init(rule->targets, 10);
}

void ASTRule_free(ASTRule* rule) {
    jary_vec_free(rule->inputs);
    jary_vec_free(rule->match);
    jary_vec_free(rule->conditions);
    jary_vec_free(rule->targets);
}

void ASTFunc_init(ASTFunc* fun) {
    jary_vec_init(fun->params, 10);
}

void ASTFunc_free(ASTFunc* fun) {
    jary_vec_free(fun->params);
}