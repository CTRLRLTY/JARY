#include <stdarg.h>

#include "parser.h"
#include "fnv.h"

#define RETURN_PARSE_ERROR(fn) \
    do { ParseError res = fn; if (res != PARSE_SUCCESS) return res; } while(0)

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef ParseError (*ParseFn)(ParseTknStream* tkns, ASTExpr* expr);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;


// fill token with current and advance
static ParseError tkns_next(ParseTknStream* tknlist, TKN* token) {
    if (vec_get(&tknlist->tkns, tknlist->idx++, token, sizeof(TKN)) != VEC_SUCCESS)
        return ERR_PARSE_GET_TKNS_VECTOR;

    return PARSE_SUCCESS;
}

// fill token with current and advance
static ParseError tkns_current(ParseTknStream* tknlist, TKN* token) {
    if (vec_get(&tknlist->tkns, tknlist->idx, token, sizeof(TKN)) != VEC_SUCCESS)
        return ERR_PARSE_GET_TKNS_VECTOR;

    return PARSE_SUCCESS;
}

static ParseError tkns_prev(ParseTknStream* tknlist, TKN* token) {
    if (vec_get(&tknlist->tkns, tknlist->idx - 1, token, sizeof(TKN)) != VEC_SUCCESS)
        return ERR_PARSE_GET_TKNS_VECTOR;

    return PARSE_SUCCESS;
}

static ParseError skip(ParseTknStream* tkns, TKN* current, size_t varcount, ...) {
    ParseError res = PARSE_SUCCESS;
    va_list types;
    
    TKN token;

    while (tkns->idx < tkns->tkns.count) {
        if ((res = tkns_next(tkns, &token)) != PARSE_SUCCESS) 
            goto end;

        va_start(types, varcount);
        
        for (size_t i = 0; i < varcount; ++i) {
            TknType type = va_arg(types, TknType);
            if (token.type == type)
                goto next;
        }
        break;
        next:
    }

end:
    *current = token;
    va_end(types);
    
    
    return PARSE_SUCCESS;
} 

static ParseRule* get_rule(TknType type);

static ParseError _precedence(ParseTknStream* tkns, ASTExpr* expr, Precedence prec) {
    TKN prevtkn;
    TKN currtkn;    
    ParseFn prefixfn;

    RETURN_PARSE_ERROR(tkns_next(tkns, &prevtkn));

    prefixfn = get_rule(prevtkn.type)->prefix;

    RETURN_PARSE_ERROR(prefixfn(tkns, expr));

    RETURN_PARSE_ERROR(tkns_current(tkns, &currtkn));

    while (prec <= get_rule(currtkn.type)->precedence) {
        RETURN_PARSE_ERROR(tkns_next(tkns, &prevtkn));
        ParseFn infixfn = get_rule(prevtkn.type)->infix;
        RETURN_PARSE_ERROR(infixfn(tkns, expr));
        RETURN_PARSE_ERROR(tkns_current(tkns, &currtkn));
    }
    
    return PARSE_SUCCESS;
}

static ParseError _literal(ParseTknStream* tkns, ASTExpr* expr) {
    TKN token;
    RETURN_PARSE_ERROR(tkns_prev(tkns, &token));

    switch (token.type)
    {
        case TKN_STRING:
            expr->type = EXPR_STRING;
            expr->as.string = token;
            break;
        case TKN_NUMBER:
            expr->type = EXPR_NUMBER;
            expr->as.number = token;
            break;
        case TKN_FALSE:
        case TKN_TRUE:
            expr->type = EXPR_BOOLEAN;
            expr->as.boolean = token;
            break;
        default:
            return ERR_PARSE_UNEXPECTED_TKN;
    }

    return PARSE_SUCCESS;
}

static ParseError _expression(ParseTknStream* tkns, ASTExpr* expr) {
    return _precedence(tkns, expr, PREC_ASSIGNMENT);
}

static ParseError _function(ParseTknStream* tkns, ASTExpr* expr) {
    TKN token;
    
    RETURN_PARSE_ERROR(tkns_prev(tkns, &token));

    if (!ASTFunc_init(&expr->as.func))
        return ERR_PARSE;
    
    expr->type = EXPR_FUNC;
    expr->as.func.name = token;

    return PARSE_SUCCESS;
}

static ParseError _call(ParseTknStream* tkns, ASTExpr* expr) {
    TKN token;

    RETURN_PARSE_ERROR(tkns_current(tkns, &token));

    if (token.type != TKN_RIGHT_PAREN) {
        do {
            ASTExpr paramexpr;

            _expression(tkns, &paramexpr);

            if (expr->as.func.params.count == 255) 
                return ERR_PARSE;

            if (vec_push(&expr->as.func.params, &paramexpr, sizeof(paramexpr)) != VEC_SUCCESS)
                return ERR_PARSE;
            
           RETURN_PARSE_ERROR(tkns_next(tkns, &token)); 
        } while(token.type == TKN_COMMA);
    }

    if (token.type != TKN_RIGHT_PAREN)
        return ERR_PARSE_UNEXPECTED_TKN;

    return PARSE_SUCCESS;
}

static ParseError _input_list(ParseTknStream* tkns, Vector* inputlist) {
    TKN token;

    RETURN_PARSE_ERROR(tkns_next(tkns, &token));

    if (token.type != TKN_COLON)
        return ERR_PARSE_UNEXPECTED_TKN;

    RETURN_PARSE_ERROR(tkns_next(tkns, &token));

    if (token.type != TKN_NEWLINE)
        return ERR_PARSE_UNEXPECTED_TKN;
    
    do {
        RETURN_PARSE_ERROR(tkns_next(tkns, &token));

        if (token.type != TKN_PVAR)
            return ERR_PARSE_UNEXPECTED_TKN;
        
        ASTInput input;

        input.name = token;

        RETURN_PARSE_ERROR(tkns_next(tkns, &token));

        if (token.type != TKN_EQUAL)
            return ERR_PARSE_UNEXPECTED_TKN;

        RETURN_PARSE_ERROR(_expression(tkns, &input.expr));

        if (vec_push(inputlist, &input, sizeof(input)) != VEC_SUCCESS)
            return ERR_PARSE;

        tkns_current(tkns, &token);
    } while (token.type != TKN_NEWLINE);

    return PARSE_SUCCESS;
}

static ParseError _declare_rule(Parser* p, ASTRule* rule) {
    TKN token; 

    RETURN_PARSE_ERROR(tkns_next(&p->tkns, &token));

    if(token.type != TKN_IDENTIFIER)
        return ERR_PARSE_UNEXPECTED_TKN;

    rule->name = token;

    RETURN_PARSE_ERROR(tkns_next(&p->tkns, &token));

    if (token.type != TKN_LEFT_BRACE)
        return ERR_PARSE_UNEXPECTED_TKN;
    
    RETURN_PARSE_ERROR(skip(&p->tkns, &token, 1, TKN_NEWLINE));

    switch (token.type) {
        case TKN_INPUT:
            RETURN_PARSE_ERROR(_input_list(&p->tkns, &rule->inputs));
            break;
        case TKN_MATCH:
            break;
        case TKN_TARGET:
            break;
        case TKN_CONDITION:
            break;
        default:
            return ERR_PARSE_UNEXPECTED_TKN;
    }

    RETURN_PARSE_ERROR(skip(&p->tkns, &token, 1, TKN_NEWLINE));

    if (token.type != TKN_RIGHT_BRACE)
        return ERR_PARSE_UNEXPECTED_TKN;

    return PARSE_SUCCESS;
}

static ParseError _entry(Parser* p, ASTProg* m) {
    TKN token;
    
    RETURN_PARSE_ERROR(skip(&p->tkns, &token, 1, TKN_NEWLINE));

    switch (token.type) {
        case TKN_RULE: 
            ASTRule rule;
            if (!ASTRule_init(&rule))
                return ERR_PARSE;
            
            RETURN_PARSE_ERROR(_declare_rule(p, &rule)); 
            
            if (vec_push(&m->rule, &rule, sizeof(rule)) != VEC_SUCCESS)
                return ERR_PARSE;
            
            break;
        default:
            return ERR_PARSE_UNEXPECTED_TKN;
    }

    return PARSE_SUCCESS;
}

static ParseRule rules[] = {
    [TKN_LEFT_PAREN]    = {NULL, _call, PREC_CALL},
    [TKN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TKN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TKN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TKN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TKN_COLON]         = {NULL,     NULL,   PREC_NONE},
    [TKN_NEWLINE]       = {NULL,     NULL,   PREC_NONE},

    [TKN_TARGET]        = {NULL,     NULL,   PREC_NONE},
    [TKN_INPUT]         = {NULL,     NULL,   PREC_NONE},
    [TKN_MATCH]         = {NULL,     NULL,   PREC_NONE},
    [TKN_CONDITION]     = {NULL,     NULL,   PREC_NONE},

    [TKN_STRING]        = {_literal, NULL, PREC_NONE},
    [TKN_NUMBER]        = {_literal, NULL, PREC_NONE},
    [TKN_FALSE]         = {_literal, NULL, PREC_NONE},
    [TKN_TRUE]          = {_literal, NULL, PREC_NONE},
    [TKN_IDENTIFIER]    = {_function, NULL, PREC_NONE}
};

static ParseRule* get_rule(TknType type) {
    return &rules[type];
}

ParseError parse_tokens(Parser* p, Vector* tkns, ASTProg* m) {
    if (p == NULL)
        return ERR_PARSE_NULL_ARGS;

    if (tkns->count <= 0)
        return ERR_PARSE_EMPTY_TKNS_VECTOR;

    p->tkns.tkns = *tkns;

    // -1 to not parse entry on last EOF tkn
    while (p->tkns.idx < tkns->count - 1) {
        RETURN_PARSE_ERROR(_entry(p, m)); 
    }

    TKN token;

    RETURN_PARSE_ERROR(tkns_current(&p->tkns, &token));

    if (token.type != TKN_EOF)
        return ERR_PARSE_UNEXPECTED_TKN;

    return PARSE_SUCCESS;
}

ParseError parse_init(Parser* p) {
    if (p == NULL)
        return ERR_PARSE_NULL_ARGS;

    p->ended = true;
    p->tkns.idx = 0;

    if (vec_init(&p->tkns.tkns, sizeof(TKN)) != VEC_SUCCESS)
        return ERR_PARSE_INIT;
    
    return PARSE_SUCCESS;
}

ParseError parse_free(Parser* p) {
    if (p == NULL)
        return ERR_PARSE_NULL_ARGS; 
    
    if (vec_free(&p->tkns.tkns) != VEC_SUCCESS)
        return ERR_PARSE_DOUBLE_FREE;

    p->tkns.idx = 0;
    p->ended = true;
    
    return PARSE_SUCCESS;
}