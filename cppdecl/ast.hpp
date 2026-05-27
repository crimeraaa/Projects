#pragma once

#include "arena.hpp"
#include "types.hpp"
#include "string.hpp"

struct Ast;

enum Ast_Kind : u8 {
    AST_NONE,

    // Expressions: Terminals
    AST_LITERAL,
    AST_VARIABLE,

    // Expressions: Non-terminals
    AST_PREFIX,
    AST_INFIX,
};

enum Ast_Prefix_Kind : u8 {
    AST_DEREF = 1,  // '*' expr
    AST_NEG,    // '-' expr
    AST_LEA,    // '&' expr
    AST_NOT,    // '!' expr
    AST_BNOT,   // '~' expr
    AST_INCR,   // "++" expr
    AST_DECR,   // "--" expr
};

enum Ast_Infix_Kind : u8 {
    AST_ASSIGN = 1, // expr '=' expr
    AST_INDEX,      // expr '[' expr ']'

    // Bitwise
    AST_BAND, // expr '&' expr
    AST_BOR,  // expr '|' expr
    AST_BXOR, // expr '^' expr
    AST_SHL,  // expr "<<" expr
    AST_SHR,  // expr ">>" expr

    // Logical
    AST_AND, // expr "&&" expr
    AST_OR,  // expr "||" expr

    // Equality
    AST_EQ, AST_NEQ, // expr "==" | "!=" expr

    // Relational
    AST_LT, AST_LEQ, // expr '<'  | "<=" expr
    AST_GT, AST_GEQ, // expr '>'  | ">=" expr

    // Arithmetic
    AST_ADD, AST_SUB,          // expr '+' | '-' expr
    AST_MUL, AST_DIV, AST_MOD, // expr '*' | '/' | '%' expr
};

struct Ast_Prefix {
    // Base kind
    Ast_Kind _ = AST_PREFIX;
    Ast_Prefix_Kind kind;
    Ast *arg;

    Ast_Prefix(Ast_Prefix_Kind kind, Ast *arg) : kind{kind}, arg{arg} {}
};

struct Ast_Infix {
    // Base kind
    Ast_Kind _ = AST_INFIX;
    Ast_Infix_Kind kind;
    Ast *left;
    Ast *right;

    Ast_Infix(Ast_Infix_Kind kind, Ast *left, Ast *right)
        : kind{kind}
        , left{left}
        , right{right}
    {}
};

enum Ast_Literal_Kind : u8 {
    UNTYPED_NONE, UNTYPED_INT, UNTYPED_FLOAT, UNTYPED_STRING,
};

struct Ast_Literal {
private:
    // Base kind
    Ast_Kind _ = AST_LITERAL;
    Ast_Literal_Kind m_kind;

    // Used only for UNTYPED_STRING. Placed here to save space.
    // We assume that if you have a 4 GB string literal, then you have
    // bigger problems to worry about.
    u32 m_len;
    union {
        u64 m_int;
        f64 m_float;
        const char *m_chars;
    };

public:
    // Needed for Parser's default constructor.
    Ast_Literal()         : m_kind{UNTYPED_NONE},   m_int{0} {}
    Ast_Literal(u64 i)    : m_kind{UNTYPED_INT},    m_int{i} {}
    Ast_Literal(f64 f)    : m_kind{UNTYPED_FLOAT},  m_float{f} {}
    Ast_Literal(String s) : m_kind{UNTYPED_STRING}, m_len{cast(u32)len(s)}, m_chars{raw_data(s)} {}

    Ast_Literal_Kind
    kind() const
    {
        return m_kind;
    }

    u64
    i() const
    {
        assert(m_kind == UNTYPED_INT);
        return m_int;
    }

    f64
    f() const
    {
        assert(m_kind == UNTYPED_FLOAT);
        return m_float;
    }

    String
    s() const
    {
        assert(m_kind == UNTYPED_STRING);
        return {m_chars, cast(size_t)m_len};
    }
};

struct Ast_Variable {
    // Base kind
    Ast_Kind _ = AST_VARIABLE;

    // Scope 0 indicates the global scope.
    // Scope 1 onwards indicates function arguments.
    // Scope 2 onwards indicates futher scoped locals.
    int scope = 0;
    String ident;

    Ast_Variable(String ident, int scope) : scope{scope}, ident{ident} {}
};

struct Ast {
private:
    union {
        Ast_Kind     m_kind;
        Ast_Prefix   m_prefix;
        Ast_Infix    m_infix;
        Ast_Literal  m_literal;
        Ast_Variable m_variable;
    };

public:
    Ast()                      : m_kind{AST_NONE} {}
    Ast(Ast_Literal data)      : m_literal{data} {}
    Ast(Ast_Prefix prefix)     : m_prefix{prefix} {}
    Ast(Ast_Infix infix)       : m_infix{infix} {}
    Ast(Ast_Variable variable) : m_variable{variable} {}

    template<class T, class ...Args>
    static Ast *
    make(Arena *arena, Args ...args)
    {
        Ast *node = cast(Ast *)arena->alloc<T>();
        if (node == nullptr) {
            throw ERR_OUT_OF_MEMORY;
        }
        // Constructor must set the base kind.
        *node = T(args...);
        return node;
    }

    Ast_Kind
    kind() const
    {
        return m_kind;
    }

    Ast_Literal
    literal() const
    {
        assert(m_kind == AST_LITERAL);
        return m_literal;
    }

    const Ast_Prefix *
    prefix() const
    {
        assert(m_kind == AST_PREFIX);
        return &m_prefix;
    }

    const Ast_Infix *
    infix() const
    {
        assert(m_kind == AST_INFIX);
        return &m_infix;
    }

    const Ast_Variable *
    variable() const
    {
        assert(m_kind == AST_VARIABLE);
        return &m_variable;
    }

    void
    dump(const int depth = 0) const;
};
