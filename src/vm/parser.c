#include <string.h>
#include <stdio.h>

#include "parser.h"
#include "scanner.h"
#include "memory.h"
#include "error.h"
#include "fnv.h"

#define RETURN_PANIC(__res)                                                         \
    do {                                                                            \
        if ((__res) == ERR_PARSE_PANIC)                                             \
            return ERR_PARSE_PANIC;                                                 \
    } while(0)

#define addtkn(__p, __tkn)                                                          \
    do {                                                                            \
        if ((__p)->tknsz == 0) {                                                    \
            size_t __new_sz = (sizeof(*(__p)->tkns) * 10);                          \
            (__p)->tkns = jary_realloc((__p)->tkns, __new_sz);                      \
        }                                                                           \
        else if (((__p)->tknsz % 10) == 0) {                                        \
            size_t __new_sz = (sizeof(*(__p)->tkns) * ((__p)->tknsz + 10));         \
            (__p)->tkns = jary_realloc((__p)->tkns, __new_sz);                      \
        }                                                                           \
        (__p)->tkns[(__p)->tknsz++] = (__tkn);                                      \
    } while (0)

#define addchild(__ast, __child)                                                    \
    do {                                                                            \
        if ((__ast)->degree == 0) {                                                 \
            size_t __new_sz = (sizeof(*(__ast)->child) * 10);                       \
            (__ast)->child = jary_realloc((__ast)->child, __new_sz);                \
        }                                                                           \
        else if (((__ast)->degree % 10) == 0) {                                     \
            size_t __new_sz = (sizeof(*(__ast)->child) * ((__ast)->degree + 10));   \
            (__ast)->child = jary_realloc((__ast)->child, __new_sz);                \
        }                                                                           \
        (__ast)->child[(__ast)->degree++] = (__child);                              \
    } while (0)

#define NODE() ((ASTNode){0})

#define MAX(__a, __b) ((__b > __a) ? __b : __a)

typedef struct Parser
{
    Tkn **tkns;
    size_t tknsz;
    Scanner *sc;

    size_t idx;
    size_t depth;
} Parser;

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
error_node(ASTNode *ast, Tkn *nextkn, Tkn *tkn, ASTError **errs, size_t *errsz, const char *msg)
{
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

    ASTError errnode = {
        .line = tkn->line,
        .offset = tkn->offset,
        .lexeme = lexeme,
        .linestr = linestr,
        .msg = (msg != NULL) ? strdup(msg) : NULL};


    if (*errsz == 0) {
        size_t newcap = sizeof(**errs) * 10;
        *errs = jary_realloc(*errs, newcap);
    } else if ((*errsz % 10) == 0) {
        size_t newcap = sizeof(**errs) * (*errsz + 10);
        *errs = jary_realloc(*errs, newcap);
    }

    (*errs)[(*errsz)++] = errnode;

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
    bool tknend = p->idx + 1 >= p->tknsz;

    return scend && tknend;
}

static Tkn *back(Parser *p)
{
    jary_assert(p->idx-1 >= 0);

    return p->tkns[p->idx--];
}

static Tkn *prev(Parser *p)
{
    jary_assert(p->idx-1 >= 0);

    return p->tkns[p->idx-1];
}

// fill token with current and advance
static Tkn *next(Parser *p)
{
    if (p->idx + 1 >= p->tknsz)
    {
        jary_assert(!ended(p));

        Tkn *tkn = jary_alloc(sizeof *tkn);

        scan_token(p->sc, tkn);
        addtkn(p, tkn);
    }

    return p->tkns[p->idx++];
}

// fill token with current and advance
static Tkn *current(Parser *p)
{
    jary_assert(p->idx < p->tknsz);

    return p->tkns[p->idx];
}

static ParseError synclist(Parser *p, size_t *size, size_t newsize)
{
    if (size != NULL)
        *size = newsize;

    Tkn *tkn;

    do
    {
        if (ended(p))
            return ERR_PARSE_PANIC;

        tkn = next(p);
    } while (
        tkn->type != TKN_RIGHT_BRACE &&
        tkn->type != TKN_NEWLINE &&
        !is_decl(tkn->type) &&
        !is_section(tkn->type));

    back(p);

    return PARSE_SUCCESS;
}

static ParseError syncsection(Parser *p, size_t *size, size_t newsize)
{
    if (size != NULL) 
        *size = newsize;

    Tkn *tkn;

    do {
        if (ended(p))
            return ERR_PARSE_PANIC;

        tkn = next(p);
    } while (
        tkn->type != TKN_RIGHT_BRACE &&
        !is_decl(tkn->type) &&
        !is_section(tkn->type));

    back(p);

    return PARSE_SUCCESS;
}

static ParseError syncdecl(Parser *p, size_t *size, size_t newsize)
{
    if (size != NULL) 
        *size = newsize;

    Tkn *tkn;

    do {
        if (ended(p))
            return ERR_PARSE_PANIC;

        tkn = next(p);
    } while (
        tkn->type != TKN_RIGHT_BRACE &&
        !is_decl(tkn->type));

    back(p);

    return PARSE_SUCCESS;
}

static ParseRule* get_rule(TknType type);

static ParseError _err(Parser* p, ASTNode* ast, ASTMetadata* m) {
    Tkn *token = prev(p);

    return error_node(ast, current(p), token, &m->errors, &m->errsz, "error: unrecognized token");
}

static ParseError _err_str(Parser* p, ASTNode* ast, ASTMetadata* m) {
    Tkn *token = prev(p);

    return error_node(ast, current(p), token, &m->errors, &m->errsz, "error: unterminated string");
}

static ParseError _literal(Parser* p, ASTNode* ast, ASTMetadata* m) {
    Tkn *token = prev(p);

    ast->type = AST_LITERAL;
    ast->tkn = token;

    return PARSE_SUCCESS;
}

static ParseError _name(Parser* p, ASTNode* ast, ASTMetadata* m) {
    Tkn *token = prev(p);

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

    if (next(p)->type != TKN_IDENTIFIER)
        return error_node(ast, current(p), back(p), &m->errors, &m->errsz, NULL);

    ASTNode name = NODE();
    if(_name(p, &name, m) != PARSE_SUCCESS)
        return error_node(ast, current(p), back(p), &m->errors, &m->errsz, "Todo: handle this if needed");

    p->depth++;
    name.id = m->size++;
    addchild(ast, name);

    return PARSE_SUCCESS;
}

static ParseError _call(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_CALL;
    ast->tkn = prev(p);

    if (current(p)->type == TKN_RIGHT_PAREN) {
        next(p); 
        return PARSE_SUCCESS;
    }

    do
    {
        ASTNode expr = NODE();

        if (_expression(p, &expr, m) != PARSE_SUCCESS)
        {
            goto PANIC;
        }

        addchild(ast, expr);
    } while (current(p)->type == TKN_COMMA);

    if (next(p)->type != TKN_RIGHT_PAREN)
    {
        error_node(ast, current(p), back(p), &m->errors, &m->errsz, NULL);
        goto PANIC;
    }

    return PARSE_SUCCESS;

PANIC:
    return ERR_PARSE_PANIC;
}

static ParseError _binary(Parser *p, ASTNode *ast, ASTMetadata *m)
{
    Tkn *optkn = prev(p);
    ParseRule* oprule = get_rule(optkn->type);

    ast->tkn = optkn;
    ast->type = AST_BINARY;
    ASTNode expr = NODE();

    RETURN_PANIC(_precedence(p, &expr, m, oprule->precedence + 1));

    addchild(ast, expr);

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
        addchild(&led, nud);

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
        return error_node(sect, current(p), back(p), &m->errors, &m->errsz, "error: unrecognized section");

    if (next(p)->type != TKN_COLON)
        return error_node(sect, current(p), back(p), &m->errors, &m->errsz, "error: expected ':' ");

    if (next(p)->type != TKN_NEWLINE)
        return error_node(sect, current(p), back(p), &m->errors, &m->errsz, "error: expected '\n' ");

    if (current(p)->type == TKN_RIGHT_BRACE) {
        return PARSE_SUCCESS;
    }

    for (Tkn *tkn = current(p);
         tkn->type != TKN_RIGHT_BRACE &&
         !ended(p) &&
         !is_decl(tkn->type) &&
         !is_section(tkn->type);
         tkn = current(p))
    {
        size_t size = m->size;
        ASTNode expr = NODE();

        p->depth = 2;

        if (_list(p, &expr, m) != PARSE_SUCCESS) {
            RETURN_PANIC(synclist(p, &m->size, size));
        }

        Tkn *nextkn = next(p);

        if (
            nextkn->type != TKN_NEWLINE && 
            nextkn->type != TKN_RIGHT_BRACE) 
        {
            ast_free(&expr);
            back(p);
            return error_node(sect, prev(p), sect->tkn, &m->errors, &m->errsz, "error: unterminated section");
        }

        m->depth = MAX(m->depth, p->depth);
        addchild(sect, expr);
    }

    p->depth = 2;

    return PARSE_SUCCESS;
}

static ParseError _declaration(Parser *p, ASTNode *decl, ASTMetadata *m)
{
    Tkn *decltkn = next(p);

    decl->tkn = decltkn;
    decl->type = AST_DECL;

    if (!is_decl(decltkn->type))
    {
        return error_node(decl, back(p), decltkn, &m->errors, &m->errsz, "error: invalid declaration");
    }

    Tkn *nametkn = next(p);

    if (nametkn->type != TKN_IDENTIFIER)
        return error_node(decl, back(p), decltkn, &m->errors, &m->errsz, "error: expected identifier for declaration");

    Tkn *leftbrace = next(p);

    if (leftbrace->type != TKN_LEFT_BRACE)
        return error_node(decl, back(p), leftbrace, &m->errors, &m->errsz, "error:");

    Tkn *newln = next(p);

    if (newln->type != TKN_NEWLINE)
        return error_node(decl, back(p), newln, &m->errors, &m->errsz, NULL);

    ASTNode nodename = {
        .type = AST_NAME,
        .tkn = nametkn,
        .id = m->size++};

    // old maxdepth
    size_t maxdepth = m->depth;

    addchild(decl, nodename);

    while (
        !ended(p) &&
        current(p)->type != TKN_RIGHT_BRACE &&
        !is_decl(current(p)->type))
    {
        ASTNode sect = NODE();
        size_t graphsz = sect.id = m->size++;

        p->depth = 1;

        if (_section(p, &sect, m) != PARSE_SUCCESS) {
            syncsection(p, &m->size, graphsz);
        } else {
            m->depth = MAX(m->depth, p->depth);
        }

        addchild(decl, sect);
    }

    if (ended(p) || next(p)->type != TKN_RIGHT_BRACE) {
        m->depth = maxdepth;
        back(p);
        error_node(decl, prev(p), decltkn, &m->errors, &m->errsz, "error: declaration unterminated");
        return ERR_PARSE_PANIC;
    }

    return PARSE_SUCCESS;
}

static void _entry(Parser *p, ASTNode *root, ASTMetadata *m)
{
    root->type = AST_ROOT;
    root->id = m->size++;
    root->tkn = NULL;

    while (!ended(p)) {
        ASTNode decl = NODE();
        size_t size = decl.id = m->size++;

        p->depth = 0;

        if (_declaration(p, &decl, m) != PARSE_SUCCESS) {
            if (syncdecl(p, &m->size, size))
                return;
        }
        else
        {
            addchild(root, decl);

            while (current(p)->type == TKN_NEWLINE)
                next(p);

            m->depth = MAX(m->depth, p->depth);
        }
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

void jary_parse(ASTNode *ast, ASTMetadata *m, const char *src, size_t length)
{
    jary_assert(ast != NULL);
    jary_assert(m != NULL);
    jary_assert(length > 0);

    Scanner scan = {.base = NULL};
    char* dupsrc = strndup(src, length);
    scan_source(&scan, dupsrc, length);

    Parser *p = jary_alloc(sizeof *p);
    p->tkns = NULL;
    p->tknsz = 0;
    p->sc = &scan;
    p->idx = 0;
    p->depth = 0;
    Tkn *tkn = jary_alloc(sizeof *tkn);
    scan_token(&scan, tkn);
    addtkn(p, tkn);

    ast->degree = 0;
    ast->child = NULL;
    ast->id = 0;
    ast->tkn = NULL;
    ast->type = AST_ROOT;

    m->src = dupsrc;
    m->tkns = NULL;
    m->tknsz = 0;
    m->errors = NULL;
    m->errsz = 0;
    m->size = 0;
    m->depth = 0;

    _entry(p, ast, m); 

    m->tkns = p->tkns;
    m->tknsz = p->tknsz;
}