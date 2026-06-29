#include <inttypes.h>   // PRI*
#include <stdio.h>      // [f]printf

#include "state.h"
#include "parser.h"
#include "ast.c"
#include "lexer.c"

static Rule
parser_get_rule(Token_Kind k);

static Ast_Node *
parser_expression(Parser *p, Precedence prec);

// Because C is stupid and doesn't have default arguments
static Ast_Node *
parser_expression0(Parser *p);

static const char *
parser_copy(char *buf, size_t buf_len, const char *input, size_t input_len)
{
    size_t it = 0, stop;
    // No known length, so try to infer it (assuming it's nul-terminated...)
    if (input_len == 0) {
        while (input[input_len] != 0) {
            input_len++;
        }
    }

    // The iteration range is exclusive, so we save the last index for the
    // nul character.
    stop = (input_len < buf_len - 1) ? input_len : buf_len - 1;
    if (input_len > stop) {
        buf[it++] = '.';
        buf[it++] = '.';
        buf[it++] = '.';
    }

    for (; it < stop; it++) {
        buf[it] = input[it];
    }
    buf[it] = 0;
    return buf;
}

static void
parser_error_at(const Parser *p, const char *info, const Token *t)
{
    char name[80];
    char loc[80];
    fprintf(stderr, "%s:%i: %s at '%s'\n",
        parser_copy(name, sizeof(name), p->lexer.name, 0), p->current.line,
        info,
        parser_copy(loc, sizeof(loc), t->lexeme, t->len));

    state_throw(p->L, LULU_SYNTAX_ERROR);
}

static void
parser_error(const Parser *p, const char *info)
{
    parser_error_at(p, info, &p->current);
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
    const char *info = NULL;
    p->consumed = p->current;

    err = lexer_scan_token(&p->lexer, &p->current);
    switch (err) {
    case LEXER_OK:
        return;
    case LEXER_UNEXPECTED_CHARACTER: info = "Unexpected character"; break;
    case LEXER_INVALID_NUMBER:       info = "Invalid number";       break;
    case LEXER_UNTERMINATED_STRING:  info = "Unterminated string";  break;
    }
    parser_error(p, info);
}

static Token
token_make_none(void)
{
    static const Token t = {TOKEN_NONE,
        /*lexeme  =*/NULL, /*len =*/0,
        /*literal =*/{0},
        /*line    =*/0,    /*col =*/0};
    return t;
}

static bool
parser_check(const Parser *p, Token_Kind k)
{
    return p->current.kind == k;
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

static void
token_print(const Token *t)
{
    printf("Token(%02i) | '%.*s'", t->kind, cast(int)t->len, t->lexeme);
    switch (t->kind) {
    case TOKEN_INT:   printf(" (u64=%" PRIu64 ")\n", t->value.u); break;
    case TOKEN_FLOAT: printf(" (f64=%.14g)\n", t->value.f);       break;
    default:
        printf("\n");
        break;
    }
}

static Parser
parser_make(lulu_State *L,
    const char *name,
    const char *input, size_t len,
    Arena      *arena)
{
    Parser p;
    p.L         = L;
    p.lexer     = lexer_make(name, input, len);
    p.consumed  = token_make_none();
    p.current   = token_make_none();
    p.arena     = arena;
    parser_advance(&p);
    return p;
}

LULU_INTERNAL_FUNC Ast_Node *
parser_parse(lulu_State *L,
    const char *name,
    const char *input, size_t len,
    Arena      *arena)
{
    Ast_Node *root;
    Parser p;

    p    = parser_make(L, name, input, len, arena);
    root = parser_expression0(&p);
    parser_expect(&p, TOKEN_EOF);
    return root;
}

static Ast_Node *
parser_binary(Parser *p, Rule rule, Ast_Node *left)
{
    Ast_Node *right;
    Ast_Binary_Kind kind;

    right = parser_expression(p, cast(Precedence)rule.right_prec);
    kind  = cast(Ast_Binary_Kind)rule.ast_kind;
    return ast_binary_new(p->L, p->arena, kind, left, right);
}

static Ast_Node *
parser_expression(Parser *p, Precedence prec)
{
    Ast_Node *expr = NULL;
    parser_advance(p);

    // Prefix
    switch (p->consumed.kind) {
    case TOKEN_INT:
        expr = ast_literal_new(p->L, p->arena, AST_UINT, p->consumed.value);
        break;
    case TOKEN_FLOAT:
        expr = ast_literal_new(p->L, p->arena, AST_FLOAT, p->consumed.value);
        break;
    case TOKEN_SUB:
        expr = parser_expression(p, PREC_UNARY);
        expr = ast_unary_new(p->L, p->arena, AST_NEG, expr);
        break;
    case TOKEN_OPEN_PAREN:
        expr = parser_expression0(p);
        parser_expect(p, TOKEN_CLOSE_PAREN);
        break;
    default:
        parser_error_consumed(p, "Expected an expression");
        break;
    }

    // Infix
    for (;;) {
        Rule rule = parser_get_rule(p->current.kind);
        if (!rule.left_prec || cast(Precedence)rule.left_prec < prec) {
            break;
        }

        // Consume the infix token.
        parser_advance(p);
        expr = parser_binary(p, rule, expr);
    }

    // Postfix
    return expr;
}

static Ast_Node *
parser_expression0(Parser *p)
{
    return parser_expression(p, PREC_NONE);
}

#define LEFT(p, k)  {cast(u8)p, cast(u8)p + 1, cast(u8)k}
#define RIGHT(p, k) {cast(u8)p, cast(u8)p,     cast(u8)k}

static const Rule
PARSER_RULES[] = {
    LEFT(PREC_ADD, AST_ADD),
    LEFT(PREC_ADD, AST_SUB),
    LEFT(PREC_MUL, AST_MUL),
    LEFT(PREC_MUL, AST_DIV),
    LEFT(PREC_MUL, AST_MOD),
    RIGHT(PREC_POW, AST_POW),
};

#undef RIGHT
#undef LEFT

static Rule
parser_get_rule(Token_Kind k)
{
    if (TOKEN_ADD <= k && k <= TOKEN_POW) {
        return PARSER_RULES[k - TOKEN_ADD];
    }
    return (Rule){PREC_NONE, PREC_NONE, 0};
}
