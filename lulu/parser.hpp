#pragma once

#include "lulu.h"
#include "lexer.hpp"
#include "chunk.hpp"
#include "mem.hpp"
#include "expr.hpp"

// If you exceed this, you should probably rethink what you did!
#define PARSER_MAX_RECURSIONS   250

struct ParserData {
    String  path, input;
    Chunk   chunk;
    Scratch scratch;
};


// Defined in `compiler.h`.
struct Compiler;
struct VarInfo;

class Parser {
    // Shared state.
    lulu_State *L;
    Compiler *  compiler;
    String      path;

    // Parser state.
    Lexer lexer;
    Token token;

    // Just in case we want to allocate a message.
    Scratch *scratch = nullptr;

    // Tracked to prevent stack overflow.
    int recursions = 0;

public:
    [[nodiscard]] static Chunk *
    parse(lulu_State *L, ParserData &data);

    [[noreturn]] void
    error_at(char const *info, Loc const &where);

    [[noreturn]] void
    error_at(char const *info, Token const &token)
    {
        this->error_at(info, token.loc);
    }

    [[noreturn]] void
    error_at(char const *info, Expr const &expr)
    {
        this->error_at(info, expr.loc);
    }

    [[noreturn]] void
    error(char const *info)
    {
        this->error_at(info, this->token.loc);
    }
private:
    void
    simple_stmt();

    void
    ident_stmt();
    
    void
    return_stmt();

    void
    decl(ExprList lhs_list);

    void
    assign(ExprList lhs_list);

    [[nodiscard]] ExprList
    primary_expr_list(bool is_lhs);

    [[nodiscard]] ExprList
    expr_list(bool is_lhs = false);

    [[nodiscard]] Expr
    expr(bool is_lhs = false, int prec_in = 1);

    [[nodiscard]] Expr
    unary_expr(bool is_lhs);

    [[nodiscard]] Expr
    primary_expr(bool is_lhs);

    void
    call(Expr *func);

    [[nodiscard]] Expr
    operand(bool is_lhs);

    [[nodiscard]] Expr
    type();

    [[nodiscard]] VarInfo *
    find_variable(String name, u16 *out);

    void
    infer_types(ExprList lhs_list, ExprList rhs_list);

    [[nodiscard]] ExprList
    make_zero_values(Type const *type, int count);

    bool
    check(TokenKind wanted) const noexcept;

    bool
    match(TokenKind wanted) noexcept;

    void
    expect(TokenKind expected);

    void
    advance();

    void
    recurse_push();

    void
    recurse_pop();
};

