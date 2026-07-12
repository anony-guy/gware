#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct {
    char* input;
    int position;
    int readPosition;
    char ch;
    int line;
    char* file;
} Lexer;

Lexer* Lexer_create(char* input, const char* file);
void Lexer_destroy(Lexer* l);
Token Lexer_nextToken(Lexer* l);

#endif // LEXER_H
