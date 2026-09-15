#include "lulu.h"
#include "internal.hpp"
#include "opcode.hpp"
#include "expr.hpp"
#include "checker.hpp"

LULU_INTERNAL_FUNC bool
checker_negate_expr(Expr *e)
{
    bool ok = e->is_literal();
    if (ok) switch (e->get_literal_kind()) {
    case Value_int:  e->literal.set_intr(-e->get_intr()); break;
    case Value_real: e->literal.set_real(-e->get_real()); break;
    default:
        // Can't negate this type!
        ok = false;
        break;
    }
    return ok;
}

LULU_INTERNAL_FUNC Option<intr>
checker_try_get_int(Expr *e, intr min, intr max)
{
    intr imm = 0;
    switch (e->get_literal_kind()) {
    case Value_int:
        imm = e->get_intr();
        break;
    case Value_real: {
        real r = e->get_real();

        // Conversion results in data loss?
        imm = cast(intr)r;
        if (cast(real)imm != r) {
            return false;
        }
        break;
    }
    default:
        return false;
    }

    if (min <= imm && imm <= max) {
        return imm;
    } else {
        return false;
    }
}

LULU_INTERNAL_FUNC bool
checker_cast_literal(Expr *e, ValueKind basic_kind)
{
    switch (e->get_literal_kind()) {
    case Value_bool: {
        bool b = e->get_bool();
        switch (basic_kind) {
        case Value_int:  e->set_intr(cast(intr)b); break;
        case Value_real: e->set_real(cast(real)b); break;
        default:         LULU_UNREACHABLE();       break;
        }
    }
    case Value_int: {
        intr i = e->get_intr();
        switch (basic_kind) {
        case Value_bool: e->set_bool(cast(bool)i); break;
        case Value_real: e->set_real(cast(real)i); break;
        default:         LULU_UNREACHABLE();       break;
        }
    }
    case Value_real: {
        real r = e->get_real();
        switch (basic_kind) {
        case Value_bool: e->set_bool(cast(bool)r); break;
        case Value_int:  e->set_intr(cast(intr)r); break;
        default:         LULU_UNREACHABLE();       break;
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
    LULU_ASSERT(e->is_literal());
    auto src_arg = e->get_literal<Src>();
    auto dst_arg = cast(Dst)src_arg;

    // Conversion results in data loss?
    if (cast(Src)dst_arg != src_arg) {
        return false;
    }

    e->set_literal(dst_arg);
    return true;
}


LULU_INTERNAL_FUNC bool
checker_coerce_rhs(Expr *restrict lhs, Expr *restrict rhs)
{
    // If rhs isn't a literal and it's of the wrong type, then we don't
    // allow it to assign to lhs as implicit casts are error-prone.
    if (!type_is_basic(lhs->type) || !rhs->is_literal()) {
        return false;
    }

    switch (lhs->type_get_basic_kind()) {
    // E.g. `x: intr = 1.0` should succeed, but `x: intr = 1.2` should fail.
    case Value_int:
        if (rhs->literal_kind == Value_real) {
            return checker_coerce_expr<real, intr>(rhs);
        }
        break;

    // E.g. `x: real = 1` should succeed,
    // but  `x: real = 90_071_992_454_740_993` should fail.
    case Value_real:
        if (rhs->literal_kind == Value_int) {
            return checker_coerce_expr<intr, real>(rhs);
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
    ValueKind rhs_kind = rhs->get_literal_kind();
    switch (lhs->get_literal_kind()) {
    case Value_int:
        switch (rhs_kind) {
        case Value_int:  LULU_UNREACHABLE(); break;
        case Value_real: return checker_coerce_expr<real, intr>(rhs);
        default:
            break;
        }
        break;
    case Value_real:
        // TODO(2026-07-08): Do we really want propagation?
        switch (rhs_kind) {
        case Value_int:  return checker_coerce_expr<intr, real>(rhs);
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
    auto lhs_literal = lhs->get_literal<T>();
    auto rhs_literal = rhs->get_literal<T>();
    // Although well-defined for IEEE, it's usually a bad idea regardless.
    if (rhs_literal == 0) {
        return Checker_Divide_By_Zero;
    }
    lhs->set_literal((*op)(lhs_literal, rhs_literal));
    return Checker_Ok;
}

template<class T>
static inline void
checker_arith(T (*op)(T a, T b), Expr *restrict lhs, Expr *restrict rhs)
{
    auto res = (*op)(lhs->get_literal<T>(), lhs->get_literal<T>());
    lhs->set_literal(res);
}

template<class T>
static inline void
checker_compare(bool (*op)(T a, T b), Expr *restrict lhs, Expr *restrict rhs, bool flip)
{
    bool b = (*op)(lhs->get_literal<T>(), rhs->get_literal<T>());
    lhs->set_literal<T>(flip ? !b : b);
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
    if (!(lhs->is_literal() && rhs->is_literal())) {
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
    switch (lhs->get_literal_kind()) {
    case Value_bool:
    {
        bool a = lhs->get_bool();
        bool b = rhs->get_bool();
        switch (op.kind) {
        case Token_Tilde_Equal: lhs->literal.set_bool(a != b); break;
        case Token_Equal_Equal: lhs->literal.set_bool(a == b); break;
        case Token_and:         lhs->literal.set_bool(a && b); break;
        case Token_or:          lhs->literal.set_bool(a || b); break;
        default:
            return Checker_Cannot_Fold;
        }
    } break;
    case Value_int:
        switch (op.kind) {
        // Bitwise
        case Token_Ampersand: checker_arith(num_band<intr>, lhs, rhs); break;
        case Token_Pipe:      checker_arith(num_bor <intr>, lhs, rhs); break;
        case Token_Caret:     checker_arith(num_bxor<intr>, lhs, rhs); break;
        default:
            return checker_fold_binary_literals<intr> (op, lhs, rhs);
        }
        break;

    case Value_real: return checker_fold_binary_literals<real>(op, lhs, rhs);
    default:
        return Checker_Cannot_Fold;
    }
    return Checker_Ok;
}


LULU_INTERNAL_FUNC CheckerBinaryResult
checker_fix_binary(Token const &op, Expr *restrict lhs, Expr *restrict rhs)
{
    // Ensure both arguments are of the same underyling type so that we
    // can dispatch the correct opcodes. Only literals can be coerced.
    if (lhs->is_literal()) {
        // E.g. `1 + x` so we want to coerce `1` to the type of `x`.
        checker_coerce_rhs(rhs, lhs);
    } else if (rhs->is_literal()) {
        checker_coerce_rhs(lhs, rhs);
    }

    // Neither lhs nor rhs are necessarily a literal by this point!
    CheckerBinaryResult r{};

    // The simplest (and strictest!) type-checking possible.
    if (lhs->type != rhs->type) {
        return r;
    }

    switch (op.kind) {
    case Token_Ampersand: r.op = Op_band; break;
    case Token_Pipe:      r.op = Op_bor;  break;
    case Token_Caret:     r.op = Op_bxor; break;
    case Token_Plus:      r.op = Op_add;  break;
    case Token_Dash:      r.op = Op_sub;  break;
    case Token_Asterisk:  r.op = Op_mul;  break;
    case Token_Slash:     r.op = Op_div;  break;
    case Token_Percent:   r.op = Op_mod;  break;
    case Token_Tilde_Equal:
        r.is_not = true;
        [[fallthrough]];
    case Token_Equal_Equal:
        r.is_compare = true;
        r.op         = Op_eq;
        break;

    // x >= y <=> !(x < y)
    case Token_Greater_Equal:
        r.is_not = true;
        [[fallthrough]];
    case Token_Less_Than:
        r.is_compare = true;
        r.op         = Op_lt;
        break;

    // x > y <=> !(x <= y)
    case Token_Greater_Than:
        r.is_not = true;
        [[fallthrough]];
    case Token_Less_Equal:
        r.is_compare = true;
        r.op         = Op_leq;
        break;
    default:
        LULU_UNREACHABLE();
        break;
    }

    r.ok = true;

    // TODO(2026-09-15): Check for literals so we can already tell which kind
    // of addressing mode we need (register, constant, or literal).
    if (lhs->type_is_basic()) switch (lhs->type_get_basic_kind()) {
    case Value_bool:
        // We don't allow ordered comparisons on booleans.
        if (r.op == Op_eq) {
            return r;
        }
        break;
    case Value_real:
        // Don't allow bitwise operations on reals.
        if (!(Op_band <= r.op && r.op <= Op_bxor)) {
            r.op = r.op + (Op_fadd - Op_add);
            return r;
        }
        break;
    case Value_int:
        return r;
    default:
        break;
    }

    r.ok = false;
    return r;
}

LULU_INTERNAL_FUNC CheckerBinaryIResult
checker_fix_arithi(OpCode *op, Expr *restrict lhs, Expr *restrict rhs)
{
    CheckerBinaryIResult r{};

    /*
     Consider the following forms:
     1)   imm  & y <=> y &   imm
     2)   imm  | y <=> y |   imm
     3)   imm  ^ y <=> y ^   imm
     4)   imm  + y <=> y +   imm
     5) (-imm) + y <=> y + (-imm) <=> y - |imm|
     6)   imm  - y <=> imm + (-y)
     7) (-imm) - y <=> -(|imm| + (-y))    <=> -((-y) + |imm|)

     4 can be safely inverted because addition is commutative.
     5 can be safely converted because we can decompose it into a subtraction.
     6 and 7 cannot be converted because they require a negation. It's easier
     just delegate to register-register arithmetic at that point.
     */
    if (lhs->is_literal()) {
        if (*op == Op_subi || *op == Op_fsubi) {
            return r;
        }

        /*
         NOTE(2026-09-11)
            Don't just swap the pointers, swap the contents! We want this
            change to reflect to the caller and their parents as well.
         */
        swap(lhs, rhs);
        r.swapped = true;
    }

    auto o = checker_try_get_int(rhs, -cast(intr)ARG_C.MAX, ARG_C.MAX);
    if (o.is_none()) {
        return r;
    }

    r.imm = o.unwrap();
    switch (*op) {
    case Op_addi:
    case Op_faddi:
        // Assumes that the corresponding sub opcode is 1 above us.
        if (r.imm < 0) {
            r.imm = -r.imm;
            *op   = *op + 1;
        }
        break;
    case Op_subi:
    case Op_fsubi:
        // Assumes that the corresponding add opcode is 1 below us.
        if (r.imm < 0) {
            r.imm = -r.imm;
            *op   = *op - 1;
        }
        break;
    default:
        LULU_UNREACHABLE();
        return r;
    }

    r.ok = true;
    return r;
}

LULU_INTERNAL_FUNC CheckerBinaryIResult
checker_fix_comparei(OpCode *op, Expr *restrict lhs, Expr *restrict rhs, bool *k)
{
    CheckerBinaryIResult r{};
 
    /*
     Consider the following forms:

     1)   imm == y  <=>   y == imm
     2) !(imm == y) <=>   y ~= imm
     3)   imm <  y  <=>   y >  imm  <=> !(y <= imm)
     4) !(imm <  y) <=> !(y >  imm) <=>   y <= imm
     5)   imm <= y  <=>   y >= imm  <=> !(y <  imm)
     6) !(imm <= y) <=> !(y >= imm) <=>   y <  imm

     1) and 2) can remain as-is but we do need to swap them so we can assume
     that `lhs` has a register. However, 2) through 6) require us to swap
     the operands. `Op_[f]lti` becomes `Op_[f]leqi` and vice-versa, while
     `k` gets flipped.
     */
    if (lhs->is_literal()) {
        switch (*op) {
        case Op_eqi:   break;
        case Op_lti:   *op = Op_leqi;  *k = !*k; break;
        case Op_leqi:  *op = Op_lti;   *k = !*k; break;
        case Op_feqi:  break;
        case Op_flti:  *op = Op_fleqi; *k = !*k; break;
        case Op_fleqi: *op = Op_flti;  *k = !*k; break;
        default:
            LULU_PANICF("Invalid immediate comparison OpCode(%i)", *op);
            LULU_UNREACHABLE();
            return {};
        }
        swap(lhs, rhs);
        r.swapped = true;
    }

    if (rhs->is_literal_bool()) {
        r.ok = true;
    } else {
        auto t = checker_try_get_int(rhs, 0, ARG_B.MAX);
        r.ok = t.is_some();
        if (r.ok) {
            r.imm = t.unwrap();
        }
    }
    return r;
}

static TValue
checker_get_constant(Expr *const rhs)
{
    switch (rhs->get_literal_kind()) {
    case Value_int:  return TValue::make_intr(rhs->get_intr());
    case Value_real: return TValue::make_real(rhs->get_real());
    default:
        LULU_PANICF("Unsupported ExprKind(%i) and/or ValueKind(%i)",
            rhs->kind, rhs->literal_kind);
        LULU_UNREACHABLE();
        break;
    }
    return {};
}

LULU_INTERNAL_FUNC CheckerBinaryKResult
checker_fix_arithk(OpCode *op, Expr *restrict lhs, Expr *restrict rhs)
{
    CheckerBinaryKResult r{};
    /*
     Consider the following forms:

     1)    k  & y <=>   y  &   k
     2)    k  | y <=>   y  |   k
     3)    k  ^ y <=>   y  ^   k
     4)    k  + y <=>   y  +   k
     5)  (-k) + y <=>   y  + (-k)
     6)    k  - y <=>   k  + (-y) <=> -(y - k)
     7)  (-k) - y <=> (-k) + (-y) <=> -(y + k)
     8)    k  * y <=>   y  * k
     9)  (-k) * y <=>   y  * (-k)
     10)   k  / y
     11) (-k) / y

     Bitwise (1, 2, and 3), Addition (4 and 5) and Multiplication (8 and 9)
     can be treated commutatively, so we can safely swap the operands. Since
     constants themselves can be negative (i.e. we aren't limited to just
     positives, as in immediates) we don't need to get the absolute value and
     flip the opcode.

     Subtraction is not commutative as it requires an implicit negation, at
     which point it would be easier to use register-register operations.

     Division is similar to Subtraction in that it is not commutative.
     */
    if (lhs->is_literal()) {
        switch (*op) {
        case Op_bandk:
        case Op_bork:
        case Op_bxork:
        case Op_addk:
        case Op_mulk:
        case Op_faddk:
        case Op_fmulk:
            break;
        default:
            return r;
        }
        r.swapped = true;
        swap(lhs, rhs);
    }

    r.constant = checker_get_constant(rhs);
    r.ok       = true;
    return r;
}

LULU_INTERNAL_FUNC CheckerBinaryKResult
checker_fix_comparek(OpCode *op, Expr *restrict lhs, Expr *restrict rhs, bool *k)
{
    CheckerBinaryKResult r{};

    /*
     Consider the following forms:

     1) k == y <=>   y == k  ; *op = Op_[f]eqk,  k = true
     2) k ~= y <=> !(y == k) ; *op = Op_[f]eqk,  k = false
     3) k <  y <=> !(y <= k) ; *op = Op_[f]leqk, k = false
     6) k >= y <=>   y <= k  ; *op = Op_[f]leqk, k = true
     4) k <= y <=> !(y <  k) ; *op = Op_[f]ltk,  k = false
     5) k >  y <=>   y <  k  ; *op = Op_[f]ltk,  k = true
     */
    if (lhs->is_literal()) {
        switch (*op) {
        case Op_eqk:    break;
        case Op_ltk:    *op = Op_leqk;  *k = !*k; break;
        case Op_leqk:   *op = Op_ltk;   *k = !*k; break;
        case Op_feqk:   break;
        case Op_fltk:   *op = Op_fleqk; *k = !*k; break;
        case Op_fleqk:  *op = Op_fltk;  *k = !*k; break;
        default:
            LULU_UNREACHABLE();
            return r;
        }
        r.swapped = true;
        swap(lhs, rhs);
    }

    r.constant = checker_get_constant(rhs);
    r.ok       = true;
    return r;
}

