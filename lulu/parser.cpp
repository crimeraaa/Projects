#include <stdio.h> // [f]printf

#include "internal.hpp"
#include "lexer.hpp"
#include "slice.hpp"
#include "state.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "expr.hpp"
#include "strings.hpp"
#include "type.hpp"

/*
 Description:
    Parses a single expression of the given precedence. By default, we parse
    the basic precedence which is a good starting point.
 */
static void
parser_expr(Parser *p, Expr *out, bool is_lhs = false, int prec = 1);

static char const *
parser_clamp_string(Slice<char> buf, String s)
{
    usize it = 0;

    // The iteration range is exclusive, so we save the last index for the
    // nul character.
    usize stop = min(len(s), len(buf) - 1);

    // Prefix "..." to indicate that the full string was truncated and that
    // you're seeing only the tail portion that fits.
    if (len(s) > stop) {
        buf[it++] = '.';
        buf[it++] = '.';
        buf[it++] = '.';
    }

    for (; it < stop; it++) {
        buf[it] = s[it];
    }
    buf[it] = 0;
    return buf.data;
}

[[noreturn]] LULU_INTERNAL_FUNC void
parser_error_at(Parser *p, char const *info, Loc const &where)
{
    char name[80];
    char loc[80];
    fprintf(stderr, "%s:%i:%i: %s at '%s'\n",
        parser_clamp_string({name, sizeof(name)}, p->lexer.get_path()),
        where.pos.line, where.pos.col, info,
        parser_clamp_string({loc, sizeof(loc)}, where.view));

    state_throw(p->L, LULU_SYNTAX_ERROR);
}

// Scan a new current token.
static void
parser_advance(Parser *p)
{
    LexerResult result = p->lexer.scan_token();
    // Nonzero error?
    if (result.is_err()) {
        LexerError err = result.unwrap_err();
        parser_error_at(p, lexer_error_string(err.kind), err.loc);
    }
    p->token = result.unwrap();
}

static bool
parser_check(Parser const *p, TokenKind k)
{
    return p->token.kind == k;
}

static bool
parser_match(Parser *p, TokenKind k)
{
    bool found = parser_check(p, k);
    if (found) {
        parser_advance(p);
    }
    return found;
}

static void
parser_expect(Parser *p, TokenKind k)
{
    if (!parser_match(p, k)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected '%s'", token_kind_cstring(k));
        parser_error(p, buf);
    }
}

/*
 TODO(2026-07-20): Add global lookup instead of worrying only about locals.
 */
static VarInfo *
parser_find_variable(Parser *p, String name, u16 *out)
{
    // Loop invariants.
    Compiler *     c      = p->compiler;
    Slice<VarInfo> locals = slice_array(c->active_locals, 0, c->active_locals_len);
    for (VarInfo &v : reverse(locals)) {
        if (name == v.loc.view) {
            if (out) {
                *out = cast(u16)(&v - raw_data(locals));
            }
            return &v;
        }
    }

    if (out) {
        *out = cast(u16)-1;
    }
    return nullptr;
}


/*
 Description:
    'operand' refers to our most basic expression. These are literals,
    identifiers, unary operations, and table constructors.

 Note (2026-07-03):
    This is about the only place `lhs` is useful. It allows us to avoid
    needlessly parsing a compound literal (in our case, a table) when in
    declarations/assignments. See Odin's parser:

    https://github.com/odin-lang/Odin/blob/1007ea278534e037fa586564c91113f5c925c286/src/parser.cpp#L2379
 */
static void
parser_operand(Parser *p, Expr *out, bool is_lhs)
{
    Token token = p->token;
    parser_advance(p);
    switch (token.kind) {
    case Token_nil:     *out = Expr::make_nil (token);                 break;
    case Token_false:   *out = Expr::make_bool(token, false);          break;
    case Token_true:    *out = Expr::make_bool(token, true);           break;
    case Token_Int:     *out = Expr::make_int (token, token.integer);  break;
    case Token_Float:   *out = Expr::make_real(token, token.floating); break;
    case Token_Open_Paren:
        parser_expr(p, out, is_lhs);
        parser_expect(p, Token_Close_Paren);
        break;
    case Token_Open_Curly:
        if (is_lhs) {
            parser_error_token(p, "Cannot assign to a compound literal", token);
        } else {
            parser_error_token(p, "Table constructors not yet supported", token);
        }
        break;
    case Token_Ident: {
        String ident = token.loc.view;
        if (!type_get(p->L, ident)
            .is_some_and([=](Type const *type) {
                *out = Expr::make_type(token, type);
                return true; }))
        {
            u16      i;
            VarInfo *v = parser_find_variable(p, ident, &i);

            /*
             Ensures that, for rvalue expressions, we absolutely have a
             variable to work with.

             For lvalue expressions, however, we can be more lenient.
             Especially for declarations since the variable referred to
             the identifier may not yet exist.
             */
            if (!v && !is_lhs) {
                parser_error_token(p, "Unknown identifier", token);
            }
            *out = Expr::make_local(token, (v) ? v->type : nullptr, i);
        }
        break;
    }
    default:
        parser_error_token(p, "Expected an operand", token);
        break;
    }
}

static void
parser_call(Parser *p, Expr *func)
{
    Expr arg;
    if (!parser_check(p, Token_Close_Paren)) {
        parser_expr(p, &arg);
    }
    parser_expect(p, Token_Close_Paren);
    compiler_call(p->compiler, func, &arg);
}

static void
parser_primary_expr(Parser *p, Expr *out, bool is_lhs)
{
    bool loop = true;
    parser_operand(p, out, is_lhs);
    while (loop) {
        switch (p->token.kind) {
        case Token_Open_Paren:
            // Consume '('.
            parser_advance(p);
            parser_call(p, out);
            break;
        default:
            loop = false;
            break;
        }
        // After the first atom, we are no longer assignable.
        is_lhs = false;
    }
}

/*
 Assumptions:
 1) We are about to consume an identifier.
 */
static void
parser_type(Parser *p, Expr *out)
{
    Token const token = p->token;
    parser_expect(p, Token_Ident);
    if (!type_get(p->L, token.loc.view)
        .is_some_and([p, out, token](Type const *type) {
            *out = Expr::make_type(token, type);
            return true; }))
    {
        parser_error(p, "Unknown type name");
    }
}

// Must be higher than all other precedences in `parser_prec()`.
#define PREC_UNARY 8

/*
 Relevant links:
 1) https://odin-lang.org/docs/overview/#operator-precedence
 */
static int
parser_prec(TokenKind k)
{
    switch (k) {
    case Token_Asterisk:
    case Token_Slash:
    case Token_Percent:
    case Token_Ampersand:     return 7;
    case Token_Plus:
    case Token_Dash:
    case Token_Pipe:
    case Token_Caret:         return 6;
    case Token_Greater_Equal:
    case Token_Less_Than:
    case Token_Greater_Than:
    case Token_Less_Equal:    return 5;
    case Token_Equal_Equal:
    case Token_Tilde_Equal:   return 4;
    case Token_and:           return 3;
    case Token_or:            return 2;
    // Precedence 1 is reserved for the base expression parsing.
    // Precedence 0 indicates we cannot parse an expression with this.
    default:
        return 0;
    }
}

/*
 Assumptions:
 1) All unary operators are right-associative. I.e. `cast(bool)cast(int)x` is
    parsed as `cast(bool)(cast(int)x)` which is the same as `bool(int(x))`.
 */
static void
parser_unary_expr(Parser *p, Expr *out, bool is_lhs)
{
    Token op = p->token;
    switch (op.kind) {
    case Token_cast: {
        parser_advance(p);
        parser_expect(p, Token_Open_Paren);

        Expr type;
        parser_type(p, &type);
        parser_expect(p, Token_Close_Paren);
        parser_expr(p, out, /*is_lhs=*/false, PREC_UNARY);
        compiler_cast(p->compiler, &type, out);
        break;
    }
    case Token_Tilde:
    case Token_Dash:
    case Token_Len:
    case Token_not:
        // Skip the unary operand so the first token of the argument
        // is our current.
        parser_advance(p);
        parser_expr(p, out, is_lhs, PREC_UNARY);
        compiler_unary(p->compiler, op, out);
        break;
    default:
        parser_primary_expr(p, out, is_lhs);
        break;
    }
}

#undef PREC_UNARY

static void
parser_recurse_push(Parser *p)
{
    LULU_ASSERT(p->recursions + 1 < PARSER_MAX_RECURSIONS);
    p->recursions++;
}

static void
parser_recurse_pop(Parser *p)
{
    LULU_ASSERT(p->recursions - 1 >= 0);
    p->recursions--;
}

static void
parser_expr(Parser *p, Expr *out, bool is_lhs, int prec_in)
{
    Expr      rhs;
    Compiler *c = p->compiler;

    parser_recurse_push(p);
    parser_unary_expr(p, out, is_lhs);
    for (;;) {
        Token op       = p->token;
        int   prec_out = parser_prec(op.kind);
        // This also catches tokens that are not binary operators.
        if (prec_out < prec_in) {
            break;
        }

        parser_advance(p);
        if (!out->is_literal()) {
            compiler_expr_any_reg(c, out);
        }

        /*
         Assumptions:
         1) All binary operators are left-associative. We don't have
            exponentiation.
         */
        parser_expr(p, &rhs, /*is_lhs=*/false, prec_out + 1);
        compiler_binary(c, op, out, &rhs);
    }
    parser_recurse_pop(p);
}

/*
 Description:
    Parses a list of comma-separated expressions.
 */
static ExprList
parser_expr_list(Parser *p, bool is_lhs = false)
{
    lulu_State *L = p->L;
    // Compiler   *c = p->compiler;
    Scratch    *x = p->scratch;
    ExprList    list;
    do {
        Expr e;
        parser_expr(p, &e, is_lhs);
        // compiler_expr_next_reg(c, &e);
        list_append(L, &list, x, e);
    } while (parser_match(p, Token_Comma));
    return list;

}

/*
 Description:
    Parses a list of comma-separated primary expressions.
 */
static ExprList
parser_primary_expr_list(Parser *p, bool is_lhs)
{
    lulu_State *L = p->L;
    Scratch    *x = p->scratch;
    ExprList    list;
    do {
        Expr e;
        parser_primary_expr(p, &e, is_lhs);
        list_append(L, &list, x, e);
    } while (parser_match(p, Token_Comma));
    return list;
}

static void
parser_infer_types(Parser *p, ExprList lhs_list, ExprList rhs_list)
{
    // We want to infer the type but we literally don't have anything
    // to infer *from*, e.g. `x:`.
    if (rhs_list.count == 0) {
        // Report the error at the *last* local variable name.
        Expr *last = list_last_elem(lhs_list);
        parser_error_expr(p, "Expected a type after ':'", last);
    }

    if (lhs_list.count != rhs_list.count) {
        Expr *last = list_last_elem(rhs_list);
        parser_error_expr(p, "Mismatched number of expressions", last);
    }

    // Copy over all assigning expression types to the targets so the
    // compiler can see them.
    ExprList tmp = lhs_list;
    for (Expr rhs : rhs_list) {
        Expr &lhs = *tmp++;
        lhs.type = rhs.type;
    }
}

static ExprList
parser_make_zero_values(Parser *p, Type const *t, int count)
{
    Expr zero;
    switch (t->kind) {
    case TypeKind_Basic:
        zero.kind         = Expr_Literal;
        zero.literal_kind = t->basic.kind;
        zero.type         = t;
        switch (zero.literal_kind) {
        case Value_bool:
            zero.set_bool(false);
            zero.loc.view = "false"_s;
            break;
        case Value_int:
            zero.set_intr(0);
            zero.loc.view = "0"_s;
            break;
        case Value_real:
            zero.set_real(0.0);
            zero.loc.view = "0.0"_s;
            break;
        default:
            LULU_PANICF("Unsupported zero type for ValueType(%i)", zero.literal_kind);
            break;
        }
        break;
    default:
        LULU_PANICF("Unsupported zero type for TypeKind(%i)", t->kind);
        break;
    }

    ExprList rhs_list;
    for (int i = 0; i < count; i++) {
        list_append(p->L, &rhs_list, p->scratch, zero);
    }
    return rhs_list;
}

static void
parser_decl(Parser *p, ExprList lhs_list)
{
    Compiler *c = p->compiler;
    compiler_declare_local(c, lhs_list);

    // If we have tokens in between ':' and '=', it must be a type.
    // Note that only one (1) type declaration is allowed, e.g. `x, y: int`
    // and not `x, y: int, real`.
    if (!parser_check(p, Token_Assign)) {
        Expr tmp;
        // This should also catch invalid 'types', like `x: 1`.
        parser_type(p, &tmp);
        for (Expr &lhs : lhs_list) {
            lhs.type = tmp.type;
        }
    }

    // `x := expr` or `x: T = expr`, but not `x: T`.
    ExprList rhs_list;
    if (parser_match(p, Token_Assign)) {
        rhs_list = parser_expr_list(p);
    }

    // We don't have a type, so we need to infer it from the assigning
    // expressions.
    if (!lhs_list->type) {
        parser_infer_types(p, lhs_list, rhs_list);
    }
    else if (rhs_list.count == 0) {
        /*
         We do have a type, but we don't have assigning expressions, e.g.
         `x, y: int`. So we need to cough up an equal-length list of
         zero-valued expressions of the appropriate type.

         Note that, grammar-wise, we only allow one (1) type declaration. This
         means that `x, y: int, int` is invalid and would error out before we
         get to this point. So we can assume that all zero values will be of the
         same type.
         */
        rhs_list = parser_make_zero_values(p, lhs_list->type, lhs_list.count);
    }

    // Temporary until we can figure out how to handle function calls.
    LULU_ASSERT(lhs_list.count == rhs_list.count);
    compiler_define_local(c, lhs_list, rhs_list);
}

static void
parser_assign(Parser *p, ExprList lhs_list)
{
    // Before anything else, ensure our assignment targets actually
    // exist.
    for (Expr &lhs : lhs_list) {
        if (!lhs.type) {
            parser_error_expr(p, "Undeclared variable", &lhs);
        }
    }

    // Note that, unlike declaration-assignments, the number of assignment
    // targets and assigning expressions MUST match.
    ExprList rhs_list = parser_expr_list(p);
    if (lhs_list.count != rhs_list.count) {
        Expr *tail = list_last_elem(rhs_list);
        parser_error_expr(p, "Mismatched number of expressions", tail);
    }
    compiler_assign(p->compiler, lhs_list, rhs_list);
}

/*
 TODO(2026-09-05):
    Allow multiple, comma-separated identifiers in a list?
    E.g. `x, y: T` or `x, y, z := expr1, expr2, expr3`
    or even `x, y, z := f()`.
 */
static void
parser_ident_stmt(Parser *p)
{
    ExprList lhs_list = parser_primary_expr_list(p, /*is_lhs=*/true);
    switch (p->token.kind) {
    case Token_Colon:
        // Consume ':'
        parser_advance(p);
        parser_decl(p, lhs_list);
        break;
    case Token_Assign:
        // Consume '='
        parser_advance(p);
        parser_assign(p, lhs_list);
        break;
    default:
        if (lhs_list.count != 1 || lhs_list->kind != Expr_Call) {
            parser_error_expr(p, "Expected a declaration, assignment, or function call",
                &*lhs_list);
        }
        break;
    }
    mem_scratch_free_all(p->scratch);
}

static void
parser_return_stmt(Parser *p)
{
    // Consume 'return'.
    parser_advance(p);

    ExprList rets = parser_expr_list(p);
    compiler_return(p->compiler, rets);
}

static void
parser_simple_stmt(Parser *p)
{
    switch (p->token.kind) {
    case Token_Ident:  parser_ident_stmt(p);  break;
    case Token_return: parser_return_stmt(p); break;
    default:
        parser_error(p, "Expected a statement");
        break;
    }

    // Optional.
    parser_match(p, Token_Semicol);
}

LULU_INTERNAL_FUNC Chunk *
parser_parse(lulu_State *L, ParserData *data)
{
    Parser   p;
    Compiler c;

    // parser init
    p.L        = L;
    p.compiler = &c;
    p.lexer    = Lexer::make(data->path, data->input);
    p.scratch  = &data->scratch;
    
    // compiler init
    c.L        = L;
    c.parser   = &p;
    c.chunk    = &data->chunk;
    parser_advance(&p);
    while (!parser_check(&p, Token_Eof)) {
        parser_simple_stmt(&p);
    }
    parser_expect(&p, Token_Eof);
    compiler_finish(&c);
    return c.chunk;
}

