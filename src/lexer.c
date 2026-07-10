#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void Lexer_readChar(Lexer* l) {
    if (l->readPosition >= strlen(l->input)) {
        l->ch = 0; // EOF
    } else {
        l->ch = l->input[l->readPosition];
    }
    l->position = l->readPosition;
    l->readPosition += 1;
}

static char Lexer_peekChar(Lexer* l) {
    if (l->readPosition >= strlen(l->input)) {
        return 0;
    } else {
        return l->input[l->readPosition];
    }
}

Lexer* Lexer_create(char* input) {
    Lexer* l = (Lexer*)malloc(sizeof(Lexer));
    l->input = strdup(input);
    l->position = 0;
    l->readPosition = 0;
    Lexer_readChar(l);
    return l;
}

void Lexer_destroy(Lexer* l) {
    if (l) {
        free(l->input);
        free(l);
    }
}

static Token newToken(TokenType type, char* literal) {
    Token t;
    t.type = type;
    t.literal = strdup(literal);
    return t;
}

static void skipWhitespace(Lexer* l) {
    while (l->ch == ' ' || l->ch == '\t' || l->ch == '\n' || l->ch == '\r') {
        Lexer_readChar(l);
    }
}

static char* readIdentifier(Lexer* l) {
    int startPosition = l->position;
    while (isalpha(l->ch) || l->ch == '_') {
        Lexer_readChar(l);
    }
    int len = l->position - startPosition;
    char* ident = (char*)malloc(len + 1);
    strncpy(ident, l->input + startPosition, len);
    ident[len] = '\0';
    return ident;
}

static char* readNumber(Lexer* l) {
    int startPosition = l->position;
    while (isdigit(l->ch)) {
        Lexer_readChar(l);
    }
    int len = l->position - startPosition;
    char* num = (char*)malloc(len + 1);
    strncpy(num, l->input + startPosition, len);
    num[len] = '\0';
    return num;
}

static char* readString(Lexer* l) {
    int startPosition = l->position + 1; // skip the first "
    while (1) {
        Lexer_readChar(l);
        if (l->ch == '"' || l->ch == 0) {
            break;
        }
    }
    int len = l->position - startPosition;
    char* str = (char*)malloc(len + 1);
    strncpy(str, l->input + startPosition, len);
    str[len] = '\0';
    return str;
}

static TokenType lookupIdent(char* ident) {
    if (strcmp(ident, "set") == 0) return TOKEN_SET;
    if (strcmp(ident, "show") == 0) return TOKEN_SHOW;
    if (strcmp(ident, "if") == 0) return TOKEN_IF;
    if (strcmp(ident, "while") == 0) return TOKEN_WHILE;
    if (strcmp(ident, "def") == 0) return TOKEN_DEF;
    if (strcmp(ident, "return") == 0) return TOKEN_RETURN;
    if (strcmp(ident, "int") == 0) return TOKEN_TYPE_INT;
    if (strcmp(ident, "string") == 0) return TOKEN_TYPE_STRING;
    // GwareWeb keywords
    if (strcmp(ident, "component") == 0) return TOKEN_COMPONENT;
    if (strcmp(ident, "action") == 0) return TOKEN_ACTION;
    if (strcmp(ident, "style") == 0) return TOKEN_STYLE;
    if (strcmp(ident, "view") == 0) return TOKEN_VIEW;
    return TOKEN_IDENTIFIER;
}

Token Lexer_nextToken(Lexer* l) {
    Token tok;
    skipWhitespace(l);

    switch (l->ch) {
        case '=': 
            if (Lexer_peekChar(l) == '=') {
                Lexer_readChar(l);
                tok = newToken(TOKEN_EQUALS, "==");
            } else {
                tok = newToken(TOKEN_ASSIGN, "="); 
            }
            break;
        case '+': tok = newToken(TOKEN_PLUS, "+"); break;
        case '-': tok = newToken(TOKEN_MINUS, "-"); break;
        case '*': tok = newToken(TOKEN_STAR, "*"); break;
        case '/': tok = newToken(TOKEN_SLASH, "/"); break;
        case '<': tok = newToken(TOKEN_LESS, "<"); break;
        case '>': tok = newToken(TOKEN_GREATER, ">"); break;
        case '(': tok = newToken(TOKEN_LPAREN, "("); break;
        case ')': tok = newToken(TOKEN_RPAREN, ")"); break;
        case '{': tok = newToken(TOKEN_LBRACE, "{"); break;
        case '}': tok = newToken(TOKEN_RBRACE, "}"); break;
        case '[': tok = newToken(TOKEN_LBRACKET, "["); break;
        case ']': tok = newToken(TOKEN_RBRACKET, "]"); break;
        case ':': tok = newToken(TOKEN_COLON, ":"); break;
        case ',': tok = newToken(TOKEN_COMMA, ","); break;
        case '"':
            tok.literal = readString(l);
            tok.type = TOKEN_STRING;
            break;
        case 0:   tok = newToken(TOKEN_EOF, ""); break;
        default:
            if (isalpha(l->ch)) {
                tok.literal = readIdentifier(l);
                tok.type = lookupIdent(tok.literal);
                return tok; 
            } else if (isdigit(l->ch)) {
                tok.literal = readNumber(l);
                tok.type = TOKEN_NUMBER;
                return tok; 
            } else {
                char str[2] = {l->ch, '\0'};
                tok = newToken(TOKEN_ILLEGAL, str);
            }
            break;
    }
    Lexer_readChar(l);
    return tok;
}
