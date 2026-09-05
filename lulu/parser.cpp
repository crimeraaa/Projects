#include <stdio.h> // [f]printf

#include "lexer.hpp"
#include "slice.hpp"
#include "state.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "expr.hpp"
#include "type.hpp"

/*
 Description:
    Parses a single expression of the given precedence. By default, we parse
    the basic precedence which is a good starting point.
 */
static void
parser_expr(Parser *p, Expr *out, bool is_lhs, int prec = 1);

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

[[noreturn]] static void
parser_error(Parser *p, char const *info)
{
    parser_error_at(p, info, p->token);
}

LULU_INTERNAL_FUNC [[noreturn]] void
parser_error_at(Parser *p, char const *info, Token const &t)
{
    char name[80];
    char loc[80];
    fprintf(stderr, "%s:%i:%i: %s at '%s'\n",
        parser_clamp_string({name, sizeof(name)}, p->lexer.path),
        t.line, t.col, info,
        parser_clamp_string({loc, sizeof(loc)}, t.lexeme));

    state_throw(p->L, LULU_SYNTAX_ERROR);
}

// Scan a new current token.
static void
parser_advance(Parser *p)
{
    LexerError err = lexer_scan_token(&p->lexer, &p->token);
    if (err) {
        parser_error(p, lexer_error_string(err));
    }
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
    Compiler *c      = p->compiler;
    auto      locals = slice_array(c->locals, 0, c->active_count);
    for (VarInfo &v : reverse(locals)) {
        if (name == v.token.lexeme) {
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
    case Token_nil:     *out = expr_make_nil (token);        break;
    case Token_false:   *out = expr_make_bool(token, false); break;
    case Token_true:    *out = expr_make_bool(token, true);  break;
    case Token_Int: {
        lulu_int   tmp = 0;
        LexerError err = lexer_parse_int(token.lexeme, &tmp);
        if (err) {
            char const *info = lexer_error_string(err);
            parser_error_at(p, info, token);
        }
        *out = expr_make_int(token, tmp);
        break;
    }
    case Token_Float: {
        lulu_real  tmp = 0;
        LexerError err = lexer_parse_real(token.lexeme, &tmp);
        if (err) {
            char const *info = lexer_error_string(err);
            parser_error_at(p, info, token);
        }
        *out = expr_make_real(token, tmp);
        break;
    }
    case Token_Open_Paren:
        parser_expr(p, out, is_lhs);
        parser_expect(p, Token_Close_Paren);
        break;
    case Token_Open_Curly:
        if (is_lhs) {
            parser_error_at(p, "Cannot assign to a compound literal", token);
        } else {
            parser_error_at(p, "Table constructors not yet supported", token);
        }
        break;
    case Token_Ident: {
        String      ident = token.lexeme;
        Type const *type  = type_get(p->L, ident);
        if (type) {
            *out = expr_make_type(token, type);
        } else {
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
                parser_error_at(p, "Unknown identifier", token);
            }
            *out = expr_make_local(token, (v) ? v->type : nullptr, i);
        }
        break;
    }
    default:
        parser_error_at(p, "Expected an operand", token);
        break;
    }
}

static void
parser_call(Parser *p, Expr *func)
{
    Expr arg;
    if (!parser_check(p, Token_Close_Paren)) {
        parser_expr(p, &arg, /*is_lhs=*/false);
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
    Token const  token = p->token;
    parser_expect(p, Token_Ident);

    Type const *type = type_get(p->L, token.lexeme);
    if (!type) {
        parser_error(p, "Unknown type name");
    }
    *out = expr_make_type(token, type);
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
    case Token_not: {
        // Skip the unary operand so the first token of the argument
        // is our current.
        parser_advance(p);
        parser_expr(p, out, is_lhs, PREC_UNARY);
        compiler_unary(p->compiler, op, out);
        break;
    }
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
        // This also catches tokens that are not binary operators.
        Token op       = p->token;
        int   prec_out = parser_prec(op.kind);
        if (prec_out < prec_in) {
            break;
        }

        parser_advance(p);
#if PARSER_CONSTANT_FOLDING
        if (!expr_is_literal(out)) {
            compiler_expr_any_reg(c, out);
        }
#else
        compiler_expr_any_reg(c, out);
#endif
        /*
         Assumptions:
         1) All binary operators are left-associative. We don't have
            exponentiation.
         */
        parser_expr(p, &rhs, false, prec_out + 1);
        compiler_binary(c, op, out, &rhs);
    }
    parser_recurse_pop(p);
}

static ExprList *
parser_expr_list(Parser *p, bool is_lhs)
{
    ExprList   *tail  = nullptr;
    lulu_State *L     = p->L;
    u16         count = 0;
    do {
        ExprList *node = mem_arena_alloc<ExprList>(L);
        *node = {/*prev=*/tail, /*expr=*/{}};
        parser_expr(p, &node->expr, is_lhs);
        node->expr.count = ++count;
        tail = node;
    } while (parser_match(p, Token_Comma));
    return tail;

}

static ExprList *
parser_primary_expr_list(Parser *p, bool is_lhs)
{
    ExprList   *tail  = nullptr;
    lulu_State *L     = p->L;
    u16         count = 0;
    do {
        ExprList *node = mem_arena_alloc<ExprList>(L);
        *node = {/*prev=*/tail, /*expr=*/{}};
        parser_primary_expr(p, &node->expr, is_lhs);
        node->expr.count = ++count;
        tail = node;
    } while (parser_match(p, Token_Comma));
    return tail;
}

static void
parser_decl(Parser *p, DeclInfo *info)
{
    Compiler *c = p->compiler;
    compiler_declare_local(c, info->lhs);

    // If we have tokens in between ':' and '=', it must be a type.
    if (!parser_check(p, Token_Assign)) {
        Expr tmp;
        parser_type(p, &tmp);
        info->type = tmp.type;
    }

    // `x := expr` or `x: T = expr`, but not `x: T`.
    if (parser_match(p, Token_Assign)) {
        info->rhs = parser_expr_list(p, /*is_lhs=*/false);
        // Infer the type we wish to assign with.
        if (!info->type) {
            // E.g. `x:` or `x: 1`, report the error at the last assigning token.
            if (info->rhs->expr.count == 0) {
                Expr *lhs = &info->lhs->expr;
                parser_error_at(p, "Expected a type after ':'", lhs->token);
            }

            // TODO(2026-09-06): Change this, will break when multiple
            // assignments are introduced.
            info->type = info->rhs->expr.type;
        }

    }
    compiler_define_local(c, info);
}

static void
parser_assign(Parser *p, DeclInfo *info)
{
    // Before anything else, ensure our assignment targets actually
    // exist.
    for (ExprList *list = info->lhs; list != nullptr; list = list->prev) {
        Expr *lhs = &list->expr;
        if (!lhs->type) {
            parser_error_at(p, "Undeclared variable", lhs->token);
        }
    }

    info->rhs = parser_expr_list(p, /*is_lhs=*/false);
    compiler_assign(p->compiler, info);
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
    DeclInfo info;

    info.lhs = parser_primary_expr_list(p, /*is_lhs=*/true);
    switch (p->token.kind) {
    case Token_Colon:
        // Consume ':'
        parser_advance(p);
        parser_decl(p, &info);
        break;
    case Token_Assign:
        // Consume '='
        parser_advance(p);
        parser_assign(p, &info);
        break;
    default:
        if (info.lhs->expr.kind != Expr_Call) {
            parser_error_at(p, "Expected a declaration, assignment, or function call",
                info.lhs->expr.token);
        }
        break;
    }
    mem_arena_free_all(&p->L->arena);
}

static void
parser_return_stmt(Parser *p)
{
    Expr tmp;
    // Consume 'return'.
    parser_advance(p);
    parser_expr(p, &tmp, /*is_lhs=*/false);
    compiler_return1(p->compiler, &tmp);
    compiler_expr_pop(p->compiler, &tmp);
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
    p.L           = L;
    p.compiler    = &c;
    p.lexer.path  = data->path;
    p.lexer.input = data->input;
    p.lexer.line  = 1;
    p.lexer.col   = 1;
    p.scratch     = &data->scratch;
    
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

