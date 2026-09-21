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

struct Pos {
    i32 line = 0;
    i32 col  = 0;
};

struct Loc {
    // String view into the source code.
    String view;

    // Line and column information of said string view.
    Pos pos;
};

struct Token {
    TokenKind kind = Token_None;
    Loc       loc;
    union {
        intr  integer = 0;
        real  floating;
    };
};

enum class LexerErrorKind : u8 {
    Unexpected_Character,
    Invalid_Base,
    Invalid_Digit,
    Invalid_Exponent,
    Excess_Underscores,
    Unterminated_String,
};

// Best to have a similar layout to `Token`.
struct LexerError {
    LexerErrorKind kind;
    Loc            loc;
};

using LexerResult = Result<Token, LexerError>;


class Lexer {
    /// File name and contents.
    String path, input;

    /// Lexeme's starting offset in `input`.
    usize prev_offset;

    // Current view offset in `input`. Must be `>= start`.
    usize curr_offset;

    /// Position information.
    Pos prev_pos, curr_pos;

public:
    static Lexer
    make(String path, String input)
    {
        Lexer x;
        x.path        = path;
        x.input       = input;
        x.prev_offset = 0;
        x.curr_offset = 0;
        x.prev_pos    = Pos{1, 1};
        x.curr_pos    = Pos{1, 1};
        return x;
    }

    String
    get_path() const noexcept { return this->path; }

    LexerResult
    scan_token();

    Token
    make_token(TokenKind kind) const noexcept
    {
        Token t;
        t.kind    = kind;
        t.loc     = Loc{this->lexeme(), this->prev_pos};
        t.integer = 0;
        return t;
    }

    Token
    make_token_int(intr i) const noexcept
    {
        Token t = this->make_token(Token_Int);
        t.integer = i;
        return t;
    }

    Token
    make_token_float(real r) const noexcept
    {
        Token t = this->make_token(Token_Float);
        t.floating = r;
        return t;
    }

    LexerError
    make_error(LexerErrorKind kind) const noexcept
    {
        return LexerError{kind, Loc{this->lexeme(), this->curr_pos}};
    }

private:
    void
    next();

    Option<char>
    peek() const noexcept
    {
        return this->peek_at(0);
    }


    Option<char>
    peek_at(usize offset) const noexcept;

    String
    lexeme()              const;

    bool
    check(char wanted)    const noexcept;

    bool
    match(char wanted)    noexcept;

    template<class F>
    int
    take_while(F f);

    Option<char>
    skip_whitespace();

    LexerResult
    scan_number(char leader);

    Result<intr, LexerError>
    parse_int(int base);

    Result<real, LexerError>
    parse_fraction();

    LexerResult
    scan_string(char quote);
}; // class Lexer

LULU_INTERNAL_FUNC char const *
token_kind_cstring(TokenKind k);

LULU_INTERNAL_FUNC char const *
lexer_error_string(LexerErrorKind err);

