#include <inttypes.h>   // PRI*
#include <stdio.h>      // [f]printf

#include "state.h"
#include "parser.h"

enum Precedence {
    PREC_NONE,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,      // x {+,-} y
    PREC_FACTOR,    // x {*,/,%} y
    PREC_EXPONENT,  // x ^ y
    PREC_UNARY,     // {-,#,not} x
    PREC_CALL,
};

typedef enum Precedence Precedence;

// Rules for infix parsing.
typedef struct Parser_Rule Parser_Rule;
struct Parser_Rule {
    u8 left_prec;  // (Child) Determines when to yield our recursive descent.
    u8 right_prec; // (Parent) Tells the child node when to yield.
};

static Parser_Rule
parser_match_rule(Token_Kind k);

static Ast_Node *
parser_expression(Parser *p, Precedence prec);

// Because C is stupid and doesn't have default arguments
static Ast_Node *
parser_expression0(Parser *p);

static const char *
parser_clamp_string(char *buf, size_t buf_len, String s)
{
    size_t it = 0, stop;
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
parser_error_at(const Parser *p, const char *info, const Token *t)
{
    char name[80];
    char loc[80];
    fprintf(stderr, "%s:%i:%i: %s at '%s'\n",
        parser_clamp_string(name, sizeof(name), p->lexer.path),
        t->line, t->col, info,
        parser_clamp_string(loc, sizeof(loc), t->lexeme));

    state_throw(p->L, LULU_SYNTAX_ERROR);
}

// The most common case is to error at the current token.
static void
parser_error(const Parser *p, const char *info)
{
    parser_error_at(p, info, &p->token);
}

static void
parser_error_consumed(const Parser *p, const char *info)
{
    parser_error_at(p, info, &p->consumed);
}

static void
parser_advance(Parser *p)
{
    Lexer_Error err;
    p->consumed = p->token;
    err = lexer_scan_token(&p->lexer, &p->token);
    if (err) {
        parser_error(p, lexer_error_string(err));
    }
}

static Token
token_make_none(void)
{
    static const Token t = {TOKEN_NONE,
        /*lexeme=*/NULL, /*len=*/0,
        /*line  =*/0,    /*col=*/0};
    return t;
}

static bool
parser_check(const Parser *p, Token_Kind k)
{
    return p->token.kind == k;
}

static bool
parser_match(Parser *p, Token_Kind k)
{
    bool found = parser_check(p, k);
    if (found) {
        parser_advance(p);
    }
    return found;
}

static void
parser_expect(Parser *p, Token_Kind k)
{
    if (!parser_match(p, k)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected '%s'", token_cstring(k));
        // If error was set (e.g. by the advance), then use it.
        // Otherwise all we know is that we have a token we don't want.
        parser_error(p, buf);
    }
}

static Ast_Node *
parser_binary(Parser *p, Precedence right_prec, Ast_Node *left)
{
    Token op;
    Ast_Node *right;

    // Copy by value as the nonlocal state will update.
    op    = p->consumed;
    right = parser_expression(p, right_prec);
    return ast_binary_new(p->L, &op, left, right);
}

static Ast_Node *
parser_expression(Parser *p, Precedence prec)
{
    lulu_State *L;
    Token prefix;
    Ast_Node *expr = NULL;

    // Consume the first token of the prefix expression.
    parser_advance(p);
    L      = p->L;
    prefix = p->consumed;
    switch (prefix.kind) {
    case TOKEN_INT:
    case TOKEN_FLOAT:
    case TOKEN_FALSE:
    case TOKEN_TRUE:
        expr = ast_literal_new(L, &prefix);
        if (expr->literal.value.kind == VALUE_NONE) {
            parser_error_at(p,
                lexer_error_string(LEXER_INVALID_NUMBER),
                &prefix);
        }
        break;
    case TOKEN_SUB:
    case TOKEN_LEN:
    case TOKEN_NOT:
        expr = parser_expression(p, PREC_UNARY);
        expr = ast_unary_new(L, &prefix, expr);
        break;
    case TOKEN_OPEN_PAREN:
        expr = parser_expression0(p);
        parser_expect(p, TOKEN_CLOSE_PAREN);
        expr = ast_paren_new(L, &prefix, &p->consumed, expr);
        break;
    case TOKEN_IDENT:
        expr = ast_ident_new(L, &prefix);
        break;
    default:
        parser_error_consumed(p, "Expected an expression");
        break;
    }

    // Infix
    for (;;) {
        Parser_Rule rule = parser_match_rule(p->token.kind);
        if (!rule.left_prec || cast(Precedence)rule.left_prec < prec) {
            break;
        }

        // Consume the infix token.
        parser_advance(p);
        expr = parser_binary(p, cast(Precedence)rule.right_prec, expr);
    }

    // Postfix
    return expr;
}

static Ast_Node *
parser_expression0(Parser *p)
{
    return parser_expression(p, PREC_NONE);
}


static Parser
parser_make(lulu_State *L, String path, String input)
{
    Parser p;
    p.L        = L;
    p.lexer    = lexer_make(path, input);
    p.token    = token_make_none();
    p.consumed = token_make_none();
    parser_advance(&p);
    return p;
}

LULU_INTERNAL_FUNC Ast_Node *
parser_parse(lulu_State *L, String path, String input)
{
    Ast_Node *root;
    Parser p;

    // TODO(2026-06-30): Allow type declarations and type casting
    p    = parser_make(L, path, input);
    root = parser_expression0(&p);
    parser_expect(&p, TOKEN_EOF);
    return root;
}

#define LEFT(p)  {cast(u8)p, cast(u8)p + 1}
#define RIGHT(p) {cast(u8)p, cast(u8)p}

static const Parser_Rule
PARSER_RULES[] = {
    // Binary Arithmetic operators.
    LEFT(PREC_TERM),       LEFT(PREC_TERM),
    LEFT(PREC_FACTOR),     LEFT(PREC_FACTOR),
    LEFT(PREC_FACTOR),     RIGHT(PREC_EXPONENT),

    // Binary Relational operators.
    LEFT(PREC_EQUALITY),   LEFT(PREC_EQUALITY),
    LEFT(PREC_COMPARISON), LEFT(PREC_COMPARISON),
    LEFT(PREC_COMPARISON), LEFT(PREC_COMPARISON),
};

static Parser_Rule
parser_rule_left(Precedence prec)
{
    Parser_Rule r = LEFT(prec);
    return r;
}

static Parser_Rule
parser_match_rule(Token_Kind k)
{
    static const Parser_Rule empty_rule = {PREC_NONE, PREC_NONE};
    if (TOKEN_ADD <= k && k <= TOKEN_GEQ) {
        return PARSER_RULES[k - TOKEN_ADD];
    }

    switch (k) {
    case TOKEN_AND: return parser_rule_left(PREC_AND);
    case TOKEN_OR:  return parser_rule_left(PREC_OR);
    default:
        break;
    }
    return empty_rule;
}

#undef RIGHT
#undef LEFT
