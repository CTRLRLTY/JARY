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

typedef ParseError (*ParseFn)(Parser* p, ASTExpr* expr);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;


// fill token with current and advance
static TKN* tkns_next(Parser* p) {
    if (p->idx + 1 >= p->tknsz)
        return NULL;

    return &p->tkns[p->idx++];  
}

// fill token with current and advance
static TKN* tkns_current(Parser* p) {
    if (p->idx >= p->tknsz)
        return NULL;

    return &p->tkns[p->idx];    
}

static TKN* tkns_prev(Parser* p) {
    if (p->idx >= p->tknsz && p->idx-1 < 0)
        return NULL;

    return &p->tkns[p->idx-1];   
}

static TKN* skip_newline(Parser* p) {
    TKN* tkn = NULL; 

    do {
        tkn = tkns_next(p);
    } while ( tkn != NULL && tkn->type == TKN_NEWLINE );
    
    return tkn;
} 

static ParseRule* get_rule(TknType type);

static ParseError _precedence(Parser* p, ASTExpr* expr, Precedence prec) {
    TKN* prevtkn;
    TKN* currtkn;    
    ParseFn prefixfn;

    prevtkn = tkns_next(p);

    if (prevtkn == NULL)
        return ERR_PARSE;

    prefixfn = get_rule(prevtkn->type)->prefix;

    assert(prefixfn != NULL);

    RETURN_PARSE_ERROR(prefixfn(p, expr));

    currtkn = tkns_current(p);

    if (currtkn == NULL)
        return ERR_PARSE;

    while (prec <= get_rule(currtkn->type)->precedence) {
        prevtkn = tkns_next(p);

        if (prevtkn == NULL)
            return ERR_PARSE;

        ParseFn infixfn = get_rule(prevtkn->type)->infix;

        assert(infixfn != NULL);

        RETURN_PARSE_ERROR(infixfn(p, expr));
        
        currtkn = tkns_current(p);

        if (currtkn == NULL)
            return ERR_PARSE;
    }
    
    return PARSE_SUCCESS;
}

static ParseError _literal(Parser* p, ASTExpr* expr) {
    TKN* token = tkns_prev(p);

    if (token == NULL)
        return ERR_PARSE;

    switch (token->type) {
    case TKN_STRING:
        expr->type = EXPR_STRING;
        expr->as.string = *token;
        break;
    case TKN_NUMBER:
        expr->type = EXPR_NUMBER;
        expr->as.number = *token;
        break;
    case TKN_FALSE:
    case TKN_TRUE:
        expr->type = EXPR_BOOLEAN;
        expr->as.boolean = *token;
        break;
    default:
        return ERR_PARSE_UNEXPECTED_TKN;
    }

    return PARSE_SUCCESS;
}

static ParseError _expression(Parser* p, ASTExpr* expr) {
    return _precedence(p, expr, PREC_ASSIGNMENT);
}

static ParseError _function(Parser* p, ASTExpr* expr) {
    TKN* token = tkns_prev(p);

    if (token == NULL)
        return ERR_PARSE;

    ASTFunc_init(&expr->as.func);
    
    expr->type = EXPR_FUNC;
    expr->as.func.name = *token;

    return PARSE_SUCCESS;
}

static ParseError _call(Parser* p, ASTExpr* expr) {
    TKN* token = tkns_current(p);

    if (token == NULL)
        return ERR_PARSE;

    if (token->type != TKN_RIGHT_PAREN) {
        do {
            ASTExpr paramexpr;

            RETURN_PARSE_ERROR(_expression(p, &paramexpr));

            if (ASTFunc_param_size(expr) == 255) 
                return ERR_PARSE;

            ASTFunc_add_param(expr, paramexpr);
            
            token = tkns_next(p);

            if (token == NULL)
                return ERR_PARSE;

        } while(token->type == TKN_COMMA);
    }

    if (token->type != TKN_RIGHT_PAREN)
        return ERR_PARSE_UNEXPECTED_TKN;

    return PARSE_SUCCESS;
}

static ParseError _input_list(Parser* p, jary_vec_t(ASTInput) inputlist) {
    TKN* token = tkns_next(p);

    if (token == NULL)
        return ERR_PARSE;

    if (token->type != TKN_COLON)
        return ERR_PARSE_UNEXPECTED_TKN;

    token = tkns_next(p);

    if (token == NULL)
        return ERR_PARSE;

    if (token->type != TKN_NEWLINE)
        return ERR_PARSE_UNEXPECTED_TKN;
    
    do {
        token = tkns_next(p);

        if (token == NULL)
            return ERR_PARSE;

        if (token->type != TKN_PVAR)
            return ERR_PARSE_UNEXPECTED_TKN;
        
        ASTInput input;

        input.name = *token;

        token = tkns_next(p);

        if (token == NULL)
            return ERR_PARSE;

        if (token->type != TKN_EQUAL)
            return ERR_PARSE_UNEXPECTED_TKN;

        RETURN_PARSE_ERROR(_expression(p, &input.expr));

        jary_vec_push(inputlist, input);

        token = tkns_current(p);
        
        if (token == NULL)
            return ERR_PARSE;

    } while (token != NULL && token->type != TKN_NEWLINE);

    return PARSE_SUCCESS;
}

static ParseError _declare_rule(Parser* p, ASTRule* rule) {
    TKN* token = tkns_next(p);

    if (token == NULL)
        return ERR_PARSE;

    if(token->type != TKN_IDENTIFIER)
        return ERR_PARSE_UNEXPECTED_TKN;

    rule->name = *token;

    token = tkns_next(p);

    if (token == NULL)
        return ERR_PARSE;

    if (token->type != TKN_LEFT_BRACE)
        return ERR_PARSE_UNEXPECTED_TKN;
    
    token = skip_newline(p);

    if (token == NULL)
        return ERR_PARSE;

    switch (token->type) {
        case TKN_INPUT:
            RETURN_PARSE_ERROR(_input_list(p, rule->inputs));
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

    token = skip_newline(p);

    if (token == NULL)
        return ERR_PARSE;

    if (token->type != TKN_RIGHT_BRACE)
        return ERR_PARSE_UNEXPECTED_TKN;

    return PARSE_SUCCESS;
}

static ParseError _entry(Parser* p, ParsedAst* ast, ParsedType* type) {
    TKN* token = skip_newline(p);

    switch (token->type) {
        case TKN_RULE: 
            *type = PARSED_RULE;
            ASTRule_init(&ast->rule);
            
            RETURN_PARSE_ERROR(_declare_rule(p, &ast->rule)); 
            
            break;
        default:
            *type = PARSED_ERR;
            return ERR_PARSE_UNEXPECTED_TKN;
    }

    return PARSE_SUCCESS;
}

static ParseRule rules[] = {
    [TKN_ERR]           = {NULL,     NULL,   PREC_NONE},

    [TKN_LEFT_PAREN]    = {NULL,     _call,  PREC_CALL},
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
    
    [TKN_RULE]          = {NULL,     NULL,   PREC_NONE},
    [TKN_IMPORT]        = {NULL,     NULL,   PREC_NONE},
    [TKN_INGRESS]       = {NULL,     NULL,   PREC_NONE},

    [TKN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TKN_LESSTHAN]      = {NULL,     NULL,   PREC_NONE}, 
    [TKN_GREATERTHAN]   = {NULL,     NULL,   PREC_NONE},
    [TKN_AND]           = {NULL,     NULL,   PREC_NONE},
    [TKN_OR]            = {NULL,     NULL,   PREC_NONE},
    [TKN_ANY]           = {NULL,     NULL,   PREC_NONE},
    [TKN_ALL]           = {NULL,     NULL,   PREC_NONE},

    [TKN_REGEXP]        = {NULL, NULL, PREC_NONE},
    [TKN_STRING]        = {_literal, NULL, PREC_NONE},
    [TKN_NUMBER]        = {_literal, NULL, PREC_NONE},
    [TKN_FALSE]         = {_literal, NULL, PREC_NONE},
    [TKN_TRUE]          = {_literal, NULL, PREC_NONE},

    [TKN_IDENTIFIER]    = {_function, NULL,  PREC_NONE},
    [TKN_PVAR]          = {NULL,     NULL,   PREC_NONE},

    [TKN_CUSTOM]        = {NULL,     NULL,   PREC_NONE}, 
    [TKN_EOF]           = {NULL,     NULL,   PREC_NONE}, 
};

static ParseRule* get_rule(TknType type) {
    return &rules[type];
}

ParseError parse_source(Parser* p, TKN* tkns, size_t length) {
    if (p == NULL || tkns == NULL)
        return ERR_PARSE_NULL_ARGS;

    if (length <= 0) 
        return ERR_PARSE_SOURCE_LENGTH;
    
    p->tkns = tkns;
    p->tknsz = length;
    p->idx = 0;

    return PARSE_SUCCESS;
}

ParseError parse_tokens(Parser* p, ParsedAst* ast, ParsedType* type) {
    if (p == NULL || ast == NULL)
        return ERR_PARSE_NULL_ARGS;

    if (parse_ended(p))
        return ERR_PARSE;

    RETURN_PARSE_ERROR(_entry(p, ast, type)); 

    return PARSE_SUCCESS;
}

bool parse_ended(Parser* p) {
    return p->idx + 1 >= p->tknsz;
}