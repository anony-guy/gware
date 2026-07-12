#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void Lexer_readChar(Lexer* l) {
    if (l->readPosition >= strlen(l->input)) {
        l->ch = 0; // EOF
    } else {
        l->ch = l->input[l->readPosition];
        if (l->ch == '\n') l->line++;
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

Lexer* Lexer_create(char* input, const char* file) {
    Lexer* l = (Lexer*)malloc(sizeof(Lexer));
    l->input = strdup(input);
    l->position = 0;
    l->readPosition = 0;
    l->line = 1;
    l->file = file ? strdup(file) : NULL;
    Lexer_readChar(l);
    return l;
}

void Lexer_destroy(Lexer* l) {
    if (l) {
        free(l->input);
        free(l->file);
        free(l);
    }
}

static Token newTokenInternal(GwTokenType type, char* literal, Lexer* l) {
    Token t;
    t.type = type;
    t.literal = literal ? strdup(literal) : NULL;
    t.line = l ? l->line : 0;
    t.file = l ? (l->file ? strdup(l->file) : NULL) : NULL;
    return t;
}

#define newToken(type, literal) newTokenInternal(type, literal, l)

static void skipWhitespace(Lexer* l) {
    while (l->ch == ' ' || l->ch == '\t' || l->ch == '\n' || l->ch == '\r') {
        Lexer_readChar(l);
    }
}

static char* readIdentifier(Lexer* l) {
    int startPosition = l->position;
    while (isalpha(l->ch) || l->ch == '_' || isdigit(l->ch)) {
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
    int capacity = 32;
    int len = 0;
    char* str = (char*)malloc(capacity);
    
    while (1) {
        Lexer_readChar(l);
        if (l->ch == '"' || l->ch == 0) {
            break;
        }
        if (l->ch == '\\') {
            Lexer_readChar(l); // skip the backslash
            // basic translation
            if (l->ch == 'n') l->ch = '\n';
            else if (l->ch == 't') l->ch = '\t';
            else if (l->ch == 'r') l->ch = '\r';
            // otherwise just keep the escaped char (e.g., ")
        }
        str[len++] = l->ch;
        if (len >= capacity - 1) {
            capacity *= 2;
            str = realloc(str, capacity);
        }
    }
    str[len] = '\0';
    return str;
}

static GwTokenType lookupIdent(char* ident) {
    if (strcmp(ident, "set") == 0) return TOKEN_SET;
    if (strcmp(ident, "show") == 0) return TOKEN_SHOW;
    if (strcmp(ident, "if") == 0) return TOKEN_IF;
    if (strcmp(ident, "else") == 0) return TOKEN_ELSE;
    if (strcmp(ident, "while") == 0) return TOKEN_WHILE;
    if (strcmp(ident, "for") == 0) return TOKEN_FOR;
    if (strcmp(ident, "in") == 0) return TOKEN_IN;
    if (strcmp(ident, "def") == 0) return TOKEN_DEF;
    if (strcmp(ident, "return") == 0) return TOKEN_RETURN;
    if (strcmp(ident, "break") == 0) return TOKEN_BREAK;
    if (strcmp(ident, "continue") == 0) return TOKEN_CONTINUE;
    if (strcmp(ident, "const") == 0) return TOKEN_CONST;
    if (strcmp(ident, "import") == 0) return TOKEN_IMPORT;
    if (strcmp(ident, "import") == 0) return TOKEN_IMPORT;
    if (strcmp(ident, "try") == 0) return TOKEN_TRY;
    if (strcmp(ident, "catch") == 0) return TOKEN_CATCH;
    if (strcmp(ident, "int") == 0) return TOKEN_TYPE_INT;
    if (strcmp(ident, "string") == 0) return TOKEN_TYPE_STRING;
    // GwareWeb keywords
    if (strcmp(ident, "component") == 0) return TOKEN_COMPONENT;
    if (strcmp(ident, "action") == 0) return TOKEN_ACTION;
    if (strcmp(ident, "style") == 0) return TOKEN_STYLE;
    if (strcmp(ident, "view") == 0) return TOKEN_VIEW;
    if (strcmp(ident, "store") == 0) return TOKEN_STORE;
    if (strcmp(ident, "router") == 0) return TOKEN_ROUTER;
    if (strcmp(ident, "route") == 0) return TOKEN_ROUTE;
    return TOKEN_IDENTIFIER;
}

Token Lexer_nextToken(Lexer* l) {
    Token tok = {0};
    skipWhitespace(l);

    switch (l->ch) {
        case '=': 
            if (Lexer_peekChar(l) == '=') {
                Lexer_readChar(l);
                tok = newToken(TOKEN_EQUALS, "==");
            } else if (Lexer_peekChar(l) == '>') {
                Lexer_readChar(l);
                tok = newToken(TOKEN_ARROW, "=>");
            } else {
                tok = newToken(TOKEN_ASSIGN, "="); 
            }
            break;
        case '!':
            if (Lexer_peekChar(l) == '=') {
                Lexer_readChar(l);
                tok = newToken(TOKEN_NOT_EQ, "!=");
            } else {
                tok = newToken(TOKEN_ILLEGAL, "!");
            }
            break;
        case '+': tok = newToken(TOKEN_PLUS, "+"); break;
        case '-': tok = newToken(TOKEN_MINUS, "-"); break;
        case '*': tok = newToken(TOKEN_STAR, "*"); break;
        case '/': 
            if (Lexer_peekChar(l) == '/') {
                while (l->ch != '\n' && l->ch != 0) {
                    Lexer_readChar(l);
                }
                return Lexer_nextToken(l);
            }
            tok = newToken(TOKEN_SLASH, "/"); 
            break;
        case '<': 
            if (Lexer_peekChar(l) == '<') {
                Lexer_readChar(l);
                tok = newToken(TOKEN_SHL, "<<");
            } else {
                tok = newToken(TOKEN_LESS, "<"); 
            }
            break;
        case '>': 
            if (Lexer_peekChar(l) == '>') {
                Lexer_readChar(l);
                tok = newToken(TOKEN_SHR, ">>");
            } else {
                tok = newToken(TOKEN_GREATER, ">"); 
            }
            break;
        case '&': tok = newToken(TOKEN_AMPERSAND, "&"); break;
        case '|': tok = newToken(TOKEN_PIPE, "|"); break;
        case '^': tok = newToken(TOKEN_CARET, "^"); break;
        case '.': tok = newToken(TOKEN_DOT, "."); break;
        case '(': tok = newToken(TOKEN_LPAREN, "("); break;
        case ')': tok = newToken(TOKEN_RPAREN, ")"); break;
        case '{': tok = newToken(TOKEN_LBRACE, "{"); break;
        case '}': tok = newToken(TOKEN_RBRACE, "}"); break;
        case '[': tok = newToken(TOKEN_LBRACKET, "["); break;
        case ']': tok = newToken(TOKEN_RBRACKET, "]"); break;
        case ':': tok = newToken(TOKEN_COLON, ":"); break;
        case '?': tok = newToken(TOKEN_QUESTION, "?"); break;

        case ',': tok = newToken(TOKEN_COMMA, ","); break;
        case '"':
            tok.literal = readString(l);
            tok.type = TOKEN_STRING;
            tok.line = l ? l->line : 0;
            tok.file = l ? (l->file ? strdup(l->file) : NULL) : NULL;
            break;
        case '$':
            if (Lexer_peekChar(l) == '"') {
                Lexer_readChar(l);
                tok.literal = readString(l);
                tok.type = TOKEN_INTERP_STRING;
                tok.line = l ? l->line : 0;
                tok.file = l ? (l->file ? strdup(l->file) : NULL) : NULL;
            } else {
                char str[2] = {'$', '\0'};
                tok = newToken(TOKEN_ILLEGAL, str);
            }
            break;
        case 0:   tok = newToken(TOKEN_EOF, ""); break;
        default:
            if (isalpha(l->ch)) {
                tok.literal = readIdentifier(l);
                tok.type = lookupIdent(tok.literal);
                tok.line = l ? l->line : 0;
                tok.file = l ? (l->file ? strdup(l->file) : NULL) : NULL;
                return tok; 
            } else if (isdigit(l->ch)) {
                tok.literal = readNumber(l);
                tok.type = TOKEN_NUMBER;
                tok.line = l ? l->line : 0;
                tok.file = l ? (l->file ? strdup(l->file) : NULL) : NULL;
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
