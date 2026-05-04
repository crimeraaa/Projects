#pragma once

#include "lexer.hpp"
#include "arena.hpp"

struct Ast;

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

    // func ::= type ident '(' func-args ')' func-body ;
    //
    // func-args ::= ""
    //  | func-arg
    //  | func-args ',' func-arg
    //  ;
    //
    // func-arg ::= type ident ;
    //
    // func-body ::= ';'
    //  | '{' block '}'
    //  ;
    //
    Ast *
    func();

    // type ::= qualifier type ptr-or-ref
    //  | ident
    //  | "bool"
    //  | "int"
    //  | "double"
    //  | "void"
    //  ;
    //
    // qualifier ::= ""
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
};

enum Ast_Kind {
    AST_NONE,
    AST_DECL,

    // Expressions
    AST_PREFIX,
    AST_INFIX,
};

enum Ast_Op {
    AST_OP_NONE,

    // Prefix
    AST_PREFIX_DEREF,   // '*' expr, desugared index expressions

    // Infix: arithmetic
    AST_INFIX_ASSIGN, // expr '=' expr
    AST_INFIX_ADD,  // expr '+' expr, desugared compound assignment rvalue
    AST_INFIX_SUB,  // expr '-' expr, desugared compound assignment rvalue
    AST_INFIX_MUL,  // expr '*' expr, desugared compound assignment rvalue
    AST_INFIX_DIV,  // expr '/' expr, desugared compound assignment rvalue
};

struct Ast_Prefix {
    Ast_Op op;
    Ast *arg;
};

struct Ast_Infix {
    Ast_Op op;
    Ast *left;
    Ast *right;
};

struct Ast {
    Ast_Kind kind;
    union {
        Ast_Prefix prefix;
        Ast_Infix  infix;
    };
};
