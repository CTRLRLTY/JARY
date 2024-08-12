#include <string.h>

#include "parser.h"
#include "fnv.h"

#define NODE() ((ASTNode){.left = NULL})
#define MSG_NOT_A_LITERAL "not a literal"
#define MSG_NOT_A_BINARY "not a binary"
#define MSG_NOT_A_INPUT_SECTION "invalid input section"
#define MSG_NOT_A_DECLARATION "invalid declaration"


typedef enum ParseError {
    PARSE_SUCCESS = 0,

    ERR_PARSE_PANIC,
} ParseError;

typedef enum Precedence {
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

typedef ParseError (*ParseFn)(Parser* p, ASTNode* ast);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static ParseError error_node(ASTNode* ast, TKN* tkn, TknType expect, const char* msg) {
    ast_free(ast);

    ast->type = AST_ERR;
    ast->value.err = jary_alloc(sizeof *ast->value.err);

    char* lexeme = jary_alloc(tkn_lexeme_size(tkn));

    tkn_lexeme(tkn, lexeme, tkn_lexeme_size(tkn));

    ast->value.err->got = tkn->type;
    ast->value.err->expect = expect;
    ast->value.err->line = tkn->line;
    ast->value.err->offset = tkn->offset;
    ast->value.err->lexeme = lexeme;
    ast->value.err->msg = (msg != NULL) ? strdup(msg) : NULL;

    return ERR_PARSE_PANIC;
}

static bool ended(Parser* p) {
    return p->idx + 1 >= p->tknsz;
}

// fill token with current and advance
static TKN* next(Parser* p) {
    jary_assert(!(p->idx + 1 >= p->tknsz));

    return &p->tkns[p->idx++];  
}

static TKN* peek(Parser* p) {
    jary_assert(!(p->idx + 1 >= p->tknsz));

    return &p->tkns[p->idx+1]; 
}

// fill token with current and advance
static TKN* current(Parser* p) {
    jary_assert(!(p->idx >= p->tknsz));

    return &p->tkns[p->idx];    
}

static TKN* prev(Parser* p) {
    jary_assert(!(p->idx >= p->tknsz && p->idx-1 < 0));

    return &p->tkns[p->idx-1];   
}

static ParseError synchronize(Parser* p, TknType until) {
    while (!ended(p) && next(p)->type != until);

    return PARSE_SUCCESS;
}

static ParseError syncsection(Parser* p) {
    for(TKN* tkn = next(p);
            !ended(p)                       &&
            tkn->type != TKN_TARGET         &&
            tkn->type != TKN_INPUT          &&
            tkn->type != TKN_MATCH          &&
            tkn->type != TKN_CONDITION      &&
            tkn->type != TKN_RIGHT_BRACE    ;
        tkn = next(p));

    return PARSE_SUCCESS;
}

static ParseRule* get_rule(TknType type);

static ParseError _precedence(Parser* p, ASTNode* ast, Precedence prec) {
    ParseRule* prefixrule = get_rule(next(p)->type);
    ParseFn prefixfn = prefixrule->prefix;

    if (prefixrule->precedence == PREC_PRIMARY) {
        if (prefixfn(p, ast) != PARSE_SUCCESS) 
            return synchronize(p, TKN_NEWLINE);
    } else {
        ast->left = jary_alloc(sizeof(ASTNode));

        jary_assert(prefixfn != NULL);

        if (prefixfn(p, ast->left) != PARSE_SUCCESS)
            return synchronize(p, TKN_NEWLINE);
    }


    while (prec <= get_rule(current(p)->type)->precedence) {
        ParseFn infixfn = get_rule(next(p)->type)->infix;

        if (infixfn(p, ast) != PARSE_SUCCESS)
            return synchronize(p, TKN_NEWLINE);
    }
    
    return PARSE_SUCCESS;
}

static ParseError _literal(Parser* p, ASTNode* ast) {
    TKN* token = prev(p);

    ast->type = AST_LITERAL;
    ast->value.tkn = token;

    switch (token->type) {
    case TKN_STRING:
    case TKN_NUMBER:
    case TKN_FALSE:
    case TKN_TRUE:
    case TKN_REGEXP:
        break;
    default:
        return error_node(ast, token, TKN_NONE, MSG_NOT_A_LITERAL);
    }

    return PARSE_SUCCESS;
}

static ParseError _expression(Parser* p, ASTNode* ast) {
    return _precedence(p, ast, PREC_ASSIGNMENT);
}

static ParseError _binary(Parser* p, ASTNode* ast) {
    TKN* optkn = prev(p);
    ParseRule* oprule = get_rule(optkn->type);
    ASTNode* expr = jary_alloc(sizeof(ASTNode));

    jary_assert(expr != NULL);

    if (_precedence(p, expr, oprule->precedence + 1) != PARSE_SUCCESS)
        return synchronize(p, TKN_NEWLINE);

    ast->value.tkn = optkn;
    ast->right = expr;

    switch (optkn->type) {
    case TKN_ASSIGN:
        ast->type = AST_ASSIGNMENT;
        break;
    
    default:
        return error_node(ast, optkn, TKN_NONE, MSG_NOT_A_BINARY);
    }

    return PARSE_SUCCESS;
}

static ParseError _assign(Parser* p, ASTNode* ast) {
    ast->type = AST_ASSIGNMENT;
    ast->value.tkn = prev(p);

    return PARSE_SUCCESS;
}

static ParseError _event(Parser* p, ASTNode* ast) {
    ast->type = AST_EVENT;
    ast->value.tkn = prev(p);

    return PARSE_SUCCESS;
}

static ParseError _dot(Parser* p, ASTNode* ast) {
    ast->type = AST_FIELD;

    if (next(p)->type != TKN_IDENTIFIER)
        return error_node(ast, prev(p), TKN_IDENTIFIER, NULL);
    
    ast->value.tkn = prev(p);

    return PARSE_SUCCESS;
}

static ParseError _name(Parser* p, ASTNode* ast) {
    TKN* token = prev(p);
    
    ast->type = AST_NAME;
    ast->value.tkn = token;

    return PARSE_SUCCESS;
}

static ParseError _call(Parser* p, ASTNode* ast) {
    ast->type = AST_CALL;

    ast->value.params = NULL;

    if (current(p)->type == TKN_RIGHT_PAREN) {
        next(p); 
        return PARSE_SUCCESS;
    }

    jary_vec_init(ast->value.params, 5);

    do {
        jary_vec_push(ast->value.params, NODE());
        ASTNode* expr = jary_vec_last(ast->value.params);

        if (_expression(p, expr) != PARSE_SUCCESS)
            return synchronize(p, TKN_NEWLINE);

        if (jary_vec_size(ast->value.params) == 255) 
            return synchronize(p, TKN_NEWLINE);

    } while(next(p)->type == TKN_COMMA);

    if (prev(p)->type != TKN_RIGHT_PAREN) 
        return error_node(ast, prev(p), TKN_RIGHT_PAREN, NULL);
        
    return PARSE_SUCCESS;
}

static ParseError _variable_section(Parser* p, ASTNode* ast) {
    if (next(p)->type != TKN_COLON)
        return error_node(ast, prev(p), TKN_COLON, NULL);

    if (next(p)->type != TKN_NEWLINE)
        return error_node(ast, prev(p), TKN_NEWLINE, NULL);

    jary_vec_init(ast->value.vars, 10);

    while (current(p)->type == TKN_IDENTIFIER) {
        jary_vec_push(ast->value.vars, NODE());
        ASTNode* var = jary_vec_last(ast->value.vars);

        if (peek(p)->type != TKN_EQUAL) {
            next(p);
            return error_node(ast, next(p), TKN_EQUAL, NULL);
        }
            
        peek(p)->type = TKN_ASSIGN;

        if (_expression(p, var) != PARSE_SUCCESS)
            return synchronize(p, TKN_NEWLINE);
        

        if (next(p)->type != TKN_NEWLINE)
            return error_node(ast, prev(p), TKN_NEWLINE, NULL);
    }

    return PARSE_SUCCESS;
}

static ParseError _input_section(Parser* p, ASTNode* ast) {
    ast->type = AST_INPUT;

    return _variable_section(p, ast);
}

static ParseError _declare_rule(Parser* p, ASTNode* ast) {
    ast->type = AST_RULE;

    TKN* name = next(p);

    if(name->type != TKN_IDENTIFIER)
        return error_node(ast, name, TKN_IDENTIFIER, NULL);

    if (next(p)->type != TKN_LEFT_BRACE)
        return error_node(ast, prev(p), TKN_LEFT_BRACE, NULL);

    if (next(p)->type != TKN_NEWLINE)
        return error_node(ast, prev(p), TKN_NEWLINE, NULL);

    ASTNode* sections;
    jary_vec_init(sections, 10);
    ast->value.sections = sections;
    ast->left = jary_alloc(sizeof(ASTNode));
    ast->left->type = AST_NAME;
    ast->left->value.tkn = name;

    while (!ended(p) && current(p)->type != TKN_RIGHT_BRACE) {
        jary_vec_push(sections, NODE());
        ASTNode* sect = jary_vec_last(sections);
        
        switch (next(p)->type) {
        case TKN_INPUT:
            if (_input_section(p, sect) != PARSE_SUCCESS)        
                return syncsection(p);                          
            break;
        case TKN_MATCH:
            break;
        case TKN_TARGET:
            break;
        case TKN_CONDITION:
            break;
        default:
            return error_node(ast, prev(p), TKN_NONE, MSG_NOT_A_INPUT_SECTION);
        }
    }

    if (next(p)->type != TKN_RIGHT_BRACE)
        return error_node(ast, prev(p), TKN_RIGHT_BRACE, NULL);

    return PARSE_SUCCESS;
}

static ParseError _declaration(Parser* p, ASTNode* ast) {
    switch (next(p)->type) {
    case TKN_RULE: 
        if(_declare_rule(p, ast) != PARSE_SUCCESS)
            return synchronize(p, TKN_RIGHT_BRACE); 
        break;
    default:
        return error_node(ast, prev(p), TKN_NONE, MSG_NOT_A_DECLARATION);
    }

    return PARSE_SUCCESS;
}

static ParseError _entry(Parser* p, ASTNode* ast) {
    ast->type = AST_ROOT;

    ASTNode* decls; 
    jary_vec_init(decls, 10);
    ast->value.decls = decls;

    while (!ended(p)) {
        jary_vec_push(decls, NODE());
        ASTNode* decl = jary_vec_last(decls);

        if(_declaration(p, decl) != PARSE_SUCCESS)
            return synchronize(p, TKN_RIGHT_BRACE); 
    }

    return PARSE_SUCCESS;
}

static ParseRule rules[] = {
    [TKN_ERR]           = {NULL,     NULL,   PREC_NONE},

    [TKN_LEFT_PAREN]    = {NULL,     _call,  PREC_CALL},
    [TKN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TKN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TKN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TKN_DOT]           = {NULL,     _dot,   PREC_CALL},
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

    [TKN_ASSIGN]        = {NULL,  _binary,   PREC_ASSIGNMENT},
    [TKN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TKN_LESSTHAN]      = {NULL,     NULL,   PREC_NONE}, 
    [TKN_GREATERTHAN]   = {NULL,     NULL,   PREC_NONE},
    [TKN_AND]           = {NULL,     NULL,   PREC_NONE},
    [TKN_OR]            = {NULL,     NULL,   PREC_NONE},
    [TKN_ANY]           = {NULL,     NULL,   PREC_NONE},
    [TKN_ALL]           = {NULL,     NULL,   PREC_NONE},

    [TKN_REGEXP]        = {NULL,     NULL,   PREC_NONE},
    [TKN_STRING]        = {_literal, NULL,   PREC_PRIMARY},
    [TKN_NUMBER]        = {_literal, NULL,   PREC_PRIMARY},
    [TKN_FALSE]         = {_literal, NULL,   PREC_PRIMARY},
    [TKN_TRUE]          = {_literal, NULL,   PREC_PRIMARY},

    [TKN_IDENTIFIER]    = {_name,    NULL,   PREC_NONE},
    [TKN_PVAR]          = {_event,    NULL,  PREC_NONE},

    [TKN_CUSTOM]        = {NULL,     NULL,   PREC_NONE}, 
    [TKN_EOF]           = {NULL,     NULL,   PREC_NONE}, 
};

static ParseRule* get_rule(TknType type) {
    return &rules[type];
}

void parse_tokens(Parser* p, ASTNode* ast, TKN* tkns, size_t length) {
    jary_assert(p != NULL);
    jary_assert(ast != NULL);
    jary_assert(length > 0);
    
    p->tkns = tkns;
    p->tknsz = length;
    p->idx = 0;

    _entry(p, ast); 
}