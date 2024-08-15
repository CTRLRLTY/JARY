#include <string.h>

#include "vector.h"
#include "parser.h"
#include "fnv.h"

#define RETURN_PANIC(__res)                                                         \
    do {                                                                            \
        if ((__res) == ERR_PARSE_PANIC)                                             \
            return ERR_PARSE_PANIC;                                                 \
    } while(0)

#define NODE() ((ASTNode){0})

#define MSG_NOT_A_TKN "invalid token"
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

typedef ParseError (*ParseFn)(Parser* p, ASTNode* ast, ASTMetadata* m);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static ParseError _precedence(Parser* p, ASTNode* ast, ASTMetadata* m, Precedence prec);
static ParseError _expression(Parser* p, ASTNode* ast, ASTMetadata* m);

static ParseError 
error_node(ASTNode* ast, TKN* tkn, TknType expect, ASTError** errs, const char* msg) {
    if (ast != NULL) {
        ast_free(ast);
    }
        

    char* lexeme = jary_alloc(tkn_lexeme_size(tkn));
    tkn_lexeme(tkn, lexeme, tkn_lexeme_size(tkn));

    ASTError err = {
        .got = tkn->type,
        .expect = expect,
        .line = tkn->line,
        .offset = tkn->offset,
        .lexeme = lexeme,
        .msg = (msg != NULL) ? strdup(msg) : NULL
    };

    jary_vec_push(*errs, err);

    return ERR_PARSE_PANIC;
}

static bool ended(Parser* p) {
    bool scend = scan_ended(p->sc);
    bool tknend = p->idx + 1 >= jary_vec_size(p->tkns);

    return scend && tknend;
}

static TKN* back(Parser* p) {
    jary_assert(p->idx-1 >= 0);

    return &p->tkns[--p->idx];   
}

static TKN* prev(Parser* p) {
    jary_assert(p->idx-1 >= 0);

    return &p->tkns[p->idx-1];   
}

// fill token with current and advance
static TKN* next(Parser* p) {
    if (p->idx + 1 >= jary_vec_size(p->tkns)) {
        jary_assert(!ended(p));

        TKN tkn = {.type = TKN_ERR};

        scan_token(p->sc, &tkn);

        jary_vec_push(p->tkns, tkn);
    }

    return &p->tkns[p->idx++];  
}

static TKN* peek(Parser* p) {
    TKN* res = next(p);
    back(p);

    return &p->tkns[p->idx+1]; 
}

// fill token with current and advance
static TKN* current(Parser* p) {
    jary_assert(p->idx < jary_vec_size(p->tkns));

    return &p->tkns[p->idx];    
}

static void synchronize(Parser* p, size_t* size, size_t newsize, TknType until) {
    if (size != NULL) 
        *size = newsize;

    while (!ended(p) && current(p)->type != until)
        next(p);
}

static void syncsection(Parser* p, size_t* size, size_t newsize) {
    if (size != NULL) 
        *size = newsize;

    for(TKN* tkn = current(p);
            !ended(p)                       &&
            tkn->type != TKN_TARGET         &&
            tkn->type != TKN_INPUT          &&
            tkn->type != TKN_MATCH          &&
            tkn->type != TKN_CONDITION      &&
            tkn->type != TKN_RIGHT_BRACE    ;
        tkn = next(p));

    back(p);
}

static ParseRule* get_rule(TknType type);

static ParseError _err(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* token = prev(p);

    return error_node(ast, token, TKN_NONE, &m->errors, MSG_NOT_A_TKN);
}

static ParseError _literal(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* token = prev(p);

    ast->type = AST_LITERAL;
    ast->tkn = token;

    switch (token->type) {
    case TKN_STRING:
    case TKN_NUMBER:
    case TKN_FALSE:
    case TKN_TRUE:
    case TKN_REGEXP:
        break;
    default:
        return error_node(ast, token, TKN_NONE, &m->errors, MSG_NOT_A_LITERAL);
    }

    return PARSE_SUCCESS;
}

static ParseError _assign(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_ASSIGNMENT;
    ast->tkn = prev(p);

    return PARSE_SUCCESS;
}

static ParseError _event(Parser* p, ASTNode* ast, ASTMetadata* m) {

    return PARSE_SUCCESS;
}

static ParseError _dot(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_FIELD;

    if (next(p)->type != TKN_IDENTIFIER)
        return error_node(ast, back(p), TKN_IDENTIFIER, &m->errors, NULL);
    
    ast->tkn = prev(p);

    return PARSE_SUCCESS;
}

static ParseError _name(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* token = prev(p);
    
    ast->type = AST_NAME;
    ast->tkn = token;

    return PARSE_SUCCESS;
}

static ParseError _call(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_CALL;

    if (current(p)->type == TKN_RIGHT_PAREN) {
        next(p); 
        return PARSE_SUCCESS;
    }

    do {
        ASTNode expr = NODE();

        RETURN_PANIC(_expression(p, &expr, m));

        jary_vec_push(ast->child, expr);
    } while(current(p)->type == TKN_COMMA);

    if (next(p)->type != TKN_RIGHT_PAREN) 
        return error_node(ast, back(p), TKN_RIGHT_PAREN, &m->errors, NULL);
        
    return PARSE_SUCCESS;
}

static ParseError _binary(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* optkn = prev(p);
    ParseRule* oprule = get_rule(optkn->type);

    ast->tkn = optkn;
    ASTNode expr = NODE();

    RETURN_PANIC(_precedence(p, &expr, m, oprule->precedence + 1));

    jary_vec_push(ast->child, expr);

    switch (optkn->type) {
    case TKN_ASSIGN:
        ast->type = AST_ASSIGNMENT;
        break;
    
    default:
        return error_node(ast, optkn, TKN_NONE, &m->errors, MSG_NOT_A_BINARY);
    }

    return PARSE_SUCCESS;
}

static ParseError _precedence(Parser* p, ASTNode* ast, ASTMetadata* m,  Precedence prec) {
    ParseRule* prefixrule = get_rule(next(p)->type);
    ParseFn prefixfn = prefixrule->prefix;
    jary_assert(prefixfn != NULL);

    ast->number = m->size++;

    if (prefixrule->precedence == PREC_PRIMARY) {
        RETURN_PANIC(prefixfn(p, ast, m));
    } else {
        jary_vec_init(ast->child, 10);
        ASTNode first = NODE(); 
        first.number = m->size++;

        RETURN_PANIC(prefixfn(p, &first, m));

        jary_vec_push(ast->child, first);
    }


    while (prec <= get_rule(current(p)->type)->precedence) {
        ParseFn infixfn = get_rule(next(p)->type)->infix;

        RETURN_PANIC(infixfn(p, ast, m));
    }
    
    return PARSE_SUCCESS;
}

static ParseError _expression(Parser* p, ASTNode* ast, ASTMetadata* m) {
    return _precedence(p, ast, m, PREC_ASSIGNMENT);
}

static ParseError _variable_section(Parser* p, ASTNode* ast, ASTMetadata* m) {
    if (next(p)->type != TKN_COLON)
        return error_node(ast, back(p), TKN_COLON, &m->errors, NULL);

    if (next(p)->type != TKN_NEWLINE)
        return error_node(ast, back(p), TKN_NEWLINE, &m->errors, NULL);

    while (current(p)->type == TKN_IDENTIFIER) {
        
        ASTNode var = NODE();

        if (peek(p)->type != TKN_EQUAL) {
            next(p);
            error_node(NULL, next(p), TKN_EQUAL, &m->errors, NULL);
            synchronize(p, NULL, 0, TKN_NEWLINE);
            continue;
        }
            
        peek(p)->type = TKN_ASSIGN;

        size_t size = m->size;

        if (_expression(p, &var, m) != PARSE_SUCCESS) 
            synchronize(p, &m->size, size, TKN_NEWLINE);
        else 
            jary_vec_push(ast->child, var);
        

        if (next(p)->type != TKN_NEWLINE)
            return error_node(ast, back(p), TKN_NEWLINE, &m->errors, NULL);
    }

    return PARSE_SUCCESS;
}

static ParseError _input_section(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_INPUT;
    ast->number = m->size++;
    jary_vec_init(ast->child, 10);

    return _variable_section(p, ast, m);
}

static ParseError _declare_rule(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_RULE;
    ast->number = m->size++;

    TKN* name = next(p);

    if(name->type != TKN_IDENTIFIER)
        return error_node(ast, back(p), TKN_IDENTIFIER, &m->errors, NULL);

    if (next(p)->type != TKN_LEFT_BRACE)
        return error_node(ast, back(p), TKN_LEFT_BRACE, &m->errors, NULL);

    if (next(p)->type != TKN_NEWLINE)
        return error_node(ast, back(p), TKN_NEWLINE, &m->errors, NULL);

    jary_vec_init(ast->child, 10);

    ASTNode nodename = {
        .type = AST_NAME, 
        .tkn = name,
        .number = m->size++
    };

    jary_vec_push(ast->child, nodename);

    while (!ended(p) && current(p)->type != TKN_RIGHT_BRACE) {
        ASTNode sect = NODE();
        
        size_t graphsz = m->size;
        ParseError result = PARSE_SUCCESS;
        switch (next(p)->type) {
        case TKN_INPUT:
            result = _input_section(p, &sect, m);                
            break;
        case TKN_MATCH:
            break;
        case TKN_TARGET:
            break;
        case TKN_CONDITION:
            break;
        default:
            return error_node(ast, back(p), TKN_NONE, &m->errors, MSG_NOT_A_INPUT_SECTION);
        }

        if (result != PARSE_SUCCESS) {
            syncsection(p, &m->size, graphsz); 
            continue;
        }

        jary_vec_push(ast->child, sect);
    }

    if (next(p)->type != TKN_RIGHT_BRACE)
        return error_node(ast, back(p), TKN_RIGHT_BRACE, &m->errors, NULL);

    return PARSE_SUCCESS;
}

static ParseError _declaration(Parser* p, ASTNode* ast, ASTMetadata* m) {
    switch (next(p)->type) {
    case TKN_RULE: 
        RETURN_PANIC(_declare_rule(p, ast, m));
        break;
    default:
        return error_node(ast, back(p), TKN_NONE, &m->errors, MSG_NOT_A_DECLARATION);
    }

    return PARSE_SUCCESS;
}

static ParseError _entry(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_ROOT;
    ast->number = m->size++;

    jary_vec_init(ast->child, 10);

    while (!ended(p)) {
        ASTNode decl = NODE();
        size_t size = m->size;

        if (_declaration(p, &decl, m) != PARSE_SUCCESS) 
            synchronize(p, &m->size, size, TKN_RIGHT_BRACE);
        else
            jary_vec_push(ast->child, decl);
    }

    return PARSE_SUCCESS;
}

static ParseRule rules[] = {
    [TKN_ERR]           = {_err,     NULL,   PREC_PRIMARY},

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

void jary_parse(Parser* p, ASTNode* ast, ASTMetadata* m, char* src, size_t length) {
    jary_assert(p != NULL);
    jary_assert(ast != NULL);
    jary_assert(m != NULL);
    jary_assert(length > 0);

    Scanner scan = {.base = NULL};
    scan_source(&scan, src, length);
    TKN tkn = {.type = TKN_ERR};
    scan_token(&scan, &tkn);
    
    jary_vec_init(p->tkns, 20);
    jary_vec_push(p->tkns, tkn);
    p->idx = 0;
    p->sc = &scan;

    m->size = 0;
    jary_vec_init(m->errors, 10);

    _entry(p, ast, m); 

    m->tkns = p->tkns;
    m->tknsz = jary_vec_size(p->tkns);
    m->errsz = jary_vec_size(m->errors);
}