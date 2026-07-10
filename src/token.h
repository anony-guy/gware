#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    TOKEN_ILLEGAL,
    TOKEN_EOF,

    // Identifiers + literals
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,

    // Operators
    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    
    // Comparison
    TOKEN_EQUALS,  // ==
    TOKEN_LESS,    // <
    TOKEN_GREATER, // >

    // Delimiters
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,

    // Keywords
    TOKEN_SET,
    TOKEN_SHOW,
    TOKEN_IF,
    TOKEN_WHILE,
    TOKEN_TYPE_INT,
    TOKEN_TYPE_STRING,

    // GwareWeb Keywords
    TOKEN_COMPONENT,
    TOKEN_ACTION,
    TOKEN_STYLE,
    TOKEN_VIEW,

    // GwareWeb Symbols
    TOKEN_COLON
} TokenType;

typedef struct {
    TokenType type;
    char* literal;
} Token;

#endif // TOKEN_H
