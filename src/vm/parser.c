#include <string.h>
#include <stdio.h>

#include "vector.h"
#include "parser.h"
#include "fnv.h"

#define RETURN_PANIC(__res)                                                         \
    do {                                                                            \
        if ((__res) == ERR_PARSE_PANIC)                                             \
            return ERR_PARSE_PANIC;                                                 \
    } while(0)

#define NODE() ((ASTNode){0})

#define MAX(__a, __b) ((__b > __a) ? __b : __a)


typedef enum ParseError {
    PARSE_SUCCESS = 0,

    ERR_PARSE_PANIC,
} ParseError;

typedef enum Precedence {
  PREC_NONE,
  PREC_ASSIGNMENT,
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
error_node(ASTNode* ast, TKN* nextkn, TKN* tkn, ASTError** errs, const char* msg) {
    if (ast != NULL) {
        ast_free(ast);
    }
    
    char* lexeme = jary_alloc(tkn_lexeme_size(tkn));
    // +1 for '\0'
    size_t linestrsz = ((size_t)(nextkn->start - tkn->start)); 
    linestrsz += tkn->offset;
    char* linestr = jary_alloc(linestrsz); 
    tkn_lexeme(tkn, lexeme, tkn_lexeme_size(tkn));
    memcpy(linestr, tkn->linestart, linestrsz);
    linestr[linestrsz-1] = '\0';

    ASTError err = {
        .line = tkn->line,
        .offset = tkn->offset,
        .lexeme = lexeme,
        .linestr = linestr,
        .msg = (msg != NULL) ? strdup(msg) : NULL
    };

    vecpush(*errs, err);

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

static bool is_decl(TknType type) {
    switch (type) {
    case TKN_RULE:
        return true;
    case TKN_IMPORT:
        return true;
    case TKN_INGRESS:
        return true;
    }

    return false;
}

static bool ended(Parser* p) {
    bool scend = scan_ended(p->sc);
    bool tknend = p->idx + 1 >= vecsize(p->tkns);

    return scend && tknend;
}

static TKN* back(Parser* p) {
    jary_assert(p->idx-1 >= 0);

    return p->tkns[p->idx--];   
}

static TKN* prev(Parser* p) {
    jary_assert(p->idx-1 >= 0);

    return p->tkns[p->idx-1];   
}

// fill token with current and advance
static TKN* next(Parser* p) {
    if (p->idx + 1 >= vecsize(p->tkns)) {
        jary_assert(!ended(p));

        TKN* tkn = jary_alloc(sizeof *tkn);

        scan_token(p->sc, tkn);

        vecpush(p->tkns, tkn);
    }

    return p->tkns[p->idx++];  
}

// fill token with current and advance
static TKN* current(Parser* p) {
    jary_assert(p->idx < vecsize(p->tkns));

    return p->tkns[p->idx];    
}

static ParseError synchronize(Parser* p, size_t* size, size_t newsize, TknType until) {
    if (size != NULL) 
        *size = newsize;

    do {
        if (ended(p))
            return ERR_PARSE_PANIC;
    } while (next(p)->type != until);
    
    back(p);
    return PARSE_SUCCESS;
}

static ParseError synclist(Parser* p, size_t* size, size_t newsize) {
    if (size != NULL) 
        *size = newsize;

    TKN* tkn;

    do {
        if (ended(p))
            return ERR_PARSE_PANIC;

        tkn = next(p);
    } while (
        tkn->type != TKN_RIGHT_BRACE         &&
        tkn->type != TKN_NEWLINE             &&
        !is_decl(tkn->type)                  &&
        !is_section(tkn->type)               
    );

    back(p);

    return PARSE_SUCCESS;    
}

static ParseError syncsection(Parser* p, size_t* size, size_t newsize) {
    if (size != NULL) 
        *size = newsize;

    TKN* tkn;

    do {
        if (ended(p))
            return ERR_PARSE_PANIC;

        tkn = next(p);
    } while (
        tkn->type != TKN_RIGHT_BRACE            &&
        !is_decl(tkn->type)                     &&
        !is_section(tkn->type)               
    );

    back(p);

    return PARSE_SUCCESS;
}

static ParseRule* get_rule(TknType type);

static ParseError _err(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* token = prev(p);

    return error_node(ast, current(p), token, &m->errors, "error: unrecognized token");
}

static ParseError _err_str(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* token = prev(p);

    return error_node(ast, current(p), token, &m->errors, "error: unterminated string");
}

static void block_add_def(size_t block, ASTMetadata* m, TKN* tkn) {
    BasicBlock* dblock = find_basic_block(&m->bbkey, &m->bbval, block);
    
    jary_assert(dblock != NULL);

    vecpush(dblock->def, *tkn);
}

static void block_add_use(size_t block, ASTMetadata* m, TKN* tkn) {
    BasicBlock* dblock = find_basic_block(&m->bbkey, &m->bbval, block);
    
    jary_assert(dblock != NULL);

    vecpush(dblock->use, *tkn);
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
    
    vecinit(ast->child, 1);

    if (next(p)->type != TKN_IDENTIFIER)
        return error_node(ast, current(p), back(p), &m->errors, NULL);

    ASTNode name = NODE();
    if(_name(p, &name, m) != PARSE_SUCCESS)
        return error_node(ast, current(p), back(p), &m->errors, "Todo: handle this if needed");
    
    p->depth++;
    name.id = m->size++;
    vecpush(ast->child, name);

    return PARSE_SUCCESS;
}

static ParseError _call(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_CALL;
    ast->tkn = prev(p);

    if (current(p)->type == TKN_RIGHT_PAREN) {
        next(p); 
        return PARSE_SUCCESS;
    }

    size_t oldblock = p->block;
    size_t block = p->block = ast->id;

    BasicBlock dblock = { .def = NULL };
    vecinit(dblock.def, 10);
    vecinit(dblock.use, 10);

    vecpush(m->bbkey, block);
    vecpush(m->bbval, dblock);

    do {
        ASTNode expr = NODE();

        if (_expression(p, &expr, m) != PARSE_SUCCESS) {
            goto PANIC;
        }

        p->block = oldblock;

        vecpush(ast->child, expr);
    } while(current(p)->type == TKN_COMMA);

    if (next(p)->type != TKN_RIGHT_PAREN) {
        error_node(ast, current(p), back(p), &m->errors, NULL);
        goto PANIC;
    }
        
    return PARSE_SUCCESS;

PANIC:
    vecpop(m->bbkey);
    vecpop(m->bbval);
    return ERR_PARSE_PANIC;
}

static ParseError _binary(Parser* p, ASTNode* ast, ASTMetadata* m) {
    TKN* optkn = prev(p);
    ParseRule* oprule = get_rule(optkn->type);

    ast->tkn = optkn;
    ast->type = AST_BINARY;
    ASTNode expr = NODE();

    RETURN_PANIC(_precedence(p, &expr, m, oprule->precedence + 1));

    vecpush(ast->child, expr);

    return PARSE_SUCCESS;
}

static ParseError _precedence(Parser* p, ASTNode* expr, ASTMetadata* m,  Precedence rbp) {
    ParseRule* prefixrule = get_rule(next(p)->type);
    ParseFn prefixfn = prefixrule->prefix;
    
    jary_assert(prefixfn != NULL);

    ASTNode nud = NODE();
    nud.id = m->size++;
    p->depth++;
    RETURN_PANIC(prefixfn(p, &nud, m));

    Precedence nextprec = get_rule(current(p)->type)->precedence;

    if (rbp >= nextprec) {
        *expr = nud;
        return PARSE_SUCCESS;
    }

    do {
        ASTNode led = NODE();
        led.id = m->size++;
        vecinit(led.child, 10);
        vecpush(led.child, nud);
        
        ParseFn infixfn = get_rule(next(p)->type)->infix;
        RETURN_PANIC(infixfn(p, &led, m));

        nextprec = get_rule(current(p)->type)->precedence;

        *expr = led;
    } while (rbp < nextprec);

    return PARSE_SUCCESS;
}

static ParseError _expression(Parser* p, ASTNode* ast, ASTMetadata* m) {
    return _precedence(p, ast, m, PREC_ASSIGNMENT);
}

static ParseError _list(Parser* p, ASTNode* expr, ASTMetadata* m) {
    RETURN_PANIC(_expression(p, expr, m));

    return PARSE_SUCCESS;
}

static ParseError _section(Parser* p, ASTNode* sect, ASTMetadata* m) {
    sect->type = AST_SECTION;
    sect->tkn = next(p);

    if (!is_section(sect->tkn->type))
        return error_node(sect, current(p), back(p), &m->errors, "error: unrecognized section");

    if (next(p)->type != TKN_COLON)
        return error_node(sect, current(p), back(p), &m->errors, "error: expected ':' ");

    if (next(p)->type != TKN_NEWLINE)
        return error_node(sect, current(p), back(p), &m->errors, "error: expected '\n' ");
    
    if (current(p)->type == TKN_RIGHT_BRACE) {
        return PARSE_SUCCESS;
    }
    
    size_t block = p->block = sect->id;
    BasicBlock dblock = { .def = NULL };
    vecinit(dblock.def, 10);
    vecinit(dblock.use, 10);

    vecpush(m->bbkey, block);
    vecpush(m->bbval, dblock);

    for (TKN* tkn = current(p);
            tkn->type != TKN_RIGHT_BRACE        &&
            !ended(p)                           &&
            !is_decl(tkn->type)                 &&
            !is_section(tkn->type)              ; 
        tkn = current(p)
    ) {
        size_t size = m->size;
        ASTNode expr = NODE();

        p->depth = 2;

        if (_list(p, &expr, m) != PARSE_SUCCESS) {
            RETURN_PANIC(synclist(p, &m->size, size));
        }

        TKN* nextkn = next(p);

        if (
            nextkn->type != TKN_NEWLINE && 
            nextkn->type != TKN_RIGHT_BRACE) 
        {
            ast_free(&expr);
            back(p);
            return error_node(sect, prev(p), sect->tkn, &m->errors, "error: unterminated section");
        }

        m->depth = MAX(m->depth, p->depth);
        vecpush(sect->child, expr);
    }

    p->depth = 2;

    return PARSE_SUCCESS;
}

static ParseError _declaration(Parser* p, ASTNode* decl, ASTMetadata* m) {
    TKN* decltkn = next(p);

    decl->tkn = decltkn;
    decl->id = m->size++;
    decl->type = AST_DECL;

    if (!is_decl(decltkn->type)) {
        return error_node(decl, back(p), decltkn, &m->errors, "error: invalid declaration");
    }

    TKN* nametkn = next(p);

    if(nametkn->type != TKN_IDENTIFIER)
        return error_node(decl, back(p), decltkn, &m->errors, "error: expected identifier for declaration");

    TKN* leftbrace = next(p);

    if (leftbrace->type != TKN_LEFT_BRACE)
        return error_node(decl, back(p), leftbrace, &m->errors, "error:");

    TKN* newln = next(p);

    if (newln->type != TKN_NEWLINE)
        return error_node(decl, back(p), newln, &m->errors, NULL);

    ASTNode nodename = {
        .type = AST_NAME, 
        .tkn = nametkn,
        .id = m->size++
    };

    // old maxdepth
    size_t maxdepth = m->depth;

    vecinit(decl->child, 10);
    vecpush(decl->child, nodename);

    while (
            !ended(p)                               && 
            current(p)->type != TKN_RIGHT_BRACE     &&
            !is_decl(current(p)->type)
        ) {
        ASTNode sect = NODE();
        size_t graphsz = sect.id = m->size++;
        vecinit(sect.child, 10);

        p->depth = 1;

        if (_section(p, &sect, m) != PARSE_SUCCESS) {
            syncsection(p, &m->size, graphsz);
        } else {
            m->depth = MAX(m->depth, p->depth);
        }

        vecpush(decl->child, sect);
    }

    if (ended(p) || next(p)->type != TKN_RIGHT_BRACE) {
        m->depth = maxdepth;
        back(p);
        error_node(decl, prev(p), decltkn, &m->errors, "error: declaration unterminated");
        return ERR_PARSE_PANIC;
    }

    return PARSE_SUCCESS;
}

static void _entry(Parser* p, ASTNode* ast, ASTMetadata* m) {
    vecinit(ast->child, 10);
    ast->type = AST_ROOT;
    ast->id = m->size++;
    ast->tkn = NULL;

    size_t block = p->block = ast->id;
    BasicBlock dblock = { .def = NULL };
    vecinit(dblock.def, 10);
    vecinit(dblock.use, 10);

    vecpush(m->bbkey, block);
    vecpush(m->bbval, dblock);

    while (!ended(p)) {
        ASTNode decl = NODE();
        size_t size = m->size;

        p->depth = 0;

        if (_declaration(p, &decl, m) != PARSE_SUCCESS) {
            ParseError res = synchronize(p, &m->size, size, TKN_RIGHT_BRACE);

            if (res != PARSE_SUCCESS)
                return;

            next(p); // consume right brace
        }
        else
            vecpush(ast->child, decl);
        
        while (current(p)->type == TKN_NEWLINE)
            next(p);

        p->block = ast->id;
        m->depth = MAX(m->depth, p->depth);
    }
}

static ParseRule rules[] = {
    [TKN_ERR]           = {_err,     NULL,   PREC_NONE},
    [TKN_ERR_STR]       = {_err_str, NULL,   PREC_NONE},

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

    [TKN_IDENTIFIER]    = {_name,    NULL,   PREC_NONE},
    [TKN_DOLLAR]        = {_event,   NULL,   PREC_NONE},

    [TKN_CUSTOM]        = {NULL,     NULL,   PREC_NONE}, 
    [TKN_EOF]           = {NULL,     NULL,   PREC_NONE}, 
};

static ParseRule* get_rule(TknType type) {
    return &rules[type];
}

void jary_parse(Parser* p, ASTNode* ast, ASTMetadata* m, const char* src, size_t length) {
    jary_assert(p != NULL);
    jary_assert(ast != NULL);
    jary_assert(m != NULL);
    jary_assert(length > 0);

    Scanner scan = {.base = NULL};
    char* dupsrc = strndup(src, length);
    scan_source(&scan, dupsrc, length);
    TKN* tkn = jary_alloc(sizeof *tkn);
    scan_token(&scan, tkn);
    
    vecinit(p->tkns, 20);
    vecpush(p->tkns, tkn);
    p->idx = 0;
    p->sc = &scan;

    // -1 represent not in a block
    p->block = -1;

    vecinit(m->errors, 10);
    vecinit(m->bbkey, 10);
    vecinit(m->bbval, 10);
    m->size = 0;

    _entry(p, ast, m); 

    m->tkns = p->tkns;
    m->tknsz = vecsize(p->tkns);
    m->errsz = vecsize(m->errors);
    m->bbsz = vecsize(m->bbkey);
    m->src = dupsrc;

    jary_assert(vecsize(m->bbkey) == vecsize(m->bbval));
}