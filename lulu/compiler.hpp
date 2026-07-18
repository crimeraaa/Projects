#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "chunk.hpp"
#include "parser.hpp"
#include "expr.hpp"

#define VARIABLES_MAX_COUNT 0x10
struct Variable {
    String      name;
    Type const *type;
    int         scope; // 0 indicates global scope.
    bool        is_constant;
};

struct Compiler {
    // Shared state.
    lulu_State *L      = nullptr;
    Parser *    parser = nullptr;

    // Compiler state.
    Chunk *  chunk           = nullptr;
    i32      pc              = 0;
    u32      constants_count = 0;
    u16      free_reg        = 0;
    u16      active_count    = 0;
    Variable variables[VARIABLES_MAX_COUNT];
};

LULU_INTERNAL_FUNC void
compiler_finish(Compiler *c);

LULU_INTERNAL_FUNC u16
compiler_expr_next_reg(Compiler *c, Expr *e);

LULU_INTERNAL_FUNC u16
compiler_expr_any_reg(Compiler *c, Expr *e);

LULU_INTERNAL_FUNC void
compiler_cast(Compiler *c, Type const *t, Expr *e);

LULU_INTERNAL_FUNC void
compiler_call(Compiler *c, Expr *func, Expr *a);

LULU_INTERNAL_FUNC void
compiler_unary(Compiler *c, Token const &op, Expr *e);

LULU_INTERNAL_FUNC void
compiler_binary(Compiler *c,
    Token const &  op,
    Expr *restrict lhs,
    Expr *restrict rhs);

LULU_INTERNAL_FUNC void
compiler_return(Compiler *c, Expr *e);

LULU_INTERNAL_FUNC void
compiler_declare(Compiler *c, Expr *restrict lhs, Expr *restrict rhs);

