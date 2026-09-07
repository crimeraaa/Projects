#include "lulu.h"
#include "internal.hpp"
#include "opcode.hpp"
#include "expr.hpp"
#include "checker.hpp"

LULU_INTERNAL_FUNC bool
checker_cast_literal(Expr *e, ValueKind basic_kind)
{
    switch (expr_literal_kind(e)) {
    case Value_bool: {
        bool b = expr_bool(e);
        switch (basic_kind) {
        case Value_int:  expr_set_int (e, cast(lulu_int) b); break;
        case Value_real: expr_set_real(e, cast(lulu_real)b); break;
        default:         LULU_UNREACHABLE();                 break;
        }
    }
    case Value_int: {
        lulu_int i = expr_int(e);
        switch (basic_kind) {
        case Value_bool: expr_set_bool(e, cast(bool)i);      break;
        case Value_real: expr_set_real(e, cast(lulu_real)i); break;
        default:         LULU_UNREACHABLE();                 break;
        }
    }
    case Value_real: {
        lulu_real r = expr_real(e);
        switch (basic_kind) {
        case Value_bool: expr_set_bool(e, cast(bool)r);     break;
        case Value_int:  expr_set_int (e, cast(lulu_int)r); break;
        default:         LULU_UNREACHABLE();                break;
        }
    }
    default:
        LULU_UNREACHABLE();
        break;
    }

    return true;
}

/*
 Description:
    Performs the equivalent of `cast(Dst)e` where `e` is of type `Src`.
    Specifically, it coerces (i.e. implicily casts) the expression from the
    source type to the destination type.

 Returns:
    `true` if the coercion can be performed. In this case, the given expression
    is also modified in-place.

    Otherwise, `false` is returned and the expression remains unchanged.
*/
template<class Src, class Dst>
static bool
checker_coerce_expr(Expr *e)
{
    LULU_ASSERT(expr_is_literal(e));
    auto src_arg = value_get<Src>(e->literal);
    auto dst_arg = cast(Dst)src_arg;

    // Conversion results in data loss?
    if (cast(Src)dst_arg != src_arg) {
        return false;
    }

    expr_set<Dst>(e, dst_arg);
    return true;
}


LULU_INTERNAL_FUNC bool
checker_coerce_rhs(Expr *restrict lhs, Expr *restrict rhs)
{
    // If rhs isn't a literal and it's of the wrong type, then we don't
    // allow it to assign to lhs as implicit casts are error-prone.
    if (!type_is_basic(lhs->type) || !expr_is_literal(rhs)) {
        return false;
    }

    switch (expr_basic_kind(lhs)) {
    // E.g. `x: int = 1.0` should succeed, but `x: int = 1.2` should fail.
    case Value_int:
        if (rhs->literal_kind == Value_real) {
            return checker_coerce_expr<lulu_real, lulu_int>(rhs);
        }
        break;

    // E.g. `x: real = 1` should succeed,
    // but  `x: real = 90_071_992_454_740_993` should fail.
    case Value_real:
        if (rhs->literal_kind == Value_int) {
            return checker_coerce_expr<lulu_int, lulu_real>(rhs);
        }
        break;
    default:
        break;
    }
    return false;
}

// TODO(2026-07-13): Make work with variables paired with literals!
LULU_INTERNAL_FUNC bool
checker_coerce_numeric(Expr *restrict lhs, Expr *restrict rhs)
{
    ValueKind rhs_kind = expr_literal_kind(rhs);
    switch (expr_literal_kind(lhs)) {
    case Value_int:
        switch (rhs_kind) {
        case Value_int:  LULU_UNREACHABLE(); break;
        case Value_real: return checker_coerce_expr<lulu_real, lulu_int>(rhs);
        default:
            break;
        }
        break;
    case Value_real:
        // TODO(2026-07-08): Do we really want propagation?
        switch (rhs_kind) {
        case Value_int:  return checker_coerce_expr<lulu_int, lulu_real>(rhs);
        case Value_real: LULU_UNREACHABLE(); break;
        default:
            break;
        }
        break;
    default:
        return false;
    }
    return false;
}

/*
 Description:
    Of the binary operations on literals of the same numeric type, this is
    the only one that could fail hence we need a separate checker for it.
 */
template<class T>
static inline CheckerError
checker_divmod(T (*op)(T a, T b), Expr *restrict lhs, Expr *restrict rhs)
{
    auto lhs_literal = expr_literal<T>(lhs);
    auto rhs_literal = expr_literal<T>(rhs);
    // Although well-defined for IEEE, it's usually a bad idea regardless.
    if (rhs_literal == 0) {
        return Checker_Divide_By_Zero;
    }
    value_set<T>(&lhs->literal, (*op)(lhs_literal, rhs_literal));
    return Checker_Ok;
}

template<class T>
static inline void
checker_arith(T (*op)(T a, T b), Expr *restrict lhs, Expr *restrict rhs)
{
    auto res = (*op)(expr_literal<T>(lhs), expr_literal<T>(rhs));
    value_set<T>(&lhs->literal, res);
}

template<class T>
static inline void
checker_compare(bool (*op)(T a, T b), Expr *restrict lhs, Expr *restrict rhs, bool flip)
{
    bool b = (*op)(expr_literal<T>(lhs), expr_literal<T>(rhs));
    value_set<T>(&lhs->literal, (flip) ? !b : b);
}

/*
 Assumptions:
 1) Both operands are literal values of the same underlying type.
 */
template<class T>
static CheckerError
checker_fold_binary_literals(Token const &op, Expr *restrict lhs, Expr *restrict rhs)
{
    bool flip = false;
    switch (op.kind) {
    // Arithmetic
    case Token_Plus:          checker_arith (num_add<T>, lhs, rhs); break;
    case Token_Dash:          checker_arith (num_sub<T>, lhs, rhs); break;
    case Token_Asterisk:      checker_arith (num_mul<T>, lhs, rhs); break;
    case Token_Slash:         return checker_divmod(num_div<T>, lhs, rhs);
    case Token_Percent:       return checker_divmod(num_mod<T>, lhs, rhs);

    // Comparison
    case Token_Tilde_Equal:   flip = true; [[fallthrough]];
    case Token_Equal_Equal:   checker_compare(num_eq <T>, lhs, rhs, flip); break;
    case Token_Greater_Equal: flip = true; [[fallthrough]];
    case Token_Less_Than:     checker_compare(num_lt <T>, lhs, rhs, flip); break;
    case Token_Greater_Than:  flip = true; [[fallthrough]];
    case Token_Less_Equal:    checker_compare(num_leq<T>, lhs, rhs, flip); break;
    default:
        return Checker_Cannot_Fold;
    }
    return Checker_Ok;
}

LULU_INTERNAL_FUNC CheckerError
checker_fold_binary(Token const &  op, Expr *restrict lhs, Expr *restrict rhs)
{
    if (!(expr_is_literal(lhs) && expr_is_literal(rhs))) {
        return Checker_Cannot_Fold;
    }

    // CHECK(2026-07-16): Literal is typed or untyped
    LULU_ASSERT(lhs->type != nullptr);
    LULU_ASSERT(rhs->type != nullptr);
    if (lhs->type != rhs->type) {
        if (!checker_coerce_numeric(lhs, rhs)) {
            return Checker_Cannot_Fold;
        }
    }

    /*
     NOTE(2026-09-05):
        If you think the following code is atrocious, just imagine how bad it
        could be WITHOUT templates!
     */
    switch (expr_literal_kind(lhs)) {
    case Value_bool: {
        bool a    = expr_bool(lhs);
        bool b    = expr_bool(rhs);
        bool flip = false;
        switch (op.kind) {
        case Token_Tilde_Equal: flip = true; [[fallthrough]];
        case Token_Equal_Equal: value_set_bool(&lhs->literal, flip ? (a != b) : (a == b)); break;
        case Token_and:         value_set_bool(&lhs->literal, a && b); break;
        case Token_or:          value_set_bool(&lhs->literal, a || b); break;
        default:
            return Checker_Cannot_Fold;
        }
        break;
    }
    case Value_int:
        switch (op.kind) {
        // Bitwise
        case Token_Ampersand: checker_arith(num_band<lulu_int>, lhs, rhs); break;
        case Token_Pipe:      checker_arith(num_bor <lulu_int>, lhs, rhs); break;
        case Token_Caret:     checker_arith(num_bxor<lulu_int>, lhs, rhs); break;
        default:
            return checker_fold_binary_literals<lulu_int> (op, lhs, rhs);
        }
        break;

    case Value_real: return checker_fold_binary_literals<lulu_real>(op, lhs, rhs);
    default:
        return Checker_Cannot_Fold;
    }
    return Checker_Ok;
}


LULU_INTERNAL_FUNC CheckerBinary
checker_fix_binary(Token const &op, Expr *restrict lhs, Expr *restrict rhs)
{
    // Ensure both arguments are of the same underyling type so that we
    // can dispatch the correct opcodes. Only literals can be coerced.
    if (expr_is_literal(lhs)) {
        // E.g. `1 + x` so we want to coerce `1` to the type of `x`.
        checker_coerce_rhs(rhs, lhs);
    } else if (expr_is_literal(rhs)) {
        checker_coerce_rhs(lhs, rhs);
    }

    if (lhs->type != rhs->type) {
        return {};
    }

    // Neither lhs nor rhs are necessarily a literal by this point!
    CheckerBinary b{};
    switch (op.kind) {
    case Token_Ampersand: b.opcode = Op_band; break;
    case Token_Pipe:      b.opcode = Op_bor;  break;
    case Token_Caret:     b.opcode = Op_bxor; break;
    case Token_Plus:      b.opcode = Op_add;  break;
    case Token_Dash:      b.opcode = Op_sub;  break;
    case Token_Asterisk:  b.opcode = Op_mul;  break;
    case Token_Slash:     b.opcode = Op_div;  break;
    case Token_Percent:   b.opcode = Op_mod;  break;
    case Token_Tilde_Equal:
        b.is_not = true;
        [[fallthrough]];
    case Token_Equal_Equal:
        b.is_compare = true;
        b.opcode     = Op_eq;
        break;

    // x >= y <=> !(x < y)
    case Token_Greater_Equal:
        b.is_not = true;
        [[fallthrough]];
    case Token_Less_Than:
        b.is_compare = true;
        b.opcode     = Op_lt;
        break;

    // x > y <=> !(x <= y)
    case Token_Greater_Than:
        b.is_not = true;
        [[fallthrough]];
    case Token_Less_Equal:
        b.is_compare = true;
        b.opcode     = Op_leq;
        break;
    default:
        LULU_UNREACHABLE();
        break;
    }

    b.ok = true;
    if (type_is_basic(lhs->type)) switch (expr_basic_kind(lhs)) {
    case Value_bool:
        // We don't allow ordered comparisons on booleans.
        if (b.opcode == Op_eq) {
            return b;
        }
        break;
    case Value_real:
        // Don't allow bitwise operations on reals.
        if (Op_add <= b.opcode && b.opcode <= Op_leq) {
            b.opcode = b.opcode + (Op_fadd - Op_add);
            return b;
        }
        break;
    case Value_int:
        return b;
    default:
        break;
    }

    b.ok = false;
    return b;
}
