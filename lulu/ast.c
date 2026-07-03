// standard
#include <inttypes.h>
#include <stdio.h>

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
    Ast_Print_Node *next;
    Ast *           ast;
    bool            done;
};

// Printer list.
typedef struct Ast_Print Ast_Print;
struct Ast_Print {
    Ast_Print_Node *head;
    Ast_Print_Node *tail;
    int             recursions;
    bool            trailing_newline;
};

static Ast_Print_Node
ast_print_node_make(Ast *a)
{
    Ast_Print_Node n = {/*next=*/NULL, /*ast=*/a, /*done=*/false};
    return n;
}

static void
ast_print_dispatch(Ast_Print *p, Ast *a);

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
string_print(String s)
{
    bool escape = false;
    printf("\"");
    for (usize i = 0; i < s.len; i++) {
        char c = s.data[i];
        if (escape) {
            char buf[8];
            printf("%s", char_escape(c, buf));
            escape = false;
            continue;
        }

        if (c == '\\') {
            escape = true;
            continue;
        }
        printf("%c", c);
    }
    printf("\"");
}

static const char *
bool2str(bool b)
{
    return b ? "true" : "false";
}

static void
ast_print_Literal(Ast_Print *p, Ast_Literal *l)
{
    Value_Literal v = l->value;
    unused(p);
    switch (v.kind) {
    case VALUE_INVALID: printf("(invalid value)");                  break;
    case VALUE_NIL:     printf("nil");                              break;
    case VALUE_BOOL:    printf("bool(%s)", bool2str(v.value_bool)); break;
    case VALUE_INT:     printf("i64(%" PRIi64 ")", v.value_int);    break;
    case VALUE_UINT:    printf("u64(%" PRIu64 ")", v.value_uint);   break;
    case VALUE_FLOAT:   printf("f64(%.14g)", v.value_float);        break;
    case VALUE_STRING:
        printf("string(");
        string_print(v.value_string);
        printf(")");
        break;
    }
}

static void
ast_print_Ident(Ast_Print *p, Ast_Ident *i)
{
    unused(p);
    printf("ident = ");
    string_print(i->token.lexeme);
}

static void
ast_print_arrow(Ast_Print *p, bool is_elbow, usize slice_len, const char *info)
{
    for (Ast_Print_Node *it = p->head; it != p->tail; it = it->next) {
        // If we already finished printing the headers for this ndoe
        // then avoid cluttering the output with pipe characters.
        printf("%c   ", (it->done) ? ' ' : '|');
    }

    p->tail->done = is_elbow;
    printf("%c-> ", (is_elbow) ? '`' : '|');

    // Not great, not terrible
    if (info) {
        if (slice_len) {
            printf("%s[%zu]\n", info, slice_len);
        } else {
            printf("%s: ", info);
        }
    }
}

// Necessary so child recursive calls see the full list.
static Ast_Print_Node *
ast_print_push(Ast_Print *p, Ast_Print_Node *next)
{
    Ast_Print_Node *curr;
    curr       = p->tail;
    curr->next = next;
    p->tail    = next;
    return curr;
}

// Prevent parent recursive calls from seeing invalid stack memory.
static void
ast_print_pop(Ast_Print *p, Ast_Print_Node *curr)
{
    p->tail    = curr;
    curr->next = NULL;
}


static void
ast_print_arm(Ast_Print *p, Ast *a, bool is_elbow, const char *info)
{
    Ast_Print_Node  next = ast_print_node_make(a);
    Ast_Print_Node *curr;
    ast_print_arrow(p, is_elbow, /*slice_len=*/0, info);
    curr = ast_print_push(p, &next);
    ast_print_dispatch(p, a);
    ast_print_pop(p, curr);

    if (p->trailing_newline) {
        printf("\n");
    }
}

static void
ast_print_arm0(Ast_Print *p, Ast *a, bool is_elbow)
{
    ast_print_arm(p, a, is_elbow, NULL);
}

static bool
ast_is_call(Ast *a)
{
    return a->kind == Ast_Kind_Call_Expr;
}

// TODO(2026-07-03): Fix the excess newline problem!
static void
ast_print_slice(Ast_Print *p, Slice_Ast nodes, bool is_elbow, const char *info)
{
    // Add another layer of tabbing so we can print `info` nicely.
    Ast_Print_Node  next = ast_print_node_make(NULL);
    Ast_Print_Node *curr = NULL;
    usize           i    = 0;

    ast_print_arrow(p, is_elbow, /*slice_len=*/nodes.len, info);
    curr = ast_print_push(p, &next);
    for (; i < nodes.len - 1; i++) {
        ast_print_arm0(p, nodes.data[i], /*is_elbow=*/false);
    }

    ast_print_arm0(p, nodes.data[i], /*is_elbow=*/true);
    ast_print_pop(p, curr);
}

static void
ast_print_Paren_Expr(Ast_Print *p, Ast_Paren_Expr *r)
{
    // Easier to read but may be misleading to omit the header?
    ast_print_dispatch(p, r->expr);

    // printf("paren\n");
    // ast_print_arm(p, r->expr, /*is_elbow=*/true);
}

static void
ast_print_Call_Expr(Ast_Print *p, Ast_Call_Expr *c)
{
    bool has_arg = c->args.data != NULL;
    printf("call\n");
    ast_print_arm(p, c->func, !has_arg, "func");

    if (has_arg) {
        ast_print_slice(p, c->args, /*is_elbow=*/true, "args");
    }
}

static void
ast_print_Cast_Expr(Ast_Print *p, Ast_Cast_Expr *c)
{
    printf("cast\n");
    ast_print_arm(p, c->type, /*is_elbow=*/false, /*info=*/"type");
    ast_print_arm(p, c->expr, /*is_elbow=*/true,  /*info=*/"expr");
}

static void
ast_print_op(Ast *a, const Token *t)
{
    printf("%s(%s)\n", ast_kind_cstring(a->kind), token_kind_cstring(t->kind));
}

#define ast_print_op(a)  (ast_print_op)(cast(Ast *)a, &(a)->op)

static void
ast_print_Decl_Stmt(Ast_Print *p, Ast_Decl_Stmt *d)
{
    bool have_values = (d->values.data != NULL);
    printf("decl\n");
    ast_print_slice(p, d->names, /*is_elbow=*/false, "names");
    ast_print_arm(p, d->type, !have_values, "type");
    if (have_values) {
        ast_print_slice(p, d->values, /*is_elbow=*/true, "values");
    }
}

static void
ast_print_Assign_Stmt(Ast_Print *p, Ast_Assign_Stmt *a)
{
    ast_print_op(a);
    ast_print_slice(p, a->left,  /*is_elbow=*/false, "left");
    ast_print_slice(p, a->right, /*is_elbow=*/true,  "right");
}

static void
ast_print_Unary_Expr(Ast_Print *p, Ast_Unary_Expr *u)
{
    ast_print_op(u);
    ast_print_arm0(p, u->arg, /*is_elbow=*/true);
}

static void
ast_print_Binary_Expr(Ast_Print *p, Ast_Binary_Expr *b)
{
    ast_print_op(b);
    ast_print_arm0(p, b->left,  /*is_elbow=*/false);
    ast_print_arm0(p, b->right, /*is_elbow=*/true);
}


static void
ast_print_dispatch(Ast_Print *p, Ast *a)
{
    LULU_ASSERT(p->recursions + 1 < AST_MAX_RECURSIONS);
    p->recursions++;

#define ast_print_None(...) cast(void)0
#define X(e, ...)  case Ast_Kind_##e: ast_print_##e(p, cast(Ast_##e *)a); break;
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
    Ast_Print_Node n = ast_print_node_make(a);
    Ast_Print      p = {&n, &n, /*recursions=*/0, /*trailing_newline=*/true};
    ast_print_dispatch(&p, a);
}
