#ifndef JSON_LEXER_H
#define JSON_LEXER_H

// local
#include "json.h"

enum json_Token_Type {
    // Zero-value: invalid token
    TOKEN_INVALID,

    // End of file
    TOKEN_EOF,

    // terminals: keywords
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_TRUE,

    // terminals: characters
    TOKEN_CURLY_OPEN,   TOKEN_CURLY_CLOSE,   // '{' '}'
    TOKEN_BRACKET_OPEN, TOKEN_BRACKET_CLOSE, // '[' ']'
    TOKEN_COMMA, // ,
    TOKEN_COLON, // :

    // non-terminals: value literals
    TOKEN_NUMBER, // integer fraction exponent
    TOKEN_STRING, // '"' [^"]* '"'
};

typedef enum json_Token_Type json_Token_Type;
typedef struct json_Token json_Token;
struct json_Token {
    json_Token_Type type;
    
    // String view/slice into Lexer's input.
    const char *text;
    size_t len;

    // Position information
    int line, col;
};

typedef struct json_Lexer json_Lexer;
struct json_Lexer {
    // String view/slice into `input`.
    const char *start, *current;

    // Input text information.
    const char *input;
    size_t len;

    // Position information.
    int line, col;
};

extern void
json_init_lexer(json_Lexer *x, const char *input, size_t len);

extern json_Error
json_scan_token(json_Lexer *x, json_Token *t);

#endif // !JSON_LEXER_H
