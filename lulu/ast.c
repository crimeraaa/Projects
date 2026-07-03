// standard
#include <inttypes.h>   // PRIu64, PRIi64
#include <stdio.h>      // FILE, vfprintf

// local
#include "ast.h"
#include "memory.h"

static const char *
AST_KIND_STRINGS[] = {
#define X(e, s, ...)    s,
    AST_KINDS(X)
#undef X
};

LULU_INTERNAL_FUNC const char *
ast_kind_cstring(Ast_Kind k)
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

LULU_INTERNAL_FUNC Ast *
ast_literal_new(lulu_State *L, const Token *token)
{
    Ast_Literal *l = mem_arena_alloc_item(Ast_Literal, L);
    l->kind  = Ast_Kind_Literal;
    l->token = *token;
    l->value = value_literal_from_string(token->kind, token->lexeme);
    return cast(Ast *)l;
}

LULU_INTERNAL_FUNC Ast *
ast_ident_new(lulu_State *L, const Token *token)
{
    Ast_Ident *i = mem_arena_alloc_item(Ast_Ident, L);
    i->kind  = Ast_Kind_Ident;
    i->hash  = string_hash(token->lexeme);
    i->token = *token;
    return cast(Ast *)i;
}

LULU_INTERNAL_FUNC Ast *
ast_paren_new(lulu_State *L, const Token *open, const Token *close, Ast *expr)
{
    Ast_Paren_Expr *p = mem_arena_alloc_item(Ast_Paren_Expr, L);
    p->kind  = Ast_Kind_Paren_Expr;
    p->open  = *open;
    p->close = *close;
    p->expr  = expr;
    return cast(Ast *)p;
}

LULU_INTERNAL_FUNC Ast *
ast_decl_new(lulu_State *L, Slice_Ast names, Ast *type, Slice_Ast values)
{
    Ast_Decl_Stmt *d = mem_arena_alloc_item(Ast_Decl_Stmt, L);
    d->kind   = Ast_Kind_Decl_Stmt;
    d->names  = names;
    d->type   = type;
    d->values = values;
    return cast(Ast *)d;
}

LULU_INTERNAL_FUNC Ast *
ast_assign_new(lulu_State *L, const Token *op, Slice_Ast left, Slice_Ast right)
{
    Ast_Assign_Stmt *a = mem_arena_alloc_item(Ast_Assign_Stmt, L);
    a->kind  = Ast_Kind_Assign_Stmt;
    a->op    = *op;
    a->left  = left;
    a->right = right;
    return cast(Ast *)a;
}

LULU_INTERNAL_FUNC Ast *
ast_cast_new(lulu_State *L, const Token *op, Ast *type, Ast *expr)
{
    Ast_Cast_Expr *c = mem_arena_alloc_item(Ast_Cast_Expr, L);
    c->kind = Ast_Kind_Cast_Expr;
    c->op   = *op;
    c->type = type;
    c->expr = expr;
    return cast(Ast *)c;
}

LULU_INTERNAL_FUNC Ast *
ast_call_new(lulu_State *L, const Token *open, const Token *close, Ast *func, Slice_Ast args)
{
    Ast_Call_Expr *c = mem_arena_alloc_item(Ast_Call_Expr, L);
    c->kind  = Ast_Kind_Call_Expr;
    c->open  = *open;
    c->close = *close;
    c->func  = func;
    c->args  = args;
    return cast(Ast *)c;
}

LULU_INTERNAL_FUNC Ast *
ast_unary_new(lulu_State *L, const Token *op, Ast *arg)
{
    Ast_Unary_Expr *u = mem_arena_alloc_item(Ast_Unary_Expr, L);
    u->kind  = Ast_Kind_Unary_Expr;
    u->op    = *op;
    u->arg   = arg;
    return cast(Ast *)u;
}

LULU_INTERNAL_FUNC Ast *
ast_binary_new(lulu_State *L, const Token *op, Ast *left, Ast *right)
{
    Ast_Binary_Expr *b  = mem_arena_alloc_item(Ast_Binary_Expr, L);
    b->kind  = Ast_Kind_Binary_Expr;
    b->op    = *op;
    b->left  = left;
    b->right = right;
    return cast(Ast *)b;
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
ast_print_kind(Ast_Print *p, Ast *a)
{
    ast_print_string(p, ast_cstring(a));
}

static void
ast_print_kind_op(Ast_Print *p, Ast *a, const Token *t)
{
    ast_print_format(p, "%s(%s)", ast_cstring(a), token_cstring(t));
}

#define ast_print_kind(p, a)    (ast_print_kind)(p, cast(Ast *)a)
#define ast_print_kind_op(p, a) (ast_print_kind_op)(p, cast(Ast *)a, &(a)->op)

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
    ast_print_kind(p, i);
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
    next->next    = NULL;
    next->done    = false;
    p->tail->next = next;
    p->tail       = next;
}

// Prevent parent recursive calls from seeing invalid stack memory.
static void
ast_print_pop(Ast_Print *p)
{
    p->tail->next = NULL;
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
    ast_print_arm_info(p, a, is_elbow, NULL);
}

static void
ast_print_slice(Ast_Print *p, Slice_Ast nodes, bool is_elbow, const char *info)
{
    // Add another layer of tabbing so we can print `info` nicely.
    Ast_Print_Node next;
    ast_print_arrow(p, is_elbow);
    ast_print_format(p, "%s[%zu]", info, nodes.len);

    // Last iteration will be the ident of the slice's last element.
    usize i = 0;
    ast_print_push(p, &next);
    for (; i < nodes.len - 1; i++) {
        ast_print_arm(p, nodes.data[i], /*is_elbow=*/false);
    }
    ast_print_arm(p, nodes.data[i], /*is_elbow=*/true);
    ast_print_pop(p);
}

static void
ast_print_Paren_Expr(Ast_Print *p, Ast_Paren_Expr *r)
{
    // Easier to read but may be misleading to omit the header?
    ast_print_dispatch(p, r->expr);
}

static void
ast_print_Call_Expr(Ast_Print *p, Ast_Call_Expr *c)
{
    bool have_args = (c->args.data != NULL);
    ast_print_kind(p, c);
    ast_print_arm_info(p, c->func, /*is_elbow=*/!have_args, "func");
    if (have_args) {
        ast_print_slice(p, c->args, /*is_elbow=*/true, "args");
    }
}

static void
ast_print_Cast_Expr(Ast_Print *p, Ast_Cast_Expr *c)
{
    ast_print_kind(p, c);
    ast_print_arm_info(p, c->type, /*is_elbow=*/false, "type");
    ast_print_arm_info(p, c->expr, /*is_elbow=*/true,  "expr");
}

static void
ast_print_Decl_Stmt(Ast_Print *p, Ast_Decl_Stmt *d)
{
    bool have_values = (d->values.data != NULL);

    ast_print_kind(p, d);
    ast_print_slice(p, d->names, /*is_elbow=*/false, "names");
    ast_print_arm_info(p, d->type, /*is_elbow=*/!have_values, "type");
    if (have_values) {
        ast_print_slice(p, d->values, /*is_elbow=*/true, "values");
    }
}

static void
ast_print_Assign_Stmt(Ast_Print *p, Ast_Assign_Stmt *a)
{
    ast_print_kind_op(p, a);
    ast_print_slice(p, a->left,  /*is_elbow=*/false, "left");
    ast_print_slice(p, a->right, /*is_elbow=*/true,  "right");
}

static void
ast_print_Unary_Expr(Ast_Print *p, Ast_Unary_Expr *u)
{
    ast_print_kind_op(p, u);
    ast_print_arm(p, u->arg, /*is_elbow=*/true);
}

static void
ast_print_Binary_Expr(Ast_Print *p, Ast_Binary_Expr *b)
{
    ast_print_kind_op(p, b);
    ast_print_arm(p, b->left,  /*is_elbow=*/false);
    ast_print_arm(p, b->right, /*is_elbow=*/true);
}


static void
ast_print_dispatch(Ast_Print *p, Ast *a)
{
    LULU_ASSERT(p->recursions + 1 < AST_MAX_RECURSIONS);
    p->recursions++;

#define ast_print_None(...) cast(void)0
#define X(e, ...)  case Ast_Kind_##e: ast_print_##e(p, cast(Ast_##e *)a); break;
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
    // We require a basal, non-null noded for the list.
    Ast_Print_Node n = {NULL, NULL, false};
    Ast_Print      p = {&n, &n, stdout, /*recursions=*/0};
    ast_print_dispatch(&p, a);
    ast_print_char(&p, '\n');
}

#undef ast_print_kind_op
#undef ast_print_kind
#undef ast_print_format
