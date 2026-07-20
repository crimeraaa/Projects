#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "strings.hpp"

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
    X(Token_Assign,    "=")                                                    \
    X(Token_Ampersand, "&")                                                    \
    X(Token_Pipe,      "|")                                                    \
    X(Token_Caret,     "^")                                                    \
    X(Token_Tilde,     "~")                                                    \
    X(Token_Len,       "#")                                                    \
/* BEGIN(2): Binary Operators */                                               \
    X(Token_Plus,          "+")  X(Token_Dash,          "-")                   \
    X(Token_Asterisk,      "*")  X(Token_Slash,         "/")                   \
    X(Token_Percent,       "%")                                                \
    X(Token_Tilde_Equal,   "~=") X(Token_Equal_Equal,   "==")                  \
    X(Token_Greater_Equal, ">=") X(Token_Less_Than,     "<")                   \
    X(Token_Greater_Than,  ">")  X(Token_Less_Equal,    "<=")                  \
/* END(2): Binary Operators */                                                 \
    X(Token_Open_Paren,    "(")  X(Token_Close_Paren,   ")")                   \
    X(Token_Open_Curly,    "{")  X(Token_Close_Curly,   "}")                   \
    X(Token_Open_Bracket,  "[")  X(Token_Close_Bracket, "]")                   \
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
    X(Token_global,   "global")                                                \
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

/*
 Relevant links:
 1) https://www.lua.org/manual/5.1/manual.html
 */
enum TokenKind : u8 {
#define X(e, s) e,
    TOKEN_KINDS(X)
#undef X
};

struct Token {
    TokenKind kind = Token_None;

    // String view into the source code.
    String lexeme;

    // Position information.
    i32 line = 0, col = 0;
};

struct Lexer {
    // File name and contents.
    String path, input;

    // Lexeme's starting position in `input`.
    usize start = 0;

    // Current view position in `input`. Must be `>= start`.
    usize cursor = 0;

    // Position information.
    i32 line = 0, col = 0;
};

enum LexerError : u8 {
    LEXER_OK,
    LEXER_UNEXPECTED_CHARACTER,
    LEXER_INVALID_NUMBER,
    LEXER_UNTERMINATED_STRING,
};

LULU_INTERNAL_FUNC char const *
token_kind_cstring(TokenKind k);

LULU_INTERNAL_FUNC char const *
lexer_error_string(LexerError err);

LULU_INTERNAL_FUNC LexerError
lexer_scan_token(Lexer *x, Token *out);

LULU_INTERNAL_FUNC bool
lexer_parse_int(String s, lulu_int *out);

LULU_INTERNAL_FUNC bool
lexer_parse_real(String s, lulu_real *out);

