#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer* l;
    Token curToken;
    Token peekToken;
} Parser;

Parser* Parser_create(Lexer* l);
void Parser_destroy(Parser* p);
ASTNode* Parser_parseProgram(Parser* p);

#endif // PARSER_H
