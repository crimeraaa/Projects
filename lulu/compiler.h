#ifndef LULU_COMPILER_H
#define LULU_COMPILER_H

#include "lulu.h"
#include "internal.h"
#include "chunk.h"
#include "parser.h"
#include "expr.h"

struct Compiler {
    // Shared state.
    lulu_State *L;
    Parser *    parser;

    // Compiler state.
    Chunk *chunk;
    u16    free_reg;
    u16    active_count;
};

static inline Compiler
compiler_make(lulu_State *L, Parser *p, Chunk *chunk)
{
    Compiler c = {L, p, chunk, /*free_reg=*/0, /*active_count=*/0};
    return c;
}

LULU_INTERNAL_FUNC u16
compiler_expr_next_reg(Compiler *c, Expr *e);

LULU_INTERNAL_FUNC u16
compiler_expr_any_reg(Compiler *c, Expr *e);

LULU_INTERNAL_FUNC void
compiler_cast(Compiler *c, const Type *t, Expr *e);

LULU_INTERNAL_FUNC void
compiler_unary(Compiler *c, const Token *op, Expr *e);

LULU_INTERNAL_FUNC void
compiler_binary(Compiler *c, const Token *op, Expr *lhs, Expr *rhs);

LULU_INTERNAL_FUNC void
compiler_return(Compiler *c, Expr *e);

#endif // !LULU_COMPILER_H
