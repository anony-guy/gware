#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct {
    char* input;
    int position;     // current position in input (points to current char)
    int readPosition; // current reading position in input (after current char)
    char ch;          // current char under examination
} Lexer;

Lexer* Lexer_create(char* input);
void Lexer_destroy(Lexer* l);
Token Lexer_nextToken(Lexer* l);

#endif // LEXER_H
