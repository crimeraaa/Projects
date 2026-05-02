#ifndef CDECL_LEXER_H
#define CDECL_LEXER_H

#include "cdecl.h"

enum Token_Kind {
    // Zero value, default type for no token yet.
    TOKEN_NONE,

    // End of file- no more input to be had!
    TOKEN_EOF,
    
    // C keywords that we care about
    TOKEN_BOOL,   TOKEN_CHAR,   TOKEN_CONST, TOKEN_DOUBLE, TOKEN_ENUM,
    TOKEN_FALSE,  TOKEN_FLOAT,  TOKEN_INT,   TOKEN_LONG,   TOKEN_SHORT,
    TOKEN_SIGNED, TOKEN_STRUCT, TOKEN_TRUE,  TOKEN_VOID,   TOKEN_VOLATILE,
    TOKEN_UNION,  TOKEN_UNSIGNED,

    // '(' ')' C-style casts, some pointer declarations
    TOKEN_PAREN_OPEN, TOKEN_PAREN_CLOSE,

    // '[' ']' Fixed-size array declarations
    TOKEN_BRACE_OPEN, TOKEN_BRACE_CLOSE,

    // `struct` and `union` definitions
    TOKEN_CURLY_OPEN, TOKEN_CURLY_CLOSE,

    // '*' Pointer declarations
    TOKEN_ASTERISK,
    TOKEN_AMPERSAND,

    // ',' Function parameter separator
    TOKEN_COMMA,

    // ';' Statement separator
    TOKEN_SEMICOLON,

    // Nonterminals
    TOKEN_IDENTIFIER,
};

struct Token {
    enum Token_Kind kind;
    const char *text;
    size_t len;
    int line, col;
};

struct Lexer {
    const char *text;
    size_t len;

    // String views into `text`.
    const char *start, *current;

    // File iteration information.
    int line, col;
};

void
lexer_init(struct Lexer *x, const char *s, size_t n);

enum Type_Error
lexer_scan_token(struct Lexer *x, struct Token *t);

#endif // !CDECL_LEXER_H
