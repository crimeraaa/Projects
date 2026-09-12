#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "opcode.hpp"
#include "expr.hpp"
#include "lexer.hpp"

enum CheckerError {
    Checker_Ok,
    Checker_Cannot_Fold,
    Checker_Divide_By_Zero,
};

/*
 Description:
    Explicitly casts the given literal expression to some basic type. This
    operation is assumed to never fail- all literals can be (explicitly) cast
    to all other basic types. It is also assumed that the caller does not
    care about data loss since they explicitly specified the cast.

 Returns:
    `true` if the cast was performed successfully. In this case, the given
    expression is also modified in-place. Note that since this never fails,
    the return value is intended only for tail call optimization.
 */
LULU_INTERNAL_FUNC bool
checker_cast_literal(Expr *e, ValueKind basic_kind);

/*
 Description:
    Attempts to coerce (i.e. implicitly cast) the expression on the right
    to the type of the one on the left. Note that we implement a much
    stricter type system, so only literals that can be implicitly converted
    without any loss of information are allowed.

 Returns:
    `true` if the coercion was successful. In this case, the right hand side
    expression was also modified in-place.

    `false` if therwise. The expression will remain unchanged.
 */
LULU_INTERNAL_FUNC bool
checker_coerce_rhs(Expr *restrict lhs, Expr *restrict rhs);

LULU_INTERNAL_FUNC bool
checker_coerce_numeric(Expr *restrict lhs, Expr *restrict rhs);

LULU_INTERNAL_FUNC CheckerError
checker_fold_binary(Token const &op, Expr *restrict lhs, Expr *restrict rhs);

struct CheckerBinaryResult {
    bool   ok;
    OpCode op;
    bool   is_compare;
    bool   is_not;
};

LULU_INTERNAL_FUNC CheckerBinaryResult
checker_fix_binary(Token const &op, Expr *restrict lhs, Expr *restrict rhs);

struct CheckerBinaryIResult {
    bool     ok;
    bool     swapped;
    lulu_int imm;
};

LULU_INTERNAL_FUNC CheckerBinaryIResult
checker_fix_arithi(OpCode *op, Expr *restrict lhs, Expr *restrict rhs);

LULU_INTERNAL_FUNC CheckerBinaryIResult
checker_fix_comparei(OpCode *op, Expr *restrict lhs, Expr *restrict rhs, bool *k);

struct CheckerBinaryKResult {
    bool   ok;
    bool   swapped;
    TValue constant;
};

LULU_INTERNAL_FUNC CheckerBinaryKResult
checker_fix_arithk(OpCode *op, Expr *restrict lhs, Expr *restrict rhs);

LULU_INTERNAL_FUNC CheckerBinaryKResult
checker_fix_comparek(OpCode *op, Expr *restrict lhs, Expr *restrict rhs, bool *k);
