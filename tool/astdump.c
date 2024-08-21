#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "parser.h"
#include "memory.h"


static char* ast2string(ASTType type) {
    char* buf = NULL;
    
    switch (type)
    {
    case AST_ROOT:
        buf = strdup("root"); break;
    case AST_DECL:
        buf = strdup("decl"); break;
    case AST_SECTION:
        buf = strdup("section"); break;
    case AST_BINARY:
        buf = strdup("binary"); break;
    case AST_CALL:
        buf = strdup("call"); break;
    case AST_UNARY:
        buf = strdup("unary"); break;
    case AST_EVENT:
        buf = strdup("event"); break;
    case AST_MEMBER:
        buf = strdup("member"); break;
    case AST_NAME:
        buf = strdup("name"); break;
    case AST_PATH:
        buf = strdup("path"); break;
    case AST_LITERAL:
        buf = strdup("literal"); break;
    default:
        buf = strdup("unknown");
    }

    return buf;
}

static void printast(ASTNode* v, ASTMetadata* m, size_t depth) {
    size_t midpoint = 2 * m->depth + 15;
    int idofs = (m->size) ? (int)log10((double) m->size) : 0;
    int col1sz = midpoint - 4; 

    char* typestr = ast2string(v->type);
    size_t typestrsz = strlen(typestr);
    size_t lexsz = lexsize(v->tkn);
    size_t printed = 0;

    if (v->type != AST_ROOT) {
        printed += printf("|");
        size_t lnsz = depth * 2 + 1;
        char depthline[lnsz];
        memset(depthline, '_', lnsz);
        depthline[lnsz-1] = '\0';
        printed += printf("%s", depthline);
    }

    if (v->type != AST_ROOT) {
        printed += printf(" ");
    }

    printed += printf("%s ", typestr);

    int diff = midpoint - printed;

    if (diff > 0) {
        char dots[diff+1];
        memset(dots, '.', diff);
        dots[diff] = '\0';
        printed += printf("%s", dots);
        printed += printf(" |");
    }

    printed += printf(" [%ld] ", v->id);

    size_t vidofs = v->id / 10;

    diff = idofs - vidofs;

    if (diff > 0) {
        char spaces[diff + 1];
        memset(spaces, ' ', diff);
        spaces[diff] = '\0';
        printed += printf("%s", spaces);
    }

    printed += printf("| ");

    if (v->tkn != NULL) {
        char lexeme[lexsz];
        lexemestr(v->tkn, lexeme, lexsz);
        printf("%s", lexeme);
    } 

    printf("\n");
        
    jary_free(typestr);

    for (size_t i = 0; i < v->degree; ++i) {
        printast(&v->child[i], m, depth + 1);
    }
}

static void dumpast(ASTNode* ast, ASTMetadata* m) {
    size_t midpoint = 2 * m->depth + 15;
    int idofs = m->size / 10;
    int col1sz = midpoint - 4; 

    printf("Tree ");
    printf("%*c ", col1sz, ' ');
    printf(" ID ");
    printf("%*c", 3 + idofs, ' ');
    printf("Token\n");

    if (m->size) {
        printast(ast, m, 0);   
    }
}

static void dumperrs(const char* path, ASTError** errors, size_t errsz) {
    for (size_t i = 0; i < errsz; ++i) {
        ASTError err = (*errors)[i];
        printf("%s:%ld:%ld %s\n", path, err.line, err.offset, err.msg);
        
        if (err.lexeme) {
            printf("%5ld |\t",  err.line);
            printf("%s\n", err.lexeme);
        }
    }
}

static size_t read_file(const char* path, char** dst)
{
    FILE* file = fopen(path, "rb");

    if(file == NULL)
    {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = jary_alloc(file_size + 1);
    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);

    if(bytes_read < file_size)
    {
        fprintf(stderr, "could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytes_read] = '\0';
    *dst = buffer;

    fclose(file);
    return file_size + 1;
}

static void run_file(const char* path)
{
    char* src;
    size_t length = read_file(path, &src);
    
    ASTNode ast;
    ASTMetadata m;

    jary_parse(&ast, &m, src, length);
    jary_free(src);

    printf( "===================" "\n"
            "| JARY AST DUMP ! |" "\n"
            "===================" "\n");
            
    printf("\n");

    dumpast(&ast, &m);

    printf("\n");
    printf("File Path     : %s\n", path);
    printf("Maximum Depth : %ld\n", m.depth);
    printf("Total nodes   : %ld\n", m.size);
    printf("ERRORS FOUND  : %ld\n", m.errsz);

    dumperrs(path, &m.errors, m.errsz);
    printf("\n");

    free_ast(&ast);
    free_ast_meta(&m);
}

int main(int argc, const char** argv)
{
    if (argc == 2)
        run_file(argv[1]);
    else 
        fprintf(stderr, "require file path");
    
    return 0;
}