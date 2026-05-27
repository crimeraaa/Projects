#pragma once

#include "arena.hpp"
#include "types.hpp"

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
    AST_DEREF,  // '*' expr
    AST_NEG,    // '-' expr
    AST_LEA,    // '&' expr
    AST_NOT,    // '!' expr
    AST_BNOT,   // '~' expr
};

enum Ast_Infix_Kind : u8 {
    AST_ASSIGN, // expr '=' expr
    AST_INDEX,  // expr '[' expr ']'

    // Arithmetic
    AST_ADD, // expr '+' expr
    AST_SUB, // expr '-' expr
    AST_MUL, // expr '*' expr
    AST_DIV, // expr '/' expr
    AST_MOD, // expr '%' expr
};

enum Ast_Literal_Kind : u8 {
    UNTYPED_NONE, UNTYPED_INT, UNTYPED_FLOAT,
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

struct Ast_Literal {
private:
    // Base kind
    Ast_Kind _ = AST_LITERAL;
    Ast_Literal_Kind m_kind;
    union {
        u64 m_int;
        f64 m_float;
    };

public:
    Ast_Literal()
        : m_kind{UNTYPED_NONE}
        , m_int{0}
    {}

    Ast_Literal(u64 i)
        : m_kind{UNTYPED_INT}
        , m_int{i}
    {}

    Ast_Literal(f64 f)
        : m_kind{UNTYPED_FLOAT}
        , m_float{f}
    {}

    Ast_Literal_Kind
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

struct Ast {
private:
    union {
        Ast_Kind    m_kind;
        Ast_Prefix  m_prefix;
        Ast_Infix   m_infix;
        Ast_Literal m_literal;
    };

public:
    Ast()                  : m_kind{AST_NONE} {}
    Ast(Ast_Literal data)  : m_literal{data} {}
    Ast(Ast_Prefix prefix) : m_prefix{prefix} {}
    Ast(Ast_Infix infix)   : m_infix{infix} {}

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

    void
    dump(const int depth = 0) const;
};
