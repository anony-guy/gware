#include <stdio.h>
#include "lexer.h"
#include "token.h"

int main() {
    printf("TOKEN_DOT = %d\n", TOKEN_DOT);
    Lexer* l = Lexer_create(".");
    Token tok = Lexer_nextToken(l);
    printf("Lexed type: %d, literal: '%s'\n", tok.type, tok.literal);
    return 0;
}
