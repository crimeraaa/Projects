#ifndef LULU_COMPILER_H
#define LULU_COMPILER_H

#include "lulu.h"
#include "internal.h"
#include "chunk.h"
#include "parser.h"
#include "expr.h"

#define VARIABLES_MAX_COUNT 0x10
typedef struct Variable Variable;
struct Variable {
    String      name;
    Type const *type;
    int         scope; // 0 indicates global scope.
};

struct Compiler {
    // Shared state.
    lulu_State *L;
    Parser *    parser;

    // Compiler state.
    Chunk *  chunk;
    u16      free_reg;
    u16      active_count;
    Variable variables[VARIABLES_MAX_COUNT];
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
compiler_cast(Compiler *c, Type const *t, Expr *e);

LULU_INTERNAL_FUNC void
compiler_call(Compiler *c, Expr *func, Expr *a);

LULU_INTERNAL_FUNC void
compiler_unary(Compiler *c, Token const *op, Expr *e);

LULU_INTERNAL_FUNC void
compiler_binary(Compiler *c, Token const *op, Expr *lhs, Expr *rhs);

LULU_INTERNAL_FUNC void
compiler_return(Compiler *c, Expr *e);

LULU_INTERNAL_FUNC void
compiler_declare(Compiler *c, Expr *lhs, Expr *rhs);

#endif // !LULU_COMPILER_H
