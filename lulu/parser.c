#include <stdio.h> // [f]printf

#include "ast.h"
#include "state.h"
#include "parser.h"
#include "memory.h"

enum Precedence {
    Prec_None, // Zero value. Useful to indicate a rule does not exist.
    Prec_Expr, // Default precedence to parse with.
    Prec_Or,
    Prec_And,
    Prec_Equality,
    Prec_Comparison,
    Prec_Term,      // x {+,-} y
    Prec_Factor,    // x {*,/,%} y
    Prec_Exponent,  // x ^ y
    Prec_Unary,     // {-,#,not} x
    Prec_Call,
};

typedef enum Precedence Precedence;

// Rules for infix parsing.
typedef struct Parser_Rule Parser_Infix_Rule;
struct Parser_Rule {
    u8 left_prec;  // (Child) Determines when to yield our recursive descent.
    u8 right_prec; // (Parent) Tells the child node when to yield.
};

// Parse a single expression of the given precedence.
static Ast *
parser_expr_prec(Parser *p, bool lhs, Precedence prec);

// Parse a single expression.
static Ast *
parser_expr(Parser *p, bool lhs);


#define LEFT(p)  {cast(u8)p, cast(u8)p + 1}
#define RIGHT(p) {cast(u8)p, cast(u8)p}

static const Parser_Infix_Rule
PARSER_INFIX_RULES[] = {
    // Binary Arithmetic operators.
    LEFT(Prec_Term),       LEFT(Prec_Term),
    LEFT(Prec_Factor),     LEFT(Prec_Factor),
    LEFT(Prec_Factor),     RIGHT(Prec_Exponent),

    // Binary Relational operators.
    LEFT(Prec_Equality),   LEFT(Prec_Equality),
    LEFT(Prec_Comparison), LEFT(Prec_Comparison),
    LEFT(Prec_Comparison), LEFT(Prec_Comparison),
};

static inline Parser_Infix_Rule
parser_rule_left(Precedence prec)
{
    Parser_Infix_Rule r = LEFT(prec);
    return r;
}

static Parser_Infix_Rule
parser_get_infix_rule(Token_Kind k)
{
    static const Parser_Infix_Rule empty_rule = {Prec_None, Prec_None};
    if (Token_Add <= k && k <= Token_Geq) {
        return PARSER_INFIX_RULES[k - Token_Add];
    }

    switch (k) {
    case Token_and: return parser_rule_left(Prec_And);
    case Token_or:  return parser_rule_left(Prec_Or);
    default:
        break;
    }
    return empty_rule;
}

#undef RIGHT
#undef LEFT

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
parser_error_at(Parser *p, const char *info, const Token *t)
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
parser_error(Parser *p, const char *info)
{
    parser_error_at(p, info, &p->token);
}

LULU_NORETURN static void
parser_error_consumed(Parser *p, const char *info)
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
    static const Token t = {Token_None,
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
NOTE(2026-07-03):
    This is about the only place `lhs` is useful. It allows us to avoid
    needlessly parsing a compound literal (in our case, a table) when in
    declarations/assignments. See Odin's parser:

    https://github.com/odin-lang/Odin/blob/1007ea278534e037fa586564c91113f5c925c286/src/parser.cpp#L2379
 */
static Ast *
parser_operand(Parser *p, bool lhs)
{
    Ast *operand = NULL;
    switch (p->token.kind) {
    case Token_Int:
    case Token_Float:
    case Token_String:
    case Token_false:
    case Token_nil:
    case Token_true:
        parser_advance(p);
        operand = ast_literal_new(p->L, &p->consumed);
        if (operand->Literal.value.kind == VALUE_INVALID) {
            const char *info = lexer_error_string(LEXER_INVALID_NUMBER);
            parser_error_consumed(p, info);
        }
        return operand;
    case Token_Open_Paren: {
        Token open, close;
        open    = parser_advance(p);
        operand = parser_expr(p, lhs);
        close   = parser_expect(p, Token_Close_Paren);
        return ast_paren_new(p->L, &open, &close, operand);
    }
    case Token_Ident:
        parser_advance(p);
        return ast_ident_new(p->L, &p->consumed);
    default:
        break;
    }
    return operand;
}

static Slice_Ast
parser_expr_list(Parser *p, bool lhs);

static Ast *
parser_call(Parser *p, Ast *func)
{
    Token     open, close;
    Slice_Ast args = {NULL, 0};

    open = parser_advance(p);
    if (!parser_check(p, Token_Close_Paren)) {
        args = parser_expr_list(p, /*lhs=*/false);
    }

    close = parser_expect(p, Token_Close_Paren);
    return ast_call_new(p->L, &open, &close, func, args);
}

static Ast *
parser_atom_expr(Parser *p, bool lhs, Ast *operand)
{
    if (operand == NULL) {
        parser_error(p, "Expected an operand");
    }

    bool loop = true;
    while (loop) {
        switch (p->token.kind) {
        case Token_Open_Paren:
            operand = parser_call(p, operand);
            break;
        default:
            loop = false;
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
    parser_expect(p, Token_Ident);
    return ast_ident_new(p->L, &p->consumed);
}

static Ast *
parser_unary_expr(Parser *p, bool lhs)
{
    switch (p->token.kind) {
    case Token_cast: {
        Token op = parser_advance(p);
        parser_expect(p, Token_Open_Paren);

        Ast *type = parser_type(p);
        parser_expect(p, Token_Close_Paren);

        Ast *expr = parser_expr(p, /*lhs=*/false);
        return ast_cast_new(p->L, &op, type, expr);
    }
    case Token_Sub:
    case Token_Len:
    case Token_not: {
        // Skip the unary operand so the first token of the argument
        // is our current.
        Token op   = parser_advance(p);
        Ast * expr = parser_expr_prec(p, lhs, Prec_Unary);
        return ast_unary_new(p->L, &op, expr);
    }
    default:
        break;
    }
    return parser_atom_expr(p, lhs, parser_operand(p, lhs));
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

static Ast *
parser_expr_prec(Parser *p, bool lhs, Precedence prec_in)
{
    parser_recurse_push(p);

    Ast *expr = parser_unary_expr(p, lhs);
    for (;;) {
        Parser_Infix_Rule rule = parser_get_infix_rule(p->token.kind);

        // This also catches tokens that are not binary operators.
        if (cast(Precedence)rule.left_prec < prec_in) {
            break;
        }

        // Copy by value as the nonlocal state will update.
        Token op    = parser_advance(p);
        Ast * right = parser_expr_prec(p, /*lhs=*/false, cast(Precedence)rule.right_prec);
        expr        = ast_binary_new(p->L, &op, expr, right);
    }
    parser_recurse_pop(p);
    return expr;
}

static Ast *
parser_expr(Parser *p, bool lhs)
{
    return parser_expr_prec(p, lhs, Prec_Expr);
}

// For pretty-printing debug logging only
#if 0

static const char *
AST_TYPE_NAMES[] = {
#define X(e, ...)   "Ast_" #e " *",
    AST_KINDS(X)
#undef X
};

static const char *
ast_type_name(Ast *a)
{
    return AST_TYPE_NAMES[a->kind];
}

#endif 

// This helps us avoid a permanent allocator for the slice.
static void
parser_expr_list_recurse(Parser *p, bool lhs, Slice_Ast *exprs, usize count)
{
    Ast *expr = parser_expr(p, lhs);
    if (parser_match(p, Token_Comma)) {
        // Recursive case.
        parser_expr_list_recurse(p, lhs, exprs, count + 1);
    } else {
        // Base case: allocate exactly as much as we need upfront.
        exprs->data = mem_arena_alloc_array(Ast *, p->L, count);
        exprs->len  = count;

    }

    // Assign the array in reverse order due to the sequence of recursions.
    exprs->data[count - 1] = expr;
}

static Slice_Ast
parser_expr_list(Parser *p, bool lhs)
{
    Slice_Ast exprs = {NULL, 0};
    parser_expr_list_recurse(p, lhs, &exprs, /*count=*/1);
    return exprs;
}

static const Token *
parser_unassignable_token(const Ast *a)
{
    switch (a->kind) {
    case AstKind_None:        break;
    case AstKind_Literal:     return &a->Literal.token;
    case AstKind_Ident:       break;
    case AstKind_ParenExpr:   return &a->ParenExpr.open;
    case AstKind_CallExpr:    return &a->CallExpr.open;
    case AstKind_CastExpr:    return &a->CastExpr.op;
    case AstKind_UnaryExpr:   return &a->UnaryExpr.op;
    case AstKind_BinaryExpr:  return &a->BinaryExpr.op;
    case AstKind_AssignStmt:  return &a->AssignStmt.op;
    case AstKind_DeclStmt:    break;
    }
    return NULL;
}

/*
 TODO(2026-07-03):
    Make more robust in order to differentiate declarations and assignments.
    E.g. pointer dereferences and array indices are not allowed inside
    declarations, but they are allowed inside assignments. Maybe we can
    use a bit-set?
 */
static void
parser_ensure_assignable(Parser *p, Slice_Ast exprs)
{
    for (usize i = 0; i < exprs.len; i++) {
        if (exprs.data[i]->kind != AstKind_Ident) {
            const Token *loc = parser_unassignable_token(exprs.data[i]);
            LULU_ASSERT(loc != NULL);
            parser_error_at(p, "Unassignable expression", loc);
        }
    }
}

/*
 Assumptions:
 1) We are about to consume a colon, i.e. the ':' character.
 */
static Ast *
parser_decl(Parser *p, Slice_Ast left)
{
    parser_ensure_assignable(p, left);

    // Consume ':'.
    parser_advance(p);
    Slice_Ast right = {NULL, 0};
    Ast *     type  = parser_type(p);

    // Right-hand side is optional- the default value is zero.
    if (parser_match(p, Token_Assign)) {
        right = parser_expr_list(p, /*lhs=*/false);
    }
    return ast_decl_new(p->L, left, type, right);
}

/*
 Assumptions
 1) We are about to consume an equals sign, i.e. the '=' character.
 */
static Ast *
parser_assign(Parser *p, Slice_Ast left)
{
    parser_ensure_assignable(p, left);

    Token     op    = parser_advance(p);
    Slice_Ast right = parser_expr_list(p, /*lhs=*/false);
    return ast_assign_new(p->L, &op, left, right);
}

static Ast *
parser_simple_stmt(Parser *p)
{
    Ast *a = NULL;
    switch (p->token.kind) {
    case Token_Ident: {
        Slice_Ast left = parser_expr_list(p, /*lhs=*/true);

        // After the given expression list, what do we want to do?
        switch (p->token.kind) {
        case Token_Colon:  a = parser_decl(p, left);   break;
        case Token_Assign: a = parser_assign(p, left); break;
        default:
            a = left.data[0];
            // Only allow solo function calls as expression-statements.
            if (left.len == 1 && a->kind == AstKind_CallExpr) {
                break;
            }
            parser_error(p, "Expected declaration/assignment/call");
        }
        break;
    }
    default:
        parser_error(p, "Expected a statement");
    }
    // Optional.
    parser_match(p, Token_Semicol);
    return a;
}


static Parser
parser_make(lulu_State *L, String path, String input)
{
    Parser p = {L, lexer_make(path, input),
         /*token     =*/token_make_none(),
         /*consumed  =*/token_make_none(),
         /*recursions=*/0};

    parser_advance(&p);
    return p;
}

LULU_INTERNAL_FUNC Ast *
parser_parse(lulu_State *L, String path, String input)
{
    Ast *  a;
    Parser p;

    p = parser_make(L, path, input);
    a = parser_simple_stmt(&p);
    parser_expect(&p, Token_Eof);
    return a;
}

