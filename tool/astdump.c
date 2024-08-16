#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "vector.h"

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

static void dumpast(ASTNode *ast, const ASTMetadata *m)
{
    size_t graphsz = m->size;
    
    bool marked[graphsz];

    memset(marked, 0, sizeof(*marked) * graphsz);

    vec_t(ASTNode *) stck = NULL;
    vec_t(size_t) depths = NULL;

    size_t index = 0;
    size_t maxdepth = m->depth;

    vecinit(stck, 10);
    vecinit(depths, 10);

    vecpush(stck, ast);
    vecpush(depths, 0);

    size_t midpoint = 2 * maxdepth + 15;
    size_t idofs = graphsz / 10;
    size_t col1sz = midpoint - 4; 

    vec_t(char) header;
    vecinit(header, 100);
    veccat(header, "Tree ", 5);
    vecset(header, ' ', col1sz);
    veccat(header, " ID ", 4);
    vecset(header, ' ', 3 + idofs);
    veccat(header, "Token", 5);
    printf("%s\n\n", header);

    vecfree(header);

    while (vecsize(stck) > 0)
    {
        ASTNode* v = *vecpop(stck);
        size_t depth = *vecpop(depths);
        size_t vnum = v->id;

        if (!marked[index]) {
            size_t degree = ast_degree(v);
            marked[index] = true;
            index++;

            vec_t(char) line = NULL;

            char* typestr = ast2string(v->type);
            size_t typestrsz = strlen(typestr);
            size_t lexsz = tkn_lexeme_size(v->tkn);

            vecinit(line, typestrsz + lexsz + depth * 2 + 100);

            if (v->type != AST_ROOT) {
                vecpush(line, '|');
                vecset(line, '_', depth * 2);
            }

            if (vecsize(line) > 0) {
                vecpush(line, ' ');
            }

            veccat(line, typestr, typestrsz);
            vecpush(line, ' ');

            int diff = midpoint - vecsize(line);

            if (diff > 0) {
                vecset(line, '.', diff);
                veccat(line, " |", 2);
            }

            char tstr[64];
            snprintf(tstr, 64, " [%ld] ", vnum);
            size_t tsz = strlen(tstr);

            veccat(line, tstr, tsz);

            size_t vidofs = v->id / 10;

            diff = idofs - vidofs;

            if (diff > 0) {
                vecset(line, ' ', diff);
            }

            vecpush(line, '|');
            vecpush(line, ' ');

            if (v->tkn != NULL) {
                char lexeme[lexsz];
                tkn_lexeme(v->tkn, lexeme, lexsz);
                veccat(line, lexeme, lexsz);
            } else {
                vecpush(line, '\0');
            }

            printf("%s\n", line);

            vecfree(line);
                
            free(typestr);

            for (size_t ii = 0; ii < degree; ii++)
            {
                ASTNode* w = &v->child[ii];
                size_t windex = index + ii;

                if (!marked[windex]) {
                    vecpush(stck, w);
                    vecpush(depths, depth+1);
                }
            }
        }
    }

    printf("\n");

    vecfree(stck);
    vecfree(depths);
}

static void dumperrs(const char* path, vec_t(ASTError) errors, size_t errsz) {
    if (vecsize(errors)) {
        printf("ERRORS FOUND: %ld\n", vecsize(errors));

        // using m->errsz instead of vecsize is intentional
        // its to ensure vecsize() == m->errsz
        for (size_t i = 0; i < errsz; ++i) {
            ASTError err = errors[i];
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
            // printf("%s\n", line);
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
    
    Parser p = {.tkns = NULL};
    ASTNode* ast = jary_alloc(sizeof(*ast));
    ASTMetadata m = {.tkns = NULL};

    jary_parse(&p, ast, &m, src, length);

    dumpast(ast, &m);
    
    dumperrs(path, m.errors, m.errsz);

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