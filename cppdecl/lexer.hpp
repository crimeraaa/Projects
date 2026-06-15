#pragma once

#include <climits> // CHAR_MAX
#include "types.hpp"
#include "string.hpp"
#include "ast.hpp"

#define KEYWORD_LIST(x)                                                        \
    x(bool),      x(break),                                                    \
    x(case),      x(char),     x(const),    x(continue),                       \
    x(default),   x(do),       x(double),                                      \
    x(else),      x(enum),                                                     \
    x(false),     x(float),    x(for),                                         \
    x(goto),                                                                   \
    x(if),        x(inline),   x(int),                                         \
    x(long),                                                                   \
    x(namespace),                                                              \
    x(return),                                                                 \
    x(short),     x(signed),   x(struct),                                      \
    x(template),  x(this),     x(true),     x(typedef),                        \
    x(union),     x(unsigned), x(using),                                       \
    x(void),      x(volatile),                                                 \
    x(while)

enum Token_Kind : u8 {
    TK_NONE,

    // Terminals: single-characters
    TK_LPAREN  = '(',  TK_RPAREN  = ')', TK_LSQUARE = '[',  TK_RSQUARE = ']',
    TK_LCURLY  = '{',  TK_RCURLY  = '}', TK_COLON   = ':',
    TK_COMMA   = ',',  TK_SEMICOL = ';',
    TK_PERIOD = '.',   TK_ASSIGN = '=',

    // Terminals: single-character bitwise operators
    TK_BNOT = '~', TK_BAND = '&', TK_BOR  = '|', TK_BXOR = '^',

    // Terminals: single-character logical operators
    TK_NOT = '!',

    // Terminals: Single-character relational operators
    TK_LT = '<', TK_GT = '>',

    // Terminals: Single-character arithmetic operators
    TK_ADD = '+', TK_SUB = '-',
    TK_MUL = '*', TK_DIV = '/', TK_MOD = '%',

    // Terminals: Multi-character sequences
    TK_MULTICHAR_START = CHAR_MAX + 1,

#define X(kw) TK_##kw
    KEYWORD_LIST(X),
#undef X

    TK_ELLIPSIS, TK_ARROW, TK_SCOPE, // "..." "->" "::"
    TK_SHL, TK_SHR, // "<<" ">>"
    TK_AND, TK_OR,  // "&&" "||"
    TK_EQ,  TK_NEQ, // "==" "!="
    TK_LEQ, TK_GEQ, // "<=" ">="

    TK_INCR,        TK_DECR,                       // "++"  "--"
    TK_BAND_ASSIGN, TK_BOR_ASSIGN, TK_BXOR_ASSIGN, // "&="  "|=" "^="
    TK_SHL_ASSIGN,  TK_SHR_ASSIGN,                 // "<<=" ">>="
    TK_ADD_ASSIGN,  TK_SUB_ASSIGN,                 // "+="  "-="
    TK_MUL_ASSIGN,  TK_DIV_ASSIGN, TK_MOD_ASSIGN,  // "*="  "/=" "%="

    TK_LITERAL_CHAR, // '\'' char '\''
    TK_LITERAL_INT, TK_LITERAL_FLOAT, TK_LITERAL_STRING,
    TK_IDENT, 

    // Misc.
    TK_EOF,

    TK_MULTICHAR_COUNT = TK_EOF - TK_MULTICHAR_START,
};

struct Token {
    Token_Kind kind;
    String lexeme;
    int line, col;
    Ast_Literal data;
};

struct Lexer {
private:
    String m_input;
    String::const_iterator m_start, m_current;
    int m_line, m_col;

public:
    Lexer(String input)
        : m_input{input}
        , m_start{input.begin()}
        , m_current{input.begin()}
        , m_line{1}
        , m_col{1}
    {}

    Error
    scan_token(Token *t);

private:
    // Returns true if we are at EOF else false.
    bool
    is_joever() const;

    // Returns the character at the current iterator.
    char
    peek() const;

    // Returns the character at 1 past the current iterator,
    // if there is one. Otherwise returns the nul byte.
    char
    peek_next() const;

    // Returns true if the current iterator contains `c`,
    // else false. We do not advance the internal iterator.
    bool
    check(char c) const;

    // Unconditionally advance the internal iterator.
    // Returns the character pointed to before the advance.
    char
    advance();

    // Returns true if `c` was consumed else false.
    bool
    match(char c);

    // Returns true if _either_ `c1` or `c2` was consumed, else false.
    bool
    match_either(char c1, char c2);

    // Returns the number of times we advanced based
    // on the predicate.
    int
    match_func(bool (*predicate)(char c));

    // Returns 0 (no error) if `c` was matched else nonzero.
    Error
    expect(char c);

    bool
    skip_whitespace();

    void
    skip_line();

    bool
    skip_multiline();

    // Returns the current lexeme.
    String
    get_text() const;

    Error
    set_token(Token *t, Token_Kind k) const;

    Token_Kind
    select_pair(char single, Token_Kind pair);

    Token_Kind
    select_assign(char single, Token_Kind assign);

    Token_Kind
    select_pair_assign(char single, Token_Kind pair, Token_Kind assign);

    Token_Kind
    select_compound(
        char single,
        Token_Kind pair,
        Token_Kind assign,
        Token_Kind pair_assign);

    Error
    try_keyword(Token *t);

    Error
    try_number(Token *t, bool is_frac);
};

