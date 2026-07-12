#include <stdio.h> // [f]printf

#include "lexer.h"
#include "state.h"
#include "parser.h"
#include "compiler.h"
#include "expr.h"
#include "type.h"

typedef enum Precedence {
    Prec_None, // Zero value. Useful to indicate a rule does not exist.
    Prec_Expr, // Default precedence to parse with.
    Prec_Or,
    Prec_And,
    Prec_Equality,
    Prec_Comparison,
    Prec_Term,      // x {+,-} y
    Prec_Factor,    // x {*,/,%} y
    Prec_Unary,     // {-,#,not} x
    Prec_Call,
} Precedence;

/*
 Description:
    Parses a single expression of the given precedence.
 */
static void
parser_expr_prec(Parser *p, Expr *out, bool lhs, Precedence prec);

/*
 Descripion:
    Parses a single expression of the base precedence until no more
    subexpressions can be found.
 */
static void
parser_expr(Parser *p, Expr *out, bool lhs);

static char const *
parser_clamp_string(char *buf, usize buf_len, String s)
{
    usize it = 0, stop;
    // The iteration range is exclusive, so we save the last index for the
    // nul character.
    stop = (s.len < buf_len - 1) ? s.len : buf_len - 1;
    if (s.len > stop) {
        buf[it++] = '.';
        buf[it++] = '.';
        buf[it++] = '.';
    }

    for (; it < stop; it++) {
        buf[it] = s.data[it];
    }
    buf[it] = 0;
    return buf;
}

static void
parser_error(Parser *p, char const *info)
{
    parser_error_at(p, info, &p->token);
}

LULU_INTERNAL_FUNC void
parser_error_at(Parser *p, char const *info, Token const *t)
{
    char name[80];
    char loc[80];
    fprintf(stderr, "%s:%i:%i: %s at '%s'\n",
        parser_clamp_string(name, sizeof(name), p->lexer.path),
        t->line, t->col, info,
        parser_clamp_string(loc, sizeof(loc), t->lexeme));


    chunk_destroy(p->L, p->compiler->chunk);
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

static Variable *
parser_find_variable(Parser *p, String name, u16 *out)
{
    Compiler *c = p->compiler;
    for (u16 i = c->active_count; i-- > 0;) {
        Variable *v = &c->variables[i];
        if (string_eq(v->name, name)) {
            if (out) *out = i;
            return v;
        }
    }
    if (out) *out = cast(u16)-1;
    return nullptr;
}


/*
NOTE(2026-07-03):
    This is about the only place `lhs` is useful. It allows us to avoid
    needlessly parsing a compound literal (in our case, a table) when in
    declarations/assignments. See Odin's parser:

    https://github.com/odin-lang/Odin/blob/1007ea278534e037fa586564c91113f5c925c286/src/parser.cpp#L2379
 */
static void
parser_operand(Parser *p, Expr *out, bool lhs)
{
    Token token = p->token;
    parser_advance(p);
    switch (token.kind) {
    case Token_nil:     *out = expr_make_nil(&token); break;
    case Token_false:   // fallthrough
    case Token_true:    *out = expr_make_bool(&token); break;
    case Token_Int: {
        lulu_uint tmp;
        if (!lexer_parse_uint(token.lexeme, &tmp)) {
            char const *info = lexer_error_string(LEXER_INVALID_NUMBER);
            parser_error_at(p, info, &token);
        }
        *out = expr_make_uint(&token, tmp);
        break;
    }
    case Token_Float: {
        lulu_real tmp;
        if (!lexer_parse_real(token.lexeme, &tmp)) {
            char const *info = lexer_error_string(LEXER_INVALID_NUMBER);
            parser_error_at(p, info, &token);
        }
        *out = expr_make_real(&token, tmp);
        break;
    }
    case Token_Open_Paren:
        parser_expr(p, out, lhs);
        parser_expect(p, Token_Close_Paren);
        break;
    case Token_Open_Curly:
        if (lhs) {
            parser_error_at(p, "Cannot assign to a compound literal", &token);
        } else {
            parser_error_at(p, "Table constructors not yet supported", &token);
        }
        break;
    case Token_Ident: {
        String      ident = token.lexeme;
        Type const *type  = type_get(p->L, ident);
        if (type) {
            *out = expr_make_type(&token, type);
        } else {
            u16       i;
            Variable *v = parser_find_variable(p, ident, &i);
            // If we're in a declaration or assignment, allow this temporarily.
            // We'll have to check if it anyway.
            if (!v && !lhs) {
                parser_error_at(p, "Unknown identifier", &token);
            }
            *out = expr_make_variable(&token, (v) ? v->type : nullptr, i);
        }
        break;
    }
    default:
        parser_error_at(p, "Expected an operand", &token);
        break;
    }
}

static void
parser_call(Parser *p, Expr *func)
{
    Expr arg = expr_make_none();
    if (!parser_check(p, Token_Close_Paren)) {
        parser_expr(p, &arg, /*lhs=*/false);
    }
    parser_expect(p, Token_Close_Paren);
    compiler_call(p->compiler, func, &arg);
}

static void
parser_primary_expr(Parser *p, Expr *out, bool lhs)
{
    bool loop = true;
    parser_operand(p, out, lhs);
    while (loop) {
        switch (p->token.kind) {
        case Token_Open_Paren:
            parser_advance(p);
            parser_call(p, out);
            break;
        default:
            loop = false;
            break;
        }
        // After the first atom, we are no longer assignable.
        lhs = false;
    }
}

static String
parser_ident(Parser *p)
{
    String ident = p->token.lexeme;
    parser_expect(p, Token_Ident);
    return ident;
}

/*
 Assumptions:
 1) We are about to consume an identifier.
 */
static Type const *
parser_type(Parser *p)
{
    String      ident = parser_ident(p);
    Type const *type  = type_get(p->L, ident);
    if (!type) {
        parser_error(p, "Unknown type name");
    }
    return type;
}

static void
parser_unary_expr(Parser *p, Expr *out, bool lhs)
{
    Token op = p->token;
    switch (op.kind) {
    case Token_cast: {
        parser_advance(p);
        parser_expect(p, Token_Open_Paren);

        Type const *type = parser_type(p);
        parser_expect(p, Token_Close_Paren);

        parser_expr_prec(p, out, /*lhs=*/false, Prec_Unary);
        compiler_cast(p->compiler, type, out);
        break;
    }
    case Token_Sub:
    case Token_Len:
    case Token_not: {
        // Skip the unary operand so the first token of the argument
        // is our current.
        parser_advance(p);
        parser_expr_prec(p, out, lhs, Prec_Unary);
        compiler_unary(p->compiler, &op, out);
        break;
    }
    default:
        parser_primary_expr(p, out, lhs);
        break;
    }
}

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

static Precedence
parser_prec(TokenKind k)
{
    switch (k) {
    case Token_Add:
    case Token_Sub: return Prec_Term;
    case Token_Mul:
    case Token_Div:
    case Token_Mod: return Prec_Factor;
    case Token_Eq:
    case Token_Neq: return Prec_Equality;
    case Token_Lt:
    case Token_Leq:
    case Token_Gt:
    case Token_Geq: return Prec_Comparison;
    case Token_and: return Prec_And;
    case Token_or:  return Prec_Or;
    default:
        break;
    }
    return Prec_None;
}

static void
parser_expr_prec(Parser *p, Expr *out, bool lhs, Precedence prec_in)
{
    Expr      rhs;
    Compiler *c = p->compiler;

    parser_recurse_push(p);
    parser_unary_expr(p, out, lhs);
    for (;;) {
        // This also catches tokens that are not binary operators.
        Token const op       = p->token;
        Precedence  prec_out = parser_prec(op.kind);
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
        parser_expr_prec(p, &rhs, false, prec_out + 1);
        compiler_binary(c, &op, out, &rhs);
    }
    parser_recurse_pop(p);
}

static void
parser_expr(Parser *p, Expr *out, bool lhs)
{
    parser_expr_prec(p, out, lhs, Prec_Expr);
}

static void
parser_stmt_expr(Parser *p)
{
    Expr e;
    parser_expr(p, &e, /*lhs=*/false);
}

static void
parser_decl(Parser *p, Expr *lhs)
{
    // If none provided, then infer from rhs type.
    Type const *type = lhs->type;
    if (lhs->type != nullptr) {
        parser_error_at(p, "Shadowing of variable", &lhs->token);
    }

    if (!parser_check(p, Token_Assign)) {
        type = parser_type(p);
    }

    Expr rhs = expr_make_none();
    if (parser_match(p, Token_Assign)) {
        parser_expr(p, &rhs, /*lhs=*/false);
        if (!type) {
            type = rhs.type;
        }
    } else if (!type) {
        parser_error_at(p, "Expected type or '=' after ':'", &lhs->token);
    }

    lhs->type = type;
    compiler_declare(p->compiler, lhs, &rhs);
}

static void
parser_simple_stmt(Parser *p)
{
    switch (p->token.kind) {
    case Token_Ident: {
        Expr lhs;
        parser_primary_expr(p, &lhs, /*lhs=*/true);
        switch (p->token.kind) {
        case Token_Colon: {
            if (lhs.kind != Expr_Variable) {
                parser_error_at(p, "Cannot assign the expression", &lhs.token);
            }
            parser_advance(p);
            parser_decl(p, &lhs);
            break;
        }
        case Token_Assign:
        default:
            if (lhs.kind != Expr_Call) {
                parser_error_at(p, "Expected a declaration, assignment, or function call", &lhs.token);
            }
            break;
        }
        break;
    }
    default:
        parser_stmt_expr(p);
        break;
    }
    // Optional.
    parser_match(p, Token_Semicol);
}


static Parser
parser_make(lulu_State *L, Compiler *c, ParserData *data)
{
    Parser p = {L, c, lexer_make(data->path, data->input),
        /*token     =*/token_make_none(),
        /*scratch   =*/&data->scratch,
        /*recursions=*/0};

    parser_advance(&p);
    return p;
}

LULU_INTERNAL_FUNC Chunk *
parser_parse(lulu_State *L, ParserData *data)
{
    Parser   p;
    Compiler c;
    p = parser_make(L, &c, data);
    c = compiler_make(L, &p, &data->chunk);
    while (!parser_check(&p, Token_Eof)) {
        parser_simple_stmt(&p);
    }
    compiler_return(&c, nullptr);
    parser_expect(&p, Token_Eof);
    return c.chunk;
}

