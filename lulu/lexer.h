#ifndef LULU_LEXER_H
#define LULU_LEXER_H

#include "internal.h"
#include "strings.h"

// Although we wish to implement a typed version of Lua, the base types
// themsleves are not keywords and can be re-assigned. This is similar to how
// Odin does it.
#define TOKEN_KINDS(X)                                                         \
    X(TOKEN_NONE,          "<none>")                                           \
    X(TOKEN_EOF,           "<eof>")                                            \
    X(TOKEN_IDENT,         "<identifier>")                                     \
    X(TOKEN_INT,           "<int>")                                            \
    X(TOKEN_FLOAT,         "<float>")                                          \
    X(TOKEN_STRING,        "<string>")                                         \
    X(TOKEN_LEN,           "#")                                                \
    X(TOKEN_ASSIGN,        "=")                                                \
    X(TOKEN_ADD,           "+")                                                \
    X(TOKEN_SUB,           "-")                                                \
    X(TOKEN_MUL,           "*")                                                \
    X(TOKEN_DIV,           "/")                                                \
    X(TOKEN_MOD,           "%")                                                \
    X(TOKEN_POW,           "^")                                                \
    X(TOKEN_EQ,            "==")                                               \
    X(TOKEN_NEQ,           "~=")                                               \
    X(TOKEN_LT,            "<")                                                \
    X(TOKEN_LEQ,           "<=")                                               \
    X(TOKEN_GT,            ">")                                                \
    X(TOKEN_GEQ,           ">=")                                               \
    X(TOKEN_OPEN_PAREN,    "(")                                                \
    X(TOKEN_CLOSE_PAREN,   ")")                                                \
    X(TOKEN_OPEN_CURLY,    "{")                                                \
    X(TOKEN_CLOSE_CURLY,   "}")                                                \
    X(TOKEN_OPEN_BRACKET,  "[")                                                \
    X(TOKEN_CLOSE_BRACKET, "]")                                                \
    X(TOKEN_COLON,          ":")                                               \
    X(TOKEN_SEMICOL,        ";")                                               \
    X(TOKEN_COMMA,          ",")                                               \
    X(TOKEN_PERIOD,         ".")                                               \
    X(TOKEN_CONCAT,         "..")                                              \
    X(TOKEN_VARARG,         "...")                                             \
    X(TOKEN_ARROW,          "->")                                              \
    X(TOKEN_AND,            "and")                                             \
    X(TOKEN_BREAK,          "break")                                           \
    X(TOKEN_DO,             "do")                                              \
    X(TOKEN_ELSE,           "else")                                            \
    X(TOKEN_ELSEIF,         "elseif")                                          \
    X(TOKEN_END,            "end")                                             \
    X(TOKEN_FALSE,          "false")                                           \
    X(TOKEN_FOR,            "for")                                             \
    X(TOKEN_FUNCTION,       "function")                                        \
    X(TOKEN_IF,             "if")                                              \
    X(TOKEN_IN,             "in")                                              \
    X(TOKEN_LOCAL,          "local")                                           \
    X(TOKEN_NIL,            "nil")                                             \
    X(TOKEN_NOT,            "not")                                             \
    X(TOKEN_OR,             "or")                                              \
    X(TOKEN_REPEAT,         "repeat")                                          \
    X(TOKEN_RETURN,         "return")                                          \
    X(TOKEN_THEN,           "then")                                            \
    X(TOKEN_TRUE,           "true")                                            \
    X(TOKEN_UNTIL,          "until")                                           \
    X(TOKEN_WHILE,          "while")

/**
 * @link https://www.lua.org/manual/5.1/manual.html
 */
enum Token_Kind {
#define X(e, s) e,
    TOKEN_KINDS(X)
#undef X
};

typedef enum   Token_Kind Token_Kind;
typedef struct Token      Token;
struct Token {
    Token_Kind kind;

    // String view into the source code.
    String lexeme;

    // Position information.
    i32 line, col;
};

typedef struct Lexer Lexer;
struct Lexer {
    // File name and contents.
    String path, input;

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
lexer_make(String path, String input);

LULU_INTERNAL_FUNC const char *
lexer_error_string(Lexer_Error err);

LULU_INTERNAL_FUNC Lexer_Error
lexer_scan_token(Lexer *x, Token *t);

LULU_INTERNAL_FUNC bool
lexer_parse_u64(String s, u64 *v);

LULU_INTERNAL_FUNC bool
lexer_parse_f64(String s, f64 *v);

#endif // !LULU_LEXER_H

