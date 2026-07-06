// standard
#include <inttypes.h>   // PRIu64, PRIi64
#include <stdio.h>      // FILE, vfprintf

// local
#include "ast.h"
#include "mem.h"

static const char *
AST_KIND_STRINGS[] = {
#define X(e, s, ...)    s,
    AST_KINDS(X)
#undef X
};

LULU_INTERNAL_FUNC const char *
ast_kind_cstring(AstKind k)
{
    return AST_KIND_STRINGS[k];
}

static inline const char *
ast_cstring(const Ast *a)
{
    return ast_kind_cstring(a->kind);
}

static inline const char *
token_cstring(const Token *t)
{
    return token_kind_cstring(t->kind);
}

#define ast_size_of(T)  offsetof(Ast, None) + sizeof(T)
#define ast_new(L, T)   cast(Ast *)mem_arena_alloc(L, ast_size_of(T))

LULU_INTERNAL_FUNC Ast *
ast_literal_new(lulu_State *L, const Token *token)
{
    Ast *a = ast_new(L, Ast_Literal);
    a->kind          = AstKind_Literal;
    a->Literal.token = *token;
    a->Literal.value = value_literal_from_string(token->kind, token->lexeme);
    return a;
}

LULU_INTERNAL_FUNC Ast *
ast_ident_new(lulu_State *L, const Token *token)
{
    Ast *a = ast_new(L, Ast_Ident);
    a->kind        = AstKind_Ident;
    a->Ident.hash  = string_hash(token->lexeme);
    a->Ident.token = *token;
    return a;
}

LULU_INTERNAL_FUNC Ast *
ast_paren_new(lulu_State *L, const Token *open, const Token *close, Ast *expr)
{
    Ast *a = ast_new(L, Ast_ParenExpr);
    a->kind            = AstKind_ParenExpr;
    a->ParenExpr.open  = *open;
    a->ParenExpr.close = *close;
    a->ParenExpr.expr  = expr;
    return a;
}

LULU_INTERNAL_FUNC Ast *
ast_cast_new(lulu_State *L, const Token *op, Ast *type, Ast *expr)
{
    Ast *c = ast_new(L, Ast_CastExpr);
    c->kind          = AstKind_CastExpr;
    c->CastExpr.op   = *op;
    c->CastExpr.type = type;
    c->CastExpr.expr = expr;
    return c;
}

LULU_INTERNAL_FUNC Ast *
ast_call_new(lulu_State *L, const Token *open, const Token *close, Ast *func, Slice_Ast args)
{
    Ast *a = ast_new(L, Ast_CallExpr);
    a->kind           = AstKind_CallExpr;
    a->CallExpr.open  = *open;
    a->CallExpr.close = *close;
    a->CallExpr.func  = func;
    a->CallExpr.args  = args;
    return a;
}

LULU_INTERNAL_FUNC Ast *
ast_unary_new(lulu_State *L, const Token *op, Ast *arg)
{
    Ast *u = ast_new(L, Ast_UnaryExpr);
    u->kind          = AstKind_UnaryExpr;
    u->UnaryExpr.op  = *op;
    u->UnaryExpr.arg = arg;
    return u;
}

LULU_INTERNAL_FUNC Ast *
ast_binary_new(lulu_State *L, const Token *op, Ast *left, Ast *right)
{
    Ast *b  = ast_new(L, Ast_BinaryExpr);
    b->kind             = AstKind_BinaryExpr;
    b->BinaryExpr.op    = *op;
    b->BinaryExpr.left  = left;
    b->BinaryExpr.right = right;
    return b;
}

LULU_INTERNAL_FUNC Ast *
ast_assign_new(lulu_State *L, const Token *op, Slice_Ast left, Slice_Ast right)
{
    Ast *a = ast_new(L, Ast_AssignStmt);
    a->kind             = AstKind_AssignStmt;
    a->AssignStmt.op    = *op;
    a->AssignStmt.left  = left;
    a->AssignStmt.right = right;
    return a;
}

LULU_INTERNAL_FUNC Ast *
ast_decl_new(lulu_State *L, Slice_Ast names, Ast *type, Slice_Ast values)
{
    Ast *a = ast_new(L, Ast_DeclStmt);
    a->kind            = AstKind_DeclStmt;
    a->DeclStmt.names  = names;
    a->DeclStmt.type   = type;
    a->DeclStmt.values = values;
    return a;
}

// Printer node.
typedef struct Ast_Print_Node Ast_Print_Node;
struct Ast_Print_Node {
    Ast_Print_Node *prev;
    Ast_Print_Node *next;
    bool            done;
};

// Printer list.
typedef struct Ast_Print Ast_Print;
struct Ast_Print {
    Ast_Print_Node *head;
    Ast_Print_Node *tail;
    FILE *          stream;
    int             recursions;
};

static void
ast_print_dispatch(Ast_Print *p, Ast *a);

/*
 TODO(2026-07-03):
    In the future, can we manage our own strings?
    Or maybe don't rely on stdio?
 */
#define ast_print_format(p, fmt, ...)    fprintf((p)->stream, fmt, __VA_ARGS__)

static inline void
ast_print_string(Ast_Print *p, const char *s)
{
    ast_print_format(p, "%s", s);
}

static inline void
ast_print_char(Ast_Print *p, char c)
{
    ast_print_format(p, "%c", c);
}


static inline void
ast_print_kind(Ast_Print *p, AstKind k)
{
    ast_print_string(p, ast_kind_cstring(k));
}

static void
ast_print_kind_op(Ast_Print *p, AstKind k, const Token *t)
{
    ast_print_format(p, "%s(%s)", ast_kind_cstring(k), token_cstring(t));
}

/*
 TODO(2026-07-03):
    Add UTF-8 support?
 */
static const char *
char_escape(char c, char *buf)
{
    char *it = buf;

    *it++ = '\\';
    switch (c) {
    case '\a': *it++ = 'a'; break;
    case '\b': *it++ = 'b'; break;
    case '\t': *it++ = 't'; break;
    case '\n': *it++ = 'n'; break;
    case '\v': *it++ = 'v'; break;
    case '\f': *it++ = 'f'; break;
    case '\r': *it++ = 'r'; break;
    case '\\':
    case '\'':
    case '\"': *it++ = c;   break;
    default:
        // Overwrite the '\\' character.
        it[-1] = c;
        break;
    }
    // Nul-terminate.
    *it++ = 0;
    return buf;
}

static void
ast_print_quoted_string(Ast_Print *p, String s)
{
    bool escape = false;
    ast_print_char(p, '\"');
    for (usize i = 0; i < s.len; i++) {
        char c = s.data[i];

        // Already in an escape sequence?
        if (escape) {
            char buf[8];
            ast_print_format(p, "%s", char_escape(c, buf));
            escape = false;
            continue;
        }

        // Start of a new escape sequence?
        if (c == '\\') {
            escape = true;
            continue;
        }

        // Not currently in an escape sequence nor starting a new one.
        ast_print_char(p, c);
    }
    ast_print_char(p, '\"');
}

static void
ast_print_Literal(Ast_Print *p, Ast_Literal *l)
{
    Value_Literal v = l->value;
    switch (v.kind) {
    case VALUE_INVALID:
        ast_print_string(p, "(invalid value)");
        break;
    case VALUE_NIL:
        ast_print_string(p, "nil");
        break;
    case VALUE_BOOL:
        ast_print_format(p, "bool(%s)", v.value_bool ? "true" : "false");
        break;
    case VALUE_INT:
        ast_print_format(p, "i64(%" PRIi64 ")", v.value_int);
        break;
    case VALUE_UINT:
        ast_print_format(p, "u64(%" PRIu64 ")", v.value_uint);
        break;
    case VALUE_FLOAT:
        ast_print_format(p, "f64(%.14g)", v.value_float);
        break;
    case VALUE_STRING:
        ast_print_string(p, "string(");
        ast_print_quoted_string(p, v.value_string);
        ast_print_string(p, ")");
        break;
    }
}

static void
ast_print_Ident(Ast_Print *p, Ast_Ident *i)
{
    ast_print_kind(p, AstKind_Ident);
    ast_print_string(p, " = ");
    ast_print_quoted_string(p, i->token.lexeme);
}

static void
ast_print_arrow(Ast_Print *p, bool is_elbow)
{
    /*
     NIT(2026-07-03):
        This ONE single line was the solution to my pain over the past
        few days. It just required me to think of printing the newlines
        on an "per-header" basis instead of a "per-ending" one...
     */
    ast_print_char(p, '\n');
    for (Ast_Print_Node *it = p->head; it != p->tail; it = it->next) {
        // If we already finished printing the headers for this ndoe
        // then avoid cluttering the output with pipe characters.
        ast_print_format(p, "%c   ", (it->done) ? ' ' : '|');
    }

    p->tail->done = is_elbow;
    ast_print_format(p, "%c-> ", (is_elbow) ? '`' : '|');
}

// Necessary so child recursive calls see the full list.
static void
ast_print_push(Ast_Print *p, Ast_Print_Node *next)
{
    next->prev    = p->tail;
    next->next    = nullptr;
    next->done    = false;
    p->tail->next = next;
    p->tail       = next;
}

// Prevent parent recursive calls from seeing invalid stack memory.
static void
ast_print_pop(Ast_Print *p)
{
    p->tail->next = nullptr;
    p->tail       = p->tail->prev;
}


static void
ast_print_arm_info(Ast_Print *p, Ast *a, bool is_elbow, const char *info)
{
    Ast_Print_Node next;
    ast_print_arrow(p, is_elbow);
    if (info) {
        ast_print_format(p, "%s: ", info);
    }

    // Push the new node only now to avoid an off-by-one error
    // in the arrow printing.
    ast_print_push(p, &next);
    ast_print_dispatch(p, a);
    ast_print_pop(p);
}

static void
ast_print_arm(Ast_Print *p, Ast *a, bool is_elbow)
{
    ast_print_arm_info(p, a, is_elbow, nullptr);
}

static void
ast_print_slice(Ast_Print *p, Slice_Ast nodes, bool is_elbow, const char *info)
{
    // Add another layer of tabbing so we can print `info` nicely.
    Ast_Print_Node next;
    ast_print_arrow(p, is_elbow);
    ast_print_format(p, "%s[%zu]", info, nodes.len);

    // Last iteration will be the index of the slice's last element.
    usize i = 0;
    ast_print_push(p, &next);
    for (; i < nodes.len - 1; i++) {
        ast_print_arm(p, nodes.data[i], /*is_elbow=*/false);
    }
    ast_print_arm(p, nodes.data[i], /*is_elbow=*/true);
    ast_print_pop(p);
}

static void
ast_print_ParenExpr(Ast_Print *p, Ast_ParenExpr *r)
{
    // Easier to read but may be misleading to omit the header?
    ast_print_dispatch(p, r->expr);
}

static void
ast_print_CallExpr(Ast_Print *p, Ast_CallExpr *c)
{
    bool have_args = (c->args.data != nullptr);
    ast_print_kind(p, AstKind_CallExpr);
    ast_print_arm_info(p, c->func, /*is_elbow=*/!have_args, "func");
    if (have_args) {
        ast_print_slice(p, c->args, /*is_elbow=*/true, "args");
    }
}

static void
ast_print_CastExpr(Ast_Print *p, Ast_CastExpr *c)
{
    ast_print_kind(p, AstKind_CastExpr);
    ast_print_arm_info(p, c->type, /*is_elbow=*/false, "type");
    ast_print_arm_info(p, c->expr, /*is_elbow=*/true,  "expr");
}

static void
ast_print_DeclStmt(Ast_Print *p, Ast_DeclStmt *d)
{
    bool have_values = (d->values.data != nullptr);

    ast_print_kind(p, AstKind_DeclStmt);
    ast_print_slice(p, d->names, /*is_elbow=*/false, "names");
    ast_print_arm_info(p, d->type, /*is_elbow=*/!have_values, "type");
    if (have_values) {
        ast_print_slice(p, d->values, /*is_elbow=*/true, "values");
    }
}

static void
ast_print_AssignStmt(Ast_Print *p, Ast_AssignStmt *a)
{
    ast_print_kind_op(p, AstKind_AssignStmt, &a->op);
    ast_print_slice(p, a->left,  /*is_elbow=*/false, "left");
    ast_print_slice(p, a->right, /*is_elbow=*/true,  "right");
}

static void
ast_print_UnaryExpr(Ast_Print *p, Ast_UnaryExpr *u)
{
    ast_print_kind_op(p, AstKind_UnaryExpr, &u->op);
    ast_print_arm(p, u->arg, /*is_elbow=*/true);
}

static void
ast_print_BinaryExpr(Ast_Print *p, Ast_BinaryExpr *b)
{
    ast_print_kind_op(p, AstKind_BinaryExpr, &b->op);
    ast_print_arm(p, b->left,  /*is_elbow=*/false);
    ast_print_arm(p, b->right, /*is_elbow=*/true);
}


static void
ast_print_dispatch(Ast_Print *p, Ast *a)
{
    LULU_ASSERT(p->recursions + 1 < AST_MAX_RECURSIONS);
    p->recursions++;

#define ast_print_None(...) cast(void)0
#define X(e, ...)  case AstKind_##e: ast_print_##e(p, &a->e); break;
    // Macro magic!
    switch (a->kind) {
    AST_KINDS(X)
    }
#undef X
#undef ast_print_None

    p->recursions--;
}


LULU_INTERNAL_FUNC void
ast_print(Ast *a)
{
    // We require a basal, non-nullptr noded for the list.
    Ast_Print_Node n = {nullptr, nullptr, false};
    Ast_Print      p = {&n, &n, stdout, /*recursions=*/0};
    ast_print_dispatch(&p, a);
    ast_print_char(&p, '\n');
}

#undef ast_print_format
#undef ast_new
