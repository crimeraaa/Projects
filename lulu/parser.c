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

static const char *
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

LULU_INTERNAL_FUNC void
parser_error(Parser *p, const char *info)
{
    parser_error_at(p, info, &p->token);
}

LULU_INTERNAL_FUNC void
parser_error_at(Parser *p, const char *info, const Token *t)
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
parser_check(const Parser *p, TokenKind k)
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
            const char *info = lexer_error_string(LEXER_INVALID_NUMBER);
            parser_error_at(p, info, &token);
        }
        *out = expr_make_uint(&token, tmp);
        break;
    }
    case Token_Float: {
        lulu_real tmp;
        if (!lexer_parse_real(token.lexeme, &tmp)) {
            const char *info = lexer_error_string(LEXER_INVALID_NUMBER);
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
    case Token_Ident:
        parser_error_at(p, "Variables not yet supported", &token);
        // operand = expr_make_ident(token.lexeme);
        break;
    default:
        parser_error_at(p, "Expected an operand", &token);
        break;
    }
}

static void
parser_atom_expr(Parser *p, Expr *operand, bool lhs)
{
#if 0
    bool loop = true;
    while (loop) {
        switch (p->token.kind) {
        case Token_Open_Paren:
            parser_call(p, operand);
            break;
        default:
            loop = false;
            break;
        }
        // After the first atom, we are no longer assignable.
        lhs = false;
    }
#endif
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
static const Type *
parser_type(Parser *p)
{
    String      ident = parser_ident(p);
    const Type *type  = type_get(p->L, ident);
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

        const Type *type = parser_type(p);
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
        parser_operand  (p, out, lhs);
        parser_atom_expr(p, out, lhs);
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
parser_expr_prec(Parser *p, Expr *lhs_out, bool lhs, Precedence prec_in)
{
    Expr      rhs_expr;
    Compiler *c = p->compiler;

    parser_recurse_push(p);
    parser_unary_expr(p, lhs_out, lhs);
    for (;;) {
        // This also catches tokens that are not binary operators.
        const Token op       = p->token;
        Precedence  prec_out = parser_prec(op.kind);
        if (prec_out < prec_in) {
            break;
        }

        parser_advance(p);
#if PARSER_CONSTANT_FOLDING
        if (!expr_is_literal(lhs_out)) {
            compiler_expr_any_reg(c, lhs_out);
        }
#else
        compiler_expr_any_reg(c, lhs_out);
#endif
        parser_expr_prec(p, &rhs_expr, false, prec_out + 1);
        compiler_binary(c, &op, lhs_out, &rhs_expr);
    }
    parser_recurse_pop(p);
}

static void
parser_expr(Parser *p, Expr *out, bool lhs)
{
    parser_expr_prec(p, out, lhs, Prec_Expr);
}

static void
parser_assign(Parser *p)
{
    Expr lhs;
    parser_expr(p, &lhs, /*lhs=*/true);
    compiler_return(p->compiler, &lhs);
#if 0
    switch (p->token.kind) {
    case Token_Colon:
    case Token_Assign:
    default:
        parser_error(p, "Unassignable expression");
        break;
    }
#endif
}

static void
parser_stmt_expr(Parser *p)
{
    Expr e;
    parser_expr(p, &e, /*lhs=*/false);
    compiler_return(p->compiler, &e);
}

static void
parser_simple_stmt(Parser *p)
{
    switch (p->token.kind) {
    case Token_Ident:
        parser_assign(p);
        break;
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
    parser_simple_stmt(&p);
    parser_expect(&p, Token_Eof);
    return c.chunk;
}

