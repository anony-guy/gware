#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    TOKEN_ILLEGAL,
    TOKEN_EOF,

    // Identifiers + literals
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_INTERP_STRING,

    // Operators
    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    
    // Comparison
    TOKEN_EQUALS,  // ==
    TOKEN_NOT_EQ,  // !=
    TOKEN_LESS,    // <
    TOKEN_GREATER, // >
    TOKEN_DOT,     // .
    TOKEN_QUESTION,// ?
    TOKEN_ARROW,   // =>
    
    // Bitwise
    TOKEN_AMPERSAND, // &
    TOKEN_PIPE,      // |
    TOKEN_CARET,     // ^
    TOKEN_SHL,       // <<
    TOKEN_SHR,       // >>


    // Delimiters
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,

    // Keywords
    TOKEN_SET,
    TOKEN_SHOW,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_DEF,
    TOKEN_RETURN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_CONST,
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
} GwTokenType;

typedef struct {
    GwTokenType type;
    char* literal;
    int line;
    char* file;
} Token;

#endif // TOKEN_H
