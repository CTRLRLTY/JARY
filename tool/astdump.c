#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

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
    case AST_NAME:
        buf = strdup("name"); break;
    case AST_LITERAL:
        buf = strdup("literal"); break;
    default:
        buf = strdup("unknown");
    }

    return buf;
}

static void printast(ASTNode* v, ASTMetadata* m, size_t depth) {
    size_t midpoint = 2 * m->depth + 15;
    int idofs = m->size / 10;
    int col1sz = midpoint - 4; 

    char* typestr = ast2string(v->type);
    size_t typestrsz = strlen(typestr);
    size_t lexsz = tkn_lexeme_size(v->tkn);
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
        tkn_lexeme(v->tkn, lexeme, lexsz);
        printf("%s", lexeme);
    } 

    printf("\n");
        
    free(typestr);

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
    printf("Token\n\n");

    printast(ast, m, 0);
}

static void dumperrs(const char* path, ASTError** errors, size_t errsz) {
    if (errsz) {
        printf("ERRORS FOUND: %ld\n", errsz);

        for (size_t i = 0; i < errsz; ++i) {
            ASTError err = (*errors)[i];
            printf("%s:%ld:%ld %s\n", path, err.line, err.offset, err.msg);
            
            size_t linenum = err.line;
            printf("%5ld |\t", linenum++);

            char* line = err.linestr;
            for(size_t j = 0; line[j] != '\0'; ++j) {
                printf("%c", line[j]);
                if (line[j] == '\n')
                    printf("%5ld |\t", linenum++);
            }
            printf("\n");
            printf("%6c|%*c\n", ' ', (int)err.offset+1, '^');
        }

        printf("\n\n");
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

    char* buffer = (char*)malloc(file_size + 1);
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

    dumpast(&ast, &m);
    
    dumperrs(path, &m.errors, m.errsz);

    free(src);
}

int main(int argc, const char** argv)
{

    if (argc == 2)
        run_file(argv[1]);
    else 
        fprintf(stderr, "require file path");
    
    return 0;
}