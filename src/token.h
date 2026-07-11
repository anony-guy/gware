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
    TOKEN_DOT,     // .


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
    TOKEN_DEF,
    TOKEN_RETURN,
    TOKEN_TYPE_INT,
    TOKEN_TYPE_STRING,
    TOKEN_IMPORT,

    // GwareWeb Keywords
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_COMPONENT,
    TOKEN_ACTION,
    TOKEN_STYLE,
    TOKEN_STORE,
    TOKEN_ROUTER,
    TOKEN_ROUTE,

    TOKEN_VIEW,

    // GwareWeb Symbols
    TOKEN_COLON,
    
    // Punctuation
    TOKEN_COMMA,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET
} TokenType;

typedef struct {
    TokenType type;
    char* literal;
} Token;

#endif // TOKEN_H
