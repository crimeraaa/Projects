#pragma once

#include <cstdint>

#include "string.hpp"

using  u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using f64 = double;

enum Error : u8 {
    ERR_NONE,

    // Not necessarily an error!
    ERR_EOF,

    // Lexing and parsing errors.
    ERR_UNEXPECTED_TOKEN,
    ERR_UNTERMINATED_COMMENT,
    ERR_UNTERMINATED_STRING,
    ERR_INVALID_NUMBER,

    // Allocation errors.
    ERR_OUT_OF_MEMORY,
    ERR_INVALID_ALLOCATOR,
};

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
    TK_PAREN_OPEN,  TK_PAREN_CLOSE,                 // '(' ')'
    TK_SQUARE_OPEN, TK_SQUARE_CLOSE,                // '[' ']'
    TK_CURLY_OPEN,  TK_CURLY_CLOSE,                 // '{' '}'
    TK_COLON,       TK_SCOPE,        // ':' "::"

    // Terminals: bitwise operators
    TK_BNOT,                    // '~'
    TK_AMPERSAND,   TK_PIPE,    // '&' '|'
    TK_SHL,         TK_SHR,     // "<<" ">>"
    TK_BAND_ASSIGN, TK_BOR_ASSIGN, TK_BXOR_ASSIGN, // "&="  "|=" "^="
    TK_SHL_ASSIGN,  TK_SHR_ASSIGN,                 // "<<=" ">>="

    // Terminals: logical operators
    TK_NOT,                     // '!'
    TK_AND,         TK_OR,      // "&&" "||"

    // Terminals: relational operators
    TK_EQ,          TK_NEQ,     // "==" "!="
    TK_ANGLE_OPEN,  TK_LEQ,     // '<' "<="
    TK_ANGLE_CLOSE, TK_GEQ,     // '>' ">="

    // Terminals: arithmetic operators
    TK_PLUS,        TK_MINUS,                      // '+'  '-'
    TK_ASTERISK,    TK_SLASH,      TK_PERCENT,     // '*'  '/'  '%'
    TK_INCR,        TK_DECR,                       // "++" "--"
    TK_ADD_ASSIGN,  TK_SUB_ASSIGN,                 // "+="  "-="
    TK_MUL_ASSIGN,  TK_DIV_ASSIGN, TK_MOD_ASSIGN,  // "*="  "/=" "%="

    // Terminals: misc. operators
    TK_ASSIGN,               // '='
    TK_PERIOD, TK_ELLIPSIS,  // '.' "..."
    TK_ARROW,                // ->

    // Terminals: misc. non-operators
    TK_COMMA,        // ','
    TK_SEMICOL,      // ';'
    TK_CHAR_LITERAL, // '\'' char '\''

    // Nonterminals
    TK_LITERAL_INT,
    TK_LITERAL_FLOAT,
    TK_LITERAL_STRING,  // '"' .* '"'
    TK_IDENT, 

    // Misc.
    TK_EOF,
};

enum Untyped_Kind : u8 {
    UNTYPED_NONE, UNTYPED_INT, UNTYPED_FLOAT,
};

struct Untyped {
private:
    Untyped_Kind m_kind;
    union {
        u64 m_int;
        f64 m_float;
    };

public:
    Untyped()
        : m_kind{UNTYPED_NONE}
        , m_int{0}
    {}

    Untyped(u64 i)
        : m_kind{UNTYPED_INT}
        , m_int{i}
    {}

    Untyped(f64 f)
        : m_kind{UNTYPED_FLOAT}
        , m_float{f}
    {}

    Untyped_Kind
    kind() const
    {
        return this->m_kind;
    }

    u64
    i() const
    {
        assert(this->m_kind == UNTYPED_INT);
        return this->m_int;
    }

    f64
    f() const
    {
        assert(this->m_kind == UNTYPED_FLOAT);
        return this->m_float;
    }
};

struct Token {
    Token_Kind kind;
    String text;
    int line, col;
    Untyped data;
};

struct Lexer {
    String input;
    String::const_iterator start, current;
    int line, col;

    Lexer(String input)
        : input{input}
        , start{input.begin()}
        , current{input.begin()}
        , line{1}
        , col{1}
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
    try_number(Token *t);
};

