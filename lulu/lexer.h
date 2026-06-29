#ifndef LULU_LEXER_H
#define LULU_LEXER_H

#include "internal.h"

// Although we wish to implement a typed version of Lua, the base types
// themsleves are not keywords and can be re-assigned. This is similar to how
// Odin does it.
#define TOKEN_KINDS(x)                                                         \
    x(TOKEN_NONE,     "<none>"),                                               \
    x(TOKEN_EOF,      "<eof>"),                                                \
    x(TOKEN_IDENT,    "<identifier>"),                                         \
    x(TOKEN_INT,      "<int>"),                                                \
    x(TOKEN_FLOAT,    "<float>"),                                              \
    x(TOKEN_STRING,   "<string>"),                                             \
    x(TOKEN_ASSIGN,   "="),                                                    \
    x(TOKEN_ADD,      "+"),     x(TOKEN_SUB,           "-"),                   \
    x(TOKEN_MUL,      "*"),     x(TOKEN_DIV,           "/"),                   \
    x(TOKEN_MOD,      "%"),     x(TOKEN_POW,           "^"),                   \
    x(TOKEN_EQ,       "=="),    x(TOKEN_NEQ,           "~="),                  \
    x(TOKEN_LT,       "<"),     x(TOKEN_LEQ,           "<="),                  \
    x(TOKEN_GT,       ">"),     x(TOKEN_GEQ,           ">="),                  \
    x(TOKEN_OPEN_PAREN,   "("), x(TOKEN_CLOSE_PAREN,   ")"),                   \
    x(TOKEN_OPEN_CURLY,   "{"), x(TOKEN_CLOSE_CURLY,   "}"),                   \
    x(TOKEN_OPEN_BRACKET, "["), x(TOKEN_CLOSE_BRACKET, "]"),                   \
    x(TOKEN_COLON,    ":"),                                                    \
    x(TOKEN_SEMICOL,  ";"),                                                    \
    x(TOKEN_COMMA,    ","),                                                    \
    x(TOKEN_PERIOD,   "."),                                                    \
    x(TOKEN_CONCAT,   ".."),                                                   \
    x(TOKEN_VARARG,   "..."),                                                  \
    x(TOKEN_ARROW,    "->"),                                                   \
    x(TOKEN_AND,      "and"),                                                  \
    x(TOKEN_BREAK,    "break"),                                                \
    x(TOKEN_DO,       "do"),                                                   \
    x(TOKEN_ELSE,     "else"),                                                 \
    x(TOKEN_ELSEIF,   "elseif"),                                               \
    x(TOKEN_END,      "end"),                                                  \
    x(TOKEN_FALSE,    "false"),                                                \
    x(TOKEN_FOR,      "for"),                                                  \
    x(TOKEN_FUNCTION, "function"),                                             \
    x(TOKEN_IF,       "if"),                                                   \
    x(TOKEN_IN,       "in"),                                                   \
    x(TOKEN_LOCAL,    "local"),                                                \
    x(TOKEN_NIL,      "nil"),                                                  \
    x(TOKEN_NOT,      "not"),                                                  \
    x(TOKEN_OR,       "or"),                                                   \
    x(TOKEN_REPEAT,   "repeat"),                                               \
    x(TOKEN_RETURN,   "return"),                                               \
    x(TOKEN_THEN,     "then"),                                                 \
    x(TOKEN_TRUE,     "true"),                                                 \
    x(TOKEN_UNTIL,    "until"),                                                \
    x(TOKEN_WHILE,    "while")

/**
 * @link https://www.lua.org/manual/5.1/manual.html
 */
enum Token_Kind {
#define TOKEN_KIND(e, s) e
    TOKEN_KINDS(TOKEN_KIND),
#undef TOKEN_KIND
};

typedef enum Token_Kind Token_Kind;
typedef struct Token Token;
struct Token {
    Token_Kind kind;
    const char *lexeme;
    size_t len;
    Value_Literal value;

    // Position information.
    i32 line, col;
};

typedef struct Lexer Lexer;
struct Lexer {
    // File name.
    const char *name;

    // File contents.
    const char *input;
    size_t len;

    // Lexeme's starting position in `input`.
    size_t start;

    // Current view position in `input`. Must be `>= start`.
    size_t cursor;

    // Position information.
    i32 line, col;
};

enum Lexer_Error {
    LEXER_OK,
    LEXER_UNEXPECTED_CHARACTER,
    LEXER_INVALID_NUMBER,
    LEXER_UNTERMINATED_STRING,
};

typedef enum Lexer_Error Lexer_Error;

LULU_INTERNAL_FUNC const char *
token_cstring(Token_Kind k);

LULU_INTERNAL_FUNC Lexer
lexer_make(const char *name, const char *input, size_t len);

LULU_INTERNAL_FUNC Lexer_Error
lexer_scan_token(Lexer *x, Token *t);

#endif // !LULU_LEXER_H

