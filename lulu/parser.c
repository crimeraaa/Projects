#include <stdio.h> // [f]printf

#include "state.h"
#include "parser.h"

#if 0
#define LOGFLN(fmt, ...) printf("%-16s " fmt "\n", &__func__[7], __VA_ARGS__)
#define LOGLN(msg)       LOGFLN("%s", msg)
#else
#define LOGFLN(...)
#define LOGLN(msg)
#endif

enum Precedence {
    PREC_NONE, // Zero value. Useful to indicate a rule does not exist.
    PREC_EXPR, // Default precedence to parse with.
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

static Ast *
parser_expr(Parser *p, bool lhs, Precedence prec);

static Ast *
parser_expr0(Parser *p, bool lhs);

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

LULU_NORETURN static void
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
LULU_NORETURN static void
parser_error(const Parser *p, const char *info)
{
    parser_error_at(p, info, &p->token);
}

LULU_NORETURN static void
parser_error_consumed(const Parser *p, const char *info)
{
    parser_error_at(p, info, &p->consumed);
}

#if 0
static void
token_print(const Token *t)
{
    printf("%i:%i: Token(%02i) = \"%.*s\"\n",
        t->line, t->col,
        t->kind,
        cast(int)t->lexeme.len, t->lexeme.data);
}
#endif

// Scan a new current token. The previous, now-consumed token, is returned.
static Token
parser_advance(Parser *p)
{
    Lexer_Error err;
    p->consumed = p->token;
    err = lexer_scan_token(&p->lexer, &p->token);
    // token_print(&p->consumed);
    // token_print(&p->token);
    // printf("===============\n");
    if (err) {
        parser_error(p, lexer_error_string(err));
    }
    return p->consumed;
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

static Token
parser_expect(Parser *p, Token_Kind k)
{
    if (!parser_match(p, k)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected '%s'", token_kind_cstring(k));
        parser_error(p, buf);
    }
    return p->consumed;
}


/*
NOTE(2026-07-02):
    `lhs` is mainly useful here to allow or disallow compound literals.
 */
static Ast *
parser_operand(Parser *p, bool lhs)
{
    Ast *operand = NULL;
    LOGFLN("token: %s", token_kind_cstring(p->token.kind));
    switch (p->token.kind) {
    case TOKEN_INT:
    case TOKEN_FLOAT:
    case TOKEN_STRING:
    case TOKEN_FALSE:
    case TOKEN_NIL:
    case TOKEN_TRUE:
        parser_advance(p);
        operand = ast_literal_new(p->L, &p->consumed);
        if (operand->literal.value.kind == VALUE_INVALID) {
            const char *info = lexer_error_string(LEXER_INVALID_NUMBER);
            parser_error_consumed(p, info);
        }
        return operand;
    case TOKEN_OPEN_PAREN: {
        Token open, close;

        // Skip the open parenthesis so the first token of the
        // inner expression is our current.
        open    = parser_advance(p);
        operand = parser_expr0(p, lhs);
        close   = parser_expect(p, TOKEN_CLOSE_PAREN);
        // TODO(2026-07-01): Determine if next token is start of a cast!
        return ast_paren_new(p->L, &open, &close, operand);
    }
    case TOKEN_IDENT:
        parser_advance(p);
        return ast_ident_new(p->L, &p->consumed);
    default:
        break;
    }
    return operand;
}

static Ast *
parser_call(Parser *p, Ast *func)
{
    Token open, close;
    Ast * arg = NULL;

    open = parser_advance(p);
    if (!parser_check(p, TOKEN_CLOSE_PAREN)) {
        arg = parser_expr0(p, /*lhs=*/false);
    }
    close = parser_expect(p, TOKEN_CLOSE_PAREN);
    return ast_call_new(p->L, &open, &close, func, arg);
}

static Ast *
parser_atom_expr(Parser *p, Ast *operand, bool lhs)
{
    if (operand == NULL) {
        parser_error(p, "Expected an operand");
    }

    bool loop = true;
    while (loop) {
        LOGFLN("token=\"%s\", loop=%i", token_kind_cstring(p->token.kind), loop);
        switch (p->token.kind) {
        case TOKEN_OPEN_PAREN:
            operand = parser_call(p, operand);
            break;
        default:
            loop = false;
            LOGFLN("loop=%i", loop);
            break;
        }
        // After the first atom, we are no longer assignable.
        lhs = false;
    }
    return operand;
}

/*
 Assumptions:
 1) We are about to consume an identifier.
 */
static Ast *
parser_type(Parser *p)
{
    parser_expect(p, TOKEN_IDENT);
    return ast_ident_new(p->L, &p->consumed);
}

static Ast *
parser_unary_expr(Parser *p, bool lhs)
{
    Ast *expr = NULL;
    switch (p->token.kind) {
    case TOKEN_CAST:
    {
        Ast *type;
        Token op = parser_advance(p);
        parser_expect(p, TOKEN_OPEN_PAREN);
        type = parser_type(p);
        parser_expect(p, TOKEN_CLOSE_PAREN);
        expr = parser_expr0(p, /*lhs=*/false);
        return ast_cast_new(p->L, &op, type, expr);
    }
    case TOKEN_SUB:
    case TOKEN_LEN:
    case TOKEN_NOT:
        // Skip the unary operand so the first token of the argument
        // is our current.
        Token op = parser_advance(p);
        expr = parser_expr(p, lhs, PREC_UNARY);
        return ast_unary_new(p->L, &op, expr);
    default:
        break;
    }
    expr = parser_operand(p, lhs);
    return parser_atom_expr(p, expr, lhs);
}

static Ast *
parser_expr(Parser *p, bool lhs, Precedence prec)
{
    Ast *expr = parser_unary_expr(p, lhs);
    LOGFLN("lhs=%i, prec=%i", lhs, prec);
    for (;;) {
        Token       op;
        Ast *       right;
        Parser_Rule rule = parser_match_rule(p->token.kind);

        LOGFLN("left_prec=%i", rule.left_prec);

        // This also catches tokens that are not binary operators.
        if (cast(Precedence)rule.left_prec < prec) {
            break;
        }

        // Copy by value as the nonlocal state will update.
        op    = parser_advance(p);
        right = parser_expr(p, /*lhs=*/false, cast(Precedence)rule.right_prec);
        expr  = ast_binary_new(p->L, &op, expr, right);
    }
    return expr;
}

static Ast *
parser_expr0(Parser *p, bool lhs)
{
    return parser_expr(p, lhs, PREC_EXPR);
}

/*
 Assumptions:
 1) We are about to consume a colon, i.e. the ':' character.

 TODO(2026-07-01):
    Use an array of AST nodes for the left hand side in order to support
    multiple declarations.

    Also ensure that all targets are mere identifiers. Since this is a
    variable declaration, we can't 'declare' array indices, pointer
    dereferences, etc.
 */
static Ast *
parser_decl(Parser *p, Ast *left)
{
    Ast *type, *right;

    parser_advance(p);
    type  = parser_type(p);
    right = NULL;

    // Consume the '=' so our current token is the first one for the
    // right-hand side expression.
    if (parser_match(p, TOKEN_ASSIGN)) {
        right = parser_expr0(p, /*lhs=*/false);
    }
    return ast_decl_new(p->L, left, type, right);
}

/*
 Assumptions
 1) We are about to consume an equals sign, i.e. the '=' character.

 TODO(2026-07-02):
    Ensure that all assignment targets are actually assignable.
    Currently we allow things like `x + 1 = y` which is clearly
    nonsensical.
 */
static Ast *
parser_assign(Parser *p, Ast *left)
{
    Token op;
    Ast * right;

    op    = parser_advance(p);
    right = parser_expr0(p, /*lhs=*/false);
    return ast_assign_new(p->L, &op, left, right);
}

static Ast *
parser_simple_stmt(Parser *p)
{
    Ast * a = NULL;
    Token t = p->token;
    switch (t.kind) {
    case TOKEN_IDENT:
        // We don't currently check for correctness.
        LOGLN("=== BEGIN LHS ============");
        a = parser_expr0(p, true);
        LOGLN("=== END LHS, BEGIN RHS ===");
        LOGFLN("token=\"%s\"", token_kind_cstring(p->token.kind));

        // TODO(2026-07-01): Allow multiple declarations/assignments
        switch (p->token.kind) {
        case TOKEN_COLON:
            a = parser_decl(p, a);
            break;
        case TOKEN_ASSIGN:
            a = parser_assign(p, a);
            break;
        default:
            if (a->kind != AST_CALL) {
                goto bruh;
            }
            break;
        }
        LOGLN("=== END RHS ==============");
        break;
    default:
bruh:
        parser_error(p, "Expected a statement");
    }
    // Optional
    parser_match(p, TOKEN_SEMICOL);
    return a;
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

LULU_INTERNAL_FUNC Ast *
parser_parse(lulu_State *L, String path, String input)
{
    Ast *  a;
    Parser p;

    // TODO(2026-06-30): Allow type declarations and type casting
    p = parser_make(L, path, input);
    a = parser_simple_stmt(&p);
    parser_expect(&p, TOKEN_EOF);
    return a;
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

static inline Parser_Rule
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

#undef LOGLN
#undef LOGFLN
