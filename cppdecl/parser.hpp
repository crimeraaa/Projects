#pragma once

#include "lexer.hpp"
#include "arena.hpp"

// @link https://en.cppreference.com/c/language/operator_precedence
enum Precedence : u8 {
    PREC_NONE,
    PREC_ASSIGN,    // = += -= *= /= %= <<= >>= &= ^= |=
    PREC_OR,        // ||
    PREC_AND,       // &&
    PREC_BSHIFT,    // << >>
    PREC_TERM,      // + -
    PREC_FACTOR,    // * / %
    PREC_UNARY,     // - * & function calls
};

struct Parser {
private:
    Lexer m_lexer;
    Token m_consumed, m_lookahead;
    // Used to allocate AST nodes.
    Arena *m_arena;

    // Used to indicate to the coller what error occured.
    String *m_message;

public:
    Parser(String input, Arena *arena)
        : m_lexer{Lexer(input)}
        , m_consumed{}
        , m_lookahead{}
        , m_arena{arena}
        , m_message{nullptr}
    {}

    // program ::= defs ;
    //
    // defs ::= ""
    //  | defs def
    //  ;
    //
    Ast *
    program(String *out);

private:
    void
    advance();

    bool
    check(Token_Kind kind) const;

    bool
    match(Token_Kind kind);

    void
    expect(Token_Kind kind);

    void
    error(String message);

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
    expr(Precedence prec = PREC_ASSIGN);

    // unary ::= UNOP expr
    //  ;
    //
    // UNOP ::= '-'
    //  | '*'
    //  | '&'
    //  ;
    Ast *
    unary(Ast_Prefix_Kind kind);

    // binary ::= expr BINOP expr
    //  ;
    //
    // BINOP ::= '+'
    //  | '-'
    //  | '*'
    //  | '/'
    //  ;
    Ast *
    binary(Ast *left, Ast_Infix_Kind kind, Precedence prec);

    struct Rule {
        Precedence left, right;
        Ast_Infix_Kind kind;
    };

    static Rule
    get_rule(Token_Kind kind);
};

