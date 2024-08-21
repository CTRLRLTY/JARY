#include <string.h>
#include <stdio.h>

#include "parser.h"
#include "scanner.h"
#include "memory.h"
#include "error.h"
#include "fnv.h"

#define PASS_PANIC(__res)                                                           \
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

    // last non newline tkn
    Tkn* lastkn;

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
  PREC_EQUALITY,    // == != ~
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
error_node(ASTNode *ast, Tkn* tkn, ASTError **errs, size_t *errsz, const char *msg)
{
    size_t line = tkn->line;
    size_t lineofs = tkn->offset;
    size_t sz = lexsize(tkn);
    char* lexeme = jary_alloc(sz);
    lexemestr(tkn, lexeme, sz);

    if (ast != NULL) {
        free_ast(ast);

        ast->degree = 0;
    }

    ASTError errnode = {
        .line = line,
        .offset = lineofs,
        .lexeme = lexeme,
        .msg = strdup(msg)};


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
    case TKN_FIELDS:
        return true;
    }

    return false;
}

static bool is_decl(TknType type) {
    switch (type) {
    case TKN_RULE:
    case TKN_IMPORT:
    case TKN_INGRESS:
    case TKN_INCLUDE:
        return true;
    }

    return false;
}

static bool ended(Parser* p) {
    bool scend = scan_ended(p->sc);
    bool tknend = p->idx + 1 >= p->tknsz;

    return scend && tknend;
}

// fill token with current then advance
static Tkn *next(Parser *p)
{
    if (p->idx + 1 >= p->tknsz)
    {
        Tkn *tkn = jary_alloc(sizeof *tkn);

        scan_token(p->sc, tkn);

        switch (tkn->type)
        {
        case TKN_EOF:
        case TKN_NEWLINE:
            break;
        
        default:
            p->lastkn = tkn;
        }

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

static void skipnewline(Parser *p) {
    while (current(p)->type == TKN_NEWLINE)
        next(p);
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

    p->idx--;

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

    p->idx--;

    return PARSE_SUCCESS;
}

static ParseError syncdecl(Parser *p, size_t *size, size_t newsize)
{
    *size = newsize;

    Tkn *tkn;

    do {
        if (ended(p))
            goto SYNCED;

        tkn = next(p);
    } while (!is_decl(tkn->type));

    p->idx--;

SYNCED:
    return PARSE_SUCCESS;
}

static ParseRule* get_rule(TknType type);

static ParseError _err(Parser* p, ASTNode* ast, ASTMetadata* m) {
    return error_node(ast, ast->tkn, &m->errors, &m->errsz, "error: unrecognized token");
}

static ParseError _err_str(Parser* p, ASTNode* ast, ASTMetadata* m) {
    return error_node(ast, ast->tkn, &m->errors, &m->errsz, "error: unterminated string");
}

static ParseError _literal(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_LITERAL;

    return PARSE_SUCCESS;
}

static ParseError _name(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_NAME;

    return PARSE_SUCCESS;
}

static ParseError _unary(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_UNARY;

    ASTNode expr = NODE();
    PASS_PANIC(_precedence(p, &expr, m, PREC_UNARY));

    addchild(ast, expr);

    return PARSE_SUCCESS;
}

static ParseError _event(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_EVENT;

    if (next(p)->type != TKN_IDENTIFIER)
        return error_node(ast, ast->tkn, &m->errors, &m->errsz, "error: expected identifier");

    ASTNode name = { 
        .id = m->size++
    };
    
    _name(p, &name, m);  

    p->depth++;
    addchild(ast, name);

    return PARSE_SUCCESS;
}

static ParseError _dot(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_MEMBER;

    Tkn* nametkn = next(p);

    if (nametkn->type != TKN_IDENTIFIER)
        return error_node(ast, ast->tkn, &m->errors, &m->errsz, "error: expected identifier");

    
    ASTNode name = {
        .tkn = nametkn,
        .id = m->size++
    };
    
    _name(p, &name, m);  

    addchild(ast, name);

    return PARSE_SUCCESS;
}

static ParseError _call(Parser* p, ASTNode* ast, ASTMetadata* m) {
    ast->type = AST_CALL;

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
        error_node(ast, p->lastkn, &m->errors, &m->errsz, "error: expected ')' after");
        goto PANIC;
    }

    return PARSE_SUCCESS;

PANIC:
    return ERR_PARSE_PANIC;
}

static ParseError _binary(Parser *p, ASTNode *binop, ASTMetadata *m)
{
    binop->type = AST_BINARY;
    ParseRule* oprule = get_rule(binop->tkn->type);

    ASTNode expr = NODE();

    PASS_PANIC(_precedence(p, &expr, m, oprule->precedence));

    addchild(binop, expr);

    return PARSE_SUCCESS;
}

static ParseError _grouping(Parser *p, ASTNode *ast, ASTMetadata *m) {
    PASS_PANIC(_expression(p, ast, m));

    Tkn* rightbrace = next(p);
    if (rightbrace->type != TKN_RIGHT_PAREN) {
        return error_node(ast, rightbrace, &m->errors, &m->errsz, "error: unterminated grouping");
    }

    return PARSE_SUCCESS;
}

static ParseError _precedence(Parser* p, ASTNode* expr, ASTMetadata* m,  Precedence rbp) {
    ASTNode nud = {
        .tkn = next(p),
        .id = m->size++
    };
    
    ParseRule* prefixrule = get_rule(nud.tkn->type);
    ParseFn prefixfn = prefixrule->prefix;

    p->depth++;
    
    if (prefixfn == NULL)
        return error_node(&nud, nud.tkn, &m->errors, &m->errsz, "error: invalid expression null denotation");
    
    PASS_PANIC(prefixfn(p, &nud, m));

    *expr = nud;

    Precedence nextprec = get_rule(current(p)->type)->precedence;

    while (rbp < nextprec) {
        ASTNode led = {
            .tkn = next(p),
            .id = m->size++
        };

        addchild(&led, *expr);

        ParseFn infixfn = get_rule(led.tkn->type)->infix;
        PASS_PANIC(infixfn(p, &led, m));

        *expr = led;
        
        nextprec = get_rule(current(p)->type)->precedence;
    }

    return PARSE_SUCCESS;
}

static ParseError _expression(Parser* p, ASTNode* ast, ASTMetadata* m) {
    return _precedence(p, ast, m, PREC_ASSIGNMENT);
}

static ParseError _section(Parser* p, ASTNode* sect, ASTMetadata* m) {
    sect->type = AST_SECTION;

    if (!is_section(sect->tkn->type))
        return error_node(sect, sect->tkn, &m->errors, &m->errsz, "error: invalid section");

    if (next(p)->type != TKN_COLON)
        return error_node(sect, sect->tkn, &m->errors, &m->errsz, "error: expected ':' after");
    
    skipnewline(p);

    if (current(p)->type == TKN_RIGHT_BRACE) {
        return PARSE_SUCCESS;
    }

    for (Tkn *tkn = current(p);
         tkn->type != TKN_RIGHT_BRACE &&
         tkn->type != TKN_NEWLINE &&
         !ended(p) &&
         !is_decl(tkn->type) &&
         !is_section(tkn->type);
         tkn = current(p))
    {
        size_t oldgraphsz = m->size;
        ASTNode expr = NODE();

        p->depth = 2;

        if (_expression(p, &expr, m) != PARSE_SUCCESS) {
            synclist(p, &m->size, oldgraphsz);
            goto NEXT_LIST;
        }

        m->depth = MAX(m->depth, p->depth);
        addchild(sect, expr);

NEXT_LIST:
        skipnewline(p);
    }

    return PARSE_SUCCESS;
}

static ParseError _declaration(Parser *p, ASTNode *decl, ASTMetadata *m)
{
    decl->type = AST_DECL;
    // old maxdepth
    size_t maxdepth = m->depth;

    switch (decl->tkn->type)
    {
    case TKN_INCLUDE: {
        Tkn* strtkn = next(p);

        if (strtkn->type != TKN_STRING) 
            return error_node(decl, decl->tkn, &m->errors, &m->errsz, "error: expected string after");

        ASTNode path = {
            .type = AST_PATH,
            .tkn = strtkn,
            .id = m->size++};

        addchild(decl, path);
        p->depth = 2;

        goto SUCCESS;
    }

    case TKN_IMPORT:
    case TKN_RULE:
    case TKN_INGRESS:
        break;
    default:
        error_node(decl, decl->tkn, &m->errors, &m->errsz, "error: invalid declaration");
        goto PANIC;
    }

    Tkn *nametkn = next(p);

    if (nametkn->type != TKN_IDENTIFIER) {
        error_node(decl, decl->tkn, &m->errors, &m->errsz, "error: expected identifier after");
        goto PANIC;
    }

    ASTNode nodename = {
        .type = AST_NAME,
        .tkn = nametkn,
        .id = m->size++};

    addchild(decl, nodename);
    p->depth = 2;

    if (decl->tkn->type == TKN_IMPORT)
        goto SUCCESS;

    if (next(p)->type != TKN_LEFT_BRACE) {
        error_node(decl, nametkn, &m->errors, &m->errsz, "error: expected '{' after");
        goto PANIC;
    }
        
    skipnewline(p);

    while (
        !ended(p) &&
        current(p)->type != TKN_RIGHT_BRACE &&
        !is_decl(current(p)->type))
    {
        ASTNode sect = {
            .tkn = next(p),
            .id = m->size++,
        };

        p->depth = 2;

        if (_section(p, &sect, m) != PARSE_SUCCESS) {
            syncsection(p, &m->size, sect.id);
            goto NEXT_SECTION;
        } 
        
        addchild(decl, sect);

NEXT_SECTION:
        skipnewline(p);
    }

    if (ended(p) || next(p)->type != TKN_RIGHT_BRACE) {
        error_node(decl, p->lastkn, &m->errors, &m->errsz, "error: expected '}' after");
        goto PANIC;
    }

SUCCESS:
    m->depth = MAX(m->depth, p->depth);
    return PARSE_SUCCESS;

PANIC:
    p->depth = 1;
    m->depth = maxdepth;
    return ERR_PARSE_PANIC;
}

static void _entry(Parser *p, ASTNode *root, ASTMetadata *m)
{
    root->type = AST_ROOT;
    root->id = m->size++;
    root->tkn = NULL;

    skipnewline(p);

    while (!ended(p)) {
        ASTNode decl = {
            .tkn = next(p),
            .id = m->size++,
        };

        p->depth = 1;

        if (_declaration(p, &decl, m) != PARSE_SUCCESS) {
            if (syncdecl(p, &m->size, decl.id) != PARSE_SUCCESS) {
                goto PANIC;
            }

            goto NEXT_DECL;
        }

        addchild(root, decl);
NEXT_DECL:
        skipnewline(p);
    }

    return;

PANIC:
    error_node(root, p->lastkn, &m->errors, &m->errsz, "error: FATAL ERROR BABY! TODO: give better explanation");
}

static ParseRule rules[] = {
    [TKN_ERR]           = {_err,        NULL,   PREC_NONE},
    [TKN_ERR_STR]       = {_err_str,    NULL,   PREC_NONE},

    [TKN_LEFT_PAREN]    = {_grouping,   _call,  PREC_CALL},
    [TKN_RIGHT_PAREN]   = {NULL,        NULL,   PREC_NONE},
    [TKN_LEFT_BRACE]    = {NULL,        NULL,   PREC_NONE}, 
    [TKN_RIGHT_BRACE]   = {NULL,        NULL,   PREC_NONE},
    [TKN_DOT]           = {NULL,        _dot,   PREC_CALL},
    [TKN_COMMA]         = {NULL,        NULL,   PREC_NONE},
    [TKN_COLON]         = {NULL,        NULL,   PREC_NONE},
    [TKN_NEWLINE]       = {NULL,        NULL,   PREC_NONE},

    [TKN_TARGET]        = {NULL,        NULL,   PREC_NONE},
    [TKN_INPUT]         = {NULL,        NULL,   PREC_NONE},
    [TKN_MATCH]         = {NULL,        NULL,   PREC_NONE},
    [TKN_CONDITION]     = {NULL,        NULL,   PREC_NONE},
    [TKN_FIELDS]        = {NULL,        NULL,   PREC_NONE},
    
    [TKN_RULE]          = {NULL,        NULL,   PREC_NONE},
    [TKN_IMPORT]        = {NULL,        NULL,   PREC_NONE},
    [TKN_INCLUDE]       = {NULL,        NULL,   PREC_NONE},
    [TKN_INGRESS]       = {NULL,        NULL,   PREC_NONE},

    [TKN_PLUS]          = {NULL,     _binary,   PREC_TERM},
    [TKN_MINUS]         = {NULL,     _binary,   PREC_TERM},
    [TKN_SLASH]         = {NULL,     _binary,   PREC_FACTOR},
    [TKN_STAR]          = {NULL,     _binary,   PREC_FACTOR},

    [TKN_NOT]           = {_unary,      NULL,   PREC_NONE},
    [TKN_EQUAL]         = {NULL,     _binary,   PREC_EQUALITY},
    [TKN_TILDE]         = {NULL,     _binary,   PREC_EQUALITY},
    [TKN_LESSTHAN]      = {NULL,     _binary,   PREC_COMPARISON}, 
    [TKN_GREATERTHAN]   = {NULL,     _binary,   PREC_COMPARISON},
    [TKN_AND]           = {NULL,     _binary,   PREC_AND},
    [TKN_OR]            = {NULL,     _binary,   PREC_OR},
    [TKN_ANY]           = {NULL,        NULL,   PREC_NONE},
    [TKN_ALL]           = {NULL,        NULL,   PREC_NONE},

    [TKN_REGEXP]        = {NULL,        NULL,   PREC_NONE},
    [TKN_STRING]        = {_literal,    NULL,   PREC_NONE},
    [TKN_NUMBER]        = {_literal,    NULL,   PREC_NONE},
    [TKN_FALSE]         = {_literal,    NULL,   PREC_NONE},
    [TKN_TRUE]          = {_literal,    NULL,   PREC_NONE},

    [TKN_IDENTIFIER]    = {_name,       NULL,   PREC_NONE},
    [TKN_DOLLAR]        = {_event,      NULL,   PREC_NONE},

    [TKN_CUSTOM]        = {NULL,        NULL,   PREC_NONE}, 
    [TKN_EOF]           = {NULL,        NULL,   PREC_NONE}, 
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
    p->lastkn = NULL;
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

    jary_free(p);
}