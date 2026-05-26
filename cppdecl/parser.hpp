#pragma once

#include "lexer.hpp"
#include "arena.hpp"

struct Ast;

enum Ast_Kind {
    AST_NONE,
    AST_DECL,

    // Expressions: Terminals
    AST_LITERAL,
    AST_VARIABLE,

    // Expressions: Non-terminals
    AST_PREFIX,
    AST_INFIX,
};

enum Ast_Prefix_Kind {
    AST_DEREF,  // '*' expr
    AST_NEG,    // '-' expr
    AST_ADDR,   // '&' expr
};

enum Ast_Infix_Kind : u8 {
    AST_ASSIGN, // expr '=' expr
    AST_INDEX,  // expr '[' expr ']'
    AST_ADD,    // expr '+' expr
    AST_SUB,    // expr '-' expr
    AST_MUL,    // expr '*' expr
    AST_DIV,    // expr '/' expr
};

enum Precedence : u8 {
    PREC_NONE,
    PREC_TERM,   // + -
    PREC_FACTOR, // * / %
    PREC_CALL,   // function calls
    PREC_UNARY,  // - * &
};

struct Parser {
    Lexer lexer;
    Token consumed, lookahead;
    Ast *root;

    // Used to allocate AST nodes.
    Arena *arena;

    Parser(String input, Arena *arena)
        : lexer{Lexer(input)}
        , consumed{}
        , lookahead{}
        , root{nullptr}
        , arena{arena}
    {}

    // program ::= defs ;
    //
    // defs ::= ""
    //  | defs def
    //  ;
    //
    Ast *
    program();

private:
    Error
    advance();

    // def ::= func
    //  | top-level-stmt ';'
    //  ;
    //
    // top-level-stmt ::=
    //  | struct-decl
    //  | typedef-stmt
    //  | var-decl
    //  | using-stmt
    //  ;
    //
    Ast *
    def();

    // func ::= type IDENT '(' func-args ')' func-body ;
    //
    // func-args ::= ""
    //  | func-arg
    //  | func-args ',' func-arg
    //  ;
    //
    // func-arg ::= type IDENT ;
    //
    // func-body ::= ';'
    //  | '{' block '}'
    //  ;
    //
    Ast *
    func();

    // type ::= QUALIFIER type ptr-or-ref
    //  | IDENT
    //  | "bool"
    //  | "int"
    //  | "double"
    //  | "void"
    //  ;
    //
    // QUALIFIER ::= ""
    //  | "const"
    //  ;
    //
    // ptr-or-ref ::= ""
    //  | '*'
    //  | '&'
    //  ;
    //
    Ast *
    type();

    // block ::= '{' stmts ''}'
    //
    // stmts ::= ""
    //  | stmt ';'
    //  | stmts stmt
    //  ;
    //
    Ast *
    block();


    // expr ::= literal
    //
    // literal ::= INT
    //  | FLOAT
    //  ;
    //
    // INT   ::= prefix digits
    //  ;
    //
    // FLOAT ::= digits '.' digits exponent
    //  ;
    //
    // exponent ::= 'e' sign digits
    // sign ::= '+'
    //  | '-'
    //  | ""
    //  ;
    //
    // prefix ::= '0' [bBdDoOxX]
    //  | ""
    //  ;
    //
    // digits ::= digit digits
    //  | ""
    //  ;
    //
    // one_nine ::= [1-9]
    // digit ::= [0-9]
    Ast *
    expr(Precedence prec = PREC_NONE);

    // unary ::= unary_op expr
    //  ;
    //
    // unary_op ::= '-'
    //  | '*'
    //  | '&'
    //  ;
    Ast *
    unary(Ast_Prefix_Kind kind);

    Ast *
    arith(Ast *left, Ast_Infix_Kind kind, Precedence prec);

    struct Rule {
        Precedence left, right;
        Ast_Infix_Kind kind;
        Ast *(Parser::*infix)(Ast *left, Ast_Infix_Kind kind, Precedence prec);
    };

    static Rule
    get_rule(Token_Kind kind);
};


struct Ast_Prefix {
    Ast_Prefix_Kind kind;
    Ast *arg;
};

struct Ast_Infix {
    Ast_Infix_Kind kind;
    Ast *left;
    Ast *right;
};

struct Ast {
private:
    Ast_Kind m_kind;
    union {
        Ast_Prefix  m_prefix;
        Ast_Infix   m_infix;
        Untyped     m_literal;
    };

public:
    Ast()
        : m_kind{AST_NONE}
        , m_literal{}
    {}

    Ast(Untyped data)
        : m_kind{AST_LITERAL}
        , m_literal{data}
    {}

    Ast(Ast_Prefix prefix)
        : m_kind{AST_PREFIX}
        , m_prefix{prefix}
    {}

    Ast(Ast_Infix infix)
        : m_kind{AST_INFIX}
        , m_infix{infix}
    {}

    Ast_Kind
    kind() const
    {
        return this->m_kind;
    }

    Untyped
    literal() const
    {
        assert(this->m_kind == AST_LITERAL);
        return this->m_literal;
    }

    const Ast_Prefix *
    prefix() const
    {
        assert(this->m_kind == AST_PREFIX);
        return &this->m_prefix;
    }

    const Ast_Infix *
    infix() const
    {
        assert(this->m_kind == AST_INFIX);
        return &this->m_infix;
    }

    void
    dump(const int depth = 0) const;
};
