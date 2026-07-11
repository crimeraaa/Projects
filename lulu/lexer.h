#ifndef LULU_LEXER_H
#define LULU_LEXER_H

#include "internal.h"
#include "strings.h"

// Although we wish to implement a typed version of Lua, the base types
// themsleves are not keywords and can be re-assigned. This is similar to how
// Odin does it.
#define TOKEN_KINDS(X)                                                         \
    X(Token_None,   "<none>")                                                  \
    X(Token_Eof,    "<eof>")                                                   \
    X(Token_Ident,  "<identifier>")                                            \
    X(Token_Int,    "<int>")                                                   \
    X(Token_Float,  "<float>")                                                 \
    X(Token_String, "<string>")                                                \
/* BEGIN(1): Operators */                                                      \
    X(Token_Len,    "#")                                                       \
    X(Token_Assign, "=")                                                       \
/* BEGIN(2): Binary Operators */                                               \
    X(Token_Add, "+")                                                          \
    X(Token_Sub, "-")                                                          \
    X(Token_Mul, "*")                                                          \
    X(Token_Div, "/")                                                          \
    X(Token_Mod, "%")                                                          \
    X(Token_Eq,  "==")                                                         \
    X(Token_Neq, "~=")                                                         \
    X(Token_Lt,  "<")                                                          \
    X(Token_Leq, "<=")                                                         \
    X(Token_Gt,  ">")                                                          \
    X(Token_Geq, ">=")                                                         \
/* END(2): Binary Operators */                                                 \
    X(Token_Open_Paren,    "(")                                                \
    X(Token_Close_Paren,   ")")                                                \
    X(Token_Open_Curly,    "{")                                                \
    X(Token_Close_Curly,   "}")                                                \
    X(Token_Open_Bracket,  "[")                                                \
    X(Token_Close_Bracket, "]")                                                \
    X(Token_Colon,         ":")                                                \
    X(Token_Semicol,       ";")                                                \
    X(Token_Comma,         ",")                                                \
    X(Token_Period,        ".")                                                \
    X(Token_Concat,        "..")                                               \
    X(Token_Vararg,        "...")                                              \
    X(Token_Arrow,         "->")                                               \
/* END(1): Operators */                                                        \
/* BEGIN:  Keywords  */                                                        \
    X(Token_and,      "and")                                                   \
    X(Token_break,    "break")                                                 \
    X(Token_cast,     "cast")                                                  \
    X(Token_do,       "do")                                                    \
    X(Token_else,     "else")                                                  \
    X(Token_elseif,   "elseif")                                                \
    X(Token_end,      "end")                                                   \
    X(Token_false,    "false")                                                 \
    X(Token_for,      "for")                                                   \
    X(Token_function, "function")                                              \
    X(Token_if,       "if")                                                    \
    X(Token_in,       "in")                                                    \
    X(Token_local,    "local")                                                 \
    X(Token_nil,      "nil")                                                   \
    X(Token_not,      "not")                                                   \
    X(Token_or,       "or")                                                    \
    X(Token_repeat,   "repeat")                                                \
    X(Token_return,   "return")                                                \
    X(Token_then,     "then")                                                  \
    X(Token_true,     "true")                                                  \
    X(Token_until,    "until")                                                 \
    X(Token_while,    "while")
/* END: Keywords */

/**
 * @link https://www.lua.org/manual/5.1/manual.html
 */
enum TokenKind {
#define X(e, s) e,
    TOKEN_KINDS(X)
#undef X
};

typedef enum   TokenKind TokenKind;
typedef struct Token     Token;
struct Token {
    TokenKind kind;

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
    usize start;

    // Current view position in `input`. Must be `>= start`.
    usize cursor;

    // Position information.
    i32 line, col;
};

enum LexerError {
    LEXER_OK,
    LEXER_UNEXPECTED_CHARACTER,
    LEXER_INVALID_NUMBER,
    LEXER_UNTERMINATED_STRING,
};

typedef enum LexerError LexerError;

LULU_INTERNAL_FUNC const char *
token_kind_cstring(TokenKind k);

static inline Token
token_make_none(void)
{
    Token t = {Token_None,
        /*lexeme=*/nullptr, /*len=*/0,
        /*line  =*/0,       /*col=*/0};
    return t;
}

static inline Lexer
lexer_make(String path, String input)
{
    Lexer x = {path, input, /*start=*/0, /*cursor=*/0, /*line=*/1, /*col=*/1};
    return x;
}

LULU_INTERNAL_FUNC const char *
lexer_error_string(LexerError err);

LULU_INTERNAL_FUNC LexerError
lexer_scan_token(Lexer *x, Token *t);

LULU_INTERNAL_FUNC bool
lexer_parse_uint(String s, lulu_uint *v);

LULU_INTERNAL_FUNC bool
lexer_parse_real(String s, lulu_real *v);

#endif // !LULU_LEXER_H

