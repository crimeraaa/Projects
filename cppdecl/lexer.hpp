#pragma once

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

#define x(kw) TK_##kw
    // Terminals: (select) C++ keywords
    KEYWORD_LIST(x),
#undef x

    // Terminals: single characters pairs
    TK_LPAREN,      TK_RPAREN,   // '(' ')'
    TK_LSQUARE,     TK_RSQUARE,  // '[' ']'
    TK_LCURLY,      TK_RCURLY,   // '{' '}'
    TK_COLON,       TK_SCOPE,    // ':' "::"

    // Terminals: bitwise operators
    TK_BNOT,                    // '~'
    TK_BAND,        TK_BOR,     // '&' '|'
    TK_SHL,         TK_SHR,     // "<<" ">>"
    TK_BAND_ASSIGN, TK_BOR_ASSIGN, TK_BXOR_ASSIGN, // "&="  "|=" "^="
    TK_SHL_ASSIGN,  TK_SHR_ASSIGN,                 // "<<=" ">>="

    // Terminals: logical operators
    TK_NOT,                     // '!'
    TK_AND,         TK_OR,      // "&&" "||"

    // Terminals: relational operators
    TK_EQ,  TK_NEQ, // "==" "!="
    TK_LT,  TK_LEQ, // '<' "<="
    TK_GT,  TK_GEQ, // '>' ">="

    // Terminals: arithmetic operators
    TK_INCR,        TK_DECR,                       // "++"  "--"
    TK_ADD,         TK_SUB,                        // '+'   '-'
    TK_MUL,         TK_DIV,        TK_MOD,         // '*'   '/'  '%'
    TK_ADD_ASSIGN,  TK_SUB_ASSIGN,                 // "+="  "-="
    TK_MUL_ASSIGN,  TK_DIV_ASSIGN, TK_MOD_ASSIGN,  // "*="  "/=" "%="

    // Terminals: misc. operators
    TK_ASSIGN,               // '='
    TK_PERIOD, TK_ELLIPSIS,  // '.' "..."
    TK_ARROW,                // ->

    // Terminals: misc. non-operators
    TK_COMMA,        // ','
    TK_SEMICOL,      // ';'
    TK_LITERAL_CHAR, // '\'' char '\''

    // Nonterminals
    TK_LITERAL_INT,
    TK_LITERAL_FLOAT,
    TK_LITERAL_STRING,  // '"' .* '"'
    TK_IDENT, 

    // Misc.
    TK_EOF,
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

    // Tries to match a pair (k1 + c), otherwise falls back to
    // the single charactert token (k1).
    //
    Error
    set_token2(Token *t, char c, Token_Kind k1c, Token_Kind k1);

    // Tries to match a pair (k1 + c), then a compound operator (k1 + '=')
    // in that order. Falls back to the single character token (k1).
    //
    Error
    set_token3(Token *t, char c, Token_Kind k1c, Token_Kind k1eq,
               Token_Kind k1);

    // Tries to match a compound assignment pair (k1 + c + '='),
    // then a pair (k1 + c), then a relational operator (k1 + '='),
    // in that order. Falls back to the single character token (k1).
    //
    Error
    set_token4(Token *t, char c, Token_Kind k1ceq, Token_Kind k1c,
               Token_Kind k1eq, Token_Kind k1);

    Error
    try_keyword(Token *t);

    Error
    try_number(Token *t, bool is_frac);
};

