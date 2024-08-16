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
#define MSG_NOT_A_SECTION "invalid section"
#define MSG_NOT_A_DECLARATION "invalid declaration"


typedef enum ParseError {
    PARSE_SUCCESS = 0,

    ERR_PARSE_PANIC,
} ParseError;

typedef enum Precedence {
  PREC_NONE,
  PREC_NAME,
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

static ParseError _precedence(Parser* p, ASTNode* ast, ASTMetadata* m, Precedence rbp);
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

static bool is_section(TknType type) {
    switch (type)
    {
    case TKN_INPUT:
    case TKN_MATCH:
    case TKN_TARGET:
    case TKN_CONDITION:
        return true;
    }

    return false;
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

// fill token with current and advance
static TKN* current(Parser* p) {
    jary_assert(p->idx < jary_vec_size(p->tkns));

    return &p->tkns[p->idx];    
}

static void synchronize(Parser* p, size_t* size, size_t newsize, TknType until) {
    if (size != NULL) 
        *size = newsize;

    while (!ended(p) && next(p)->type != until);
}

static void syncsection(Parser* p, size_t* size, size_t newsize) {
    if (size != NULL) 
        *size = newsize;

    for(TKN* tkn = next(p); 
            tkn->type != TKN_RIGHT_BRACE    &&
            !ended(p)                       && 
            !is_section(tkn->type)          ; 
        tkn = next(p));
}

static ParseRule* get_rule(TknType type);

static ParseError _err(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* token = prev(p);

    return error_node(ast, token, TKN_NONE, &m->errors, MSG_NOT_A_TKN);
}

static void block_add_def(size_t block, ASTMetadata* m, TKN* tkn) {
    BasicBlock* dblock = find_basic_block(&m->bbkey, &m->bbval, block);
    
    jary_assert(dblock != NULL);

    jary_vec_push(dblock->def, *tkn);
}

static void block_add_use(size_t block, ASTMetadata* m, TKN* tkn) {
    BasicBlock* dblock = find_basic_block(&m->bbkey, &m->bbval, block);
    
    jary_assert(dblock != NULL);

    jary_vec_push(dblock->use, *tkn);
}

static ParseError _literal(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* token = prev(p);

    ast->type = AST_LITERAL;
    ast->tkn = token;

    block_add_def(p->block, m, token);

    return PARSE_SUCCESS;
}

static ParseError _name(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* token = prev(p);

    block_add_use(p->block, m, token);

    ast->type = AST_NAME;
    ast->tkn = token;

    return PARSE_SUCCESS;
}

static ParseError _unary(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_UNARY;
    ast->tkn = prev(p);

    RETURN_PANIC(_precedence(p, ast, m, PREC_UNARY));

    return PARSE_SUCCESS;
}

static ParseError _event(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_EVENT;
    ast->tkn = prev(p);
    
    jary_vec_init(ast->child, 1);

    ASTNode expr = NODE();
    RETURN_PANIC(_precedence(p, &expr, m, PREC_PRIMARY));

    jary_vec_push(ast->child, expr);

    return PARSE_SUCCESS;
}

static ParseError _call(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_CALL;

    if (current(p)->type == TKN_RIGHT_PAREN) {
        next(p); 
        return PARSE_SUCCESS;
    }

    size_t oldblock = p->block;
    size_t block = p->block = ast->number;

    BasicBlock dblock = { .def = NULL };
    jary_vec_init(dblock.def, 10);
    jary_vec_init(dblock.use, 10);

    jary_vec_push(m->bbkey, block);
    jary_vec_push(m->bbval, dblock);

    do {
        ASTNode expr = NODE();

        if (_expression(p, &expr, m) != PARSE_SUCCESS) {
            goto PANIC;
        }

        p->block = oldblock;

        jary_vec_push(ast->child, expr);
    } while(current(p)->type == TKN_COMMA);

    if (next(p)->type != TKN_RIGHT_PAREN) {
        error_node(ast, back(p), TKN_RIGHT_PAREN, &m->errors, NULL);
        goto PANIC;
    }
        
    return PARSE_SUCCESS;

PANIC:
    jary_vec_pop(m->bbkey);
    jary_vec_pop(m->bbval);
    return ERR_PARSE_PANIC;
}

static ParseError _binary(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* optkn = prev(p);
    ParseRule* oprule = get_rule(optkn->type);

    ast->tkn = optkn;
    ast->type = AST_BINARY;
    ASTNode expr = NODE();

    RETURN_PANIC(_precedence(p, &expr, m, oprule->precedence + 1));

    jary_vec_push(ast->child, expr);

    return PARSE_SUCCESS;
}

static ParseError _precedence(Parser* p, ASTNode* expr, ASTMetadata* m,  Precedence rbp) {
    ParseRule* prefixrule = get_rule(next(p)->type);
    ParseFn prefixfn = prefixrule->prefix;
    
    jary_assert(prefixfn != NULL);

    ASTNode nud = NODE();
    nud.number = m->size++;
    RETURN_PANIC(prefixfn(p, &nud, m));
    *expr = nud;

    Precedence nextprec = get_rule(current(p)->type)->precedence;

    while (rbp < nextprec) {
        ASTNode led = NODE();
        led.number = m->size++;
        jary_vec_init(led.child, 10);
        jary_vec_push(led.child, *expr);
        
        ParseFn infixfn = get_rule(next(p)->type)->infix;
        RETURN_PANIC(infixfn(p, &led, m));
        nextprec = get_rule(current(p)->type)->precedence;
        *expr = led;
    }
    
    return PARSE_SUCCESS;
}

static ParseError _expression(Parser* p, ASTNode* ast, ASTMetadata* m) {
    return _precedence(p, ast, m, PREC_NAME);
}

static ParseError _list(Parser* p, ASTNode* sect, ASTMetadata* m) {
    ASTNode expr = NODE();

    RETURN_PANIC(_expression(p, &expr, m));

    jary_vec_push(sect->child, expr);

    return PARSE_SUCCESS;
}

static ParseError _section(Parser* p, ASTNode* sect, ASTMetadata* m) {
    sect->type = AST_SECTION;

    if (!is_section(next(p)->type))
        return error_node(sect, back(p), TKN_NONE, &m->errors, MSG_NOT_A_SECTION);

    if (next(p)->type != TKN_COLON)
        return error_node(sect, back(p), TKN_COLON, &m->errors, NULL);

    if (next(p)->type != TKN_NEWLINE)
        return error_node(sect, back(p), TKN_NEWLINE, &m->errors, NULL);
    
    size_t block = p->block = sect->number;
    BasicBlock dblock = { .def = NULL };
    jary_vec_init(dblock.def, 10);
    jary_vec_init(dblock.use, 10);

    jary_vec_push(m->bbkey, block);
    jary_vec_push(m->bbval, dblock);

    for (TKN* tkn = current(p);
            tkn->type != TKN_RIGHT_BRACE        &&
            tkn->type != TKN_NEWLINE            &&
            !ended(p)                           &&
            !is_section(tkn->type)              ; 
        tkn = current(p)
    ) {
        size_t size = m->size;

        if (_list(p, sect, m) != PARSE_SUCCESS) {
            synchronize(p, &m->size, size, TKN_NEWLINE);
            continue;
        }

        if (next(p)->type != TKN_NEWLINE) {
            error_node(jary_vec_last(sect->child), back(p), TKN_NEWLINE, &m->errors, NULL);
            continue;
        }
    }

    return PARSE_SUCCESS;
}

static ParseError _declaration(Parser* p, ASTNode* decl, ASTMetadata* m) {
    TKN* decltkn = next(p);

    decl->tkn = decltkn;
    decl->number = m->size++;
    decl->type = AST_DECL;

    switch (decltkn->type) {
    case TKN_RULE:
        break;
    default:
        return error_node(decl, back(p), TKN_NONE, &m->errors, MSG_NOT_A_DECLARATION);
    }

    TKN* nametkn = next(p);

    if(nametkn->type != TKN_IDENTIFIER)
        return error_node(decl, back(p), TKN_IDENTIFIER, &m->errors, NULL);

    if (next(p)->type != TKN_LEFT_BRACE)
        return error_node(decl, back(p), TKN_LEFT_BRACE, &m->errors, NULL);

    if (next(p)->type != TKN_NEWLINE)
        return error_node(decl, back(p), TKN_NEWLINE, &m->errors, NULL);

    ASTNode nodename = {
        .type = AST_NAME, 
        .tkn = nametkn,
        .number = m->size++
    };

    jary_vec_init(decl->child, 10);
    jary_vec_push(decl->child, nodename);

    while (!ended(p) && current(p)->type != TKN_RIGHT_BRACE) {
        ASTNode sect = NODE();
        size_t graphsz = sect.number = m->size++;
        jary_vec_init(sect.child, 10);

        if (_section(p, &sect, m) != PARSE_SUCCESS) {
            syncsection(p, &m->size, graphsz);
        }

        jary_vec_push(decl->child, sect);
    }

    if (next(p)->type != TKN_RIGHT_BRACE) {
        return error_node(decl, back(p), TKN_RIGHT_BRACE, &m->errors, NULL);
    }

    return PARSE_SUCCESS;
}

static void _entry(Parser* p, ASTNode* ast, ASTMetadata* m) {
    jary_vec_init(ast->child, 10);
    ast->type = AST_ROOT;
    ast->number = m->size++;

    size_t block = p->block = ast->number;
    BasicBlock dblock = { .def = NULL };
    jary_vec_init(dblock.def, 10);
    jary_vec_init(dblock.use, 10);

    jary_vec_push(m->bbkey, block);
    jary_vec_push(m->bbval, dblock);

    while (!ended(p)) {
        ASTNode decl = NODE();
        size_t size = m->size;

        if (_declaration(p, &decl, m) != PARSE_SUCCESS) 
            synchronize(p, &m->size, size, TKN_RIGHT_BRACE);
        else
            jary_vec_push(ast->child, decl);

        p->block = ast->number;
    }
}

static ParseRule rules[] = {
    [TKN_ERR]           = {_err,     NULL,   PREC_NONE},

    [TKN_LEFT_PAREN]    = {NULL,     _call,  PREC_CALL},
    [TKN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TKN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TKN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TKN_DOT]           = {NULL,     NULL,   PREC_CALL},
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

    [TKN_EQUAL]         = {NULL,  _binary,   PREC_EQUALITY},
    [TKN_LESSTHAN]      = {NULL,  _binary,   PREC_COMPARISON}, 
    [TKN_GREATERTHAN]   = {NULL,  _binary,   PREC_COMPARISON},
    [TKN_AND]           = {NULL,  _binary,   PREC_AND },
    [TKN_OR]            = {NULL,  _binary,   PREC_OR  },
    [TKN_ANY]           = {NULL,     NULL,   PREC_NONE},
    [TKN_ALL]           = {NULL,     NULL,   PREC_NONE},

    [TKN_REGEXP]        = {NULL,     NULL,   PREC_NONE},
    [TKN_STRING]        = {_literal, NULL,   PREC_NONE},
    [TKN_NUMBER]        = {_literal, NULL,   PREC_NONE},
    [TKN_FALSE]         = {_literal, NULL,   PREC_NONE},
    [TKN_TRUE]          = {_literal, NULL,   PREC_NONE},

    [TKN_IDENTIFIER]    = {_name,    NULL,   PREC_NAME},
    [TKN_DOLLAR]        = {_event,   NULL,   PREC_NONE},

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

    // -1 represent not in a block
    p->block = -1;

    jary_vec_init(m->errors, 10);
    jary_vec_init(m->bbkey, 10);
    jary_vec_init(m->bbval, 10);
    m->size = 0;

    _entry(p, ast, m); 

    m->tkns = p->tkns;
    m->tknsz = jary_vec_size(p->tkns);
    m->errsz = jary_vec_size(m->errors);
    m->bbsz = jary_vec_size(m->bbkey);

    jary_assert(jary_vec_size(m->bbkey) == jary_vec_size(m->bbval));
}