// standard
#include <inttypes.h>
#include <stdio.h>

// local
#include "ast.h"
#include "state.h"

static void *
state_arena_alloc(lulu_State *L, usize size)
{
    void *p = arena_alloc(&L->arena, size);
    if (!p) {
        state_throw(L, LULU_MEMORY_ERROR);
    }
    return p;
}

#define ast_new(T, L) cast(T *)state_arena_alloc(L, sizeof(T))

LULU_INTERNAL_FUNC Ast *
ast_literal_new(lulu_State *L, const Token *token)
{
    Ast_Literal *l = ast_new(Ast_Literal, L);
    l->kind  = AST_LITERAL;
    l->token = *token;
    l->value = value_literal_from_string(token->kind, token->lexeme);
    return cast(Ast *)l;
}

LULU_INTERNAL_FUNC Ast *
ast_ident_new(lulu_State *L, const Token *token)
{
    Ast_Ident *i = ast_new(Ast_Ident, L);
    i->kind  = AST_IDENT;
    i->hash  = string_hash(token->lexeme);
    i->token = *token;
    return cast(Ast *)i;
}

LULU_INTERNAL_FUNC Ast *
ast_paren_new(lulu_State *L, const Token *open, const Token *close, Ast *expr)
{
    Ast_Paren *p = ast_new(Ast_Paren, L);
    p->kind  = AST_PAREN;
    p->open  = *open;
    p->close = *close;
    p->expr  = expr;
    return cast(Ast *)p;
}

LULU_INTERNAL_FUNC Ast *
ast_decl_new(lulu_State *L, Ast *name, Ast *type, Ast *value)
{
    Ast_Decl *d = ast_new(Ast_Decl, L);
    d->kind  = AST_DECL;
    d->name  = name;
    d->type  = type;
    d->value = value;
    return cast(Ast *)d;
}

LULU_INTERNAL_FUNC Ast *
ast_assign_new(lulu_State *L, const Token *op, Ast *left, Ast *right)
{
    Ast_Assign *a = ast_new(Ast_Assign, L);
    a->kind  = AST_ASSIGN;
    a->op    = *op;
    a->left  = left;
    a->right = right;
    return cast(Ast *)a;
}

LULU_INTERNAL_FUNC Ast *
ast_cast_new(lulu_State *L, const Token *op, Ast *type, Ast *expr)
{
    Ast_Cast *c = ast_new(Ast_Cast, L);
    c->kind = AST_CAST;
    c->op   = *op;
    c->type = type;
    c->expr = expr;
    return cast(Ast *)c;
}

LULU_INTERNAL_FUNC Ast *
ast_call_new(lulu_State *L, const Token *open, const Token *close, Ast *func, Ast *arg)
{
    Ast_Call *c = ast_new(Ast_Call, L);
    c->kind  = AST_CALL;
    c->open  = *open;
    c->close = *close;
    c->func  = func;
    c->arg   = arg;
    return cast(Ast *)c;
}

LULU_INTERNAL_FUNC Ast *
ast_unary_new(lulu_State *L, const Token *op, Ast *arg)
{
    Ast_Unary *u  = ast_new(Ast_Unary, L);
    u->kind  = AST_UNARY;
    u->op    = *op;
    u->arg   = arg;
    return cast(Ast *)u;
}

LULU_INTERNAL_FUNC Ast *
ast_binary_new(lulu_State *L, const Token *op, Ast *left, Ast *right)
{
    Ast_Binary *b  = ast_new(Ast_Binary, L);
    b->kind  = AST_BINARY;
    b->op    = *op;
    b->left  = left;
    b->right = right;
    return cast(Ast *)b;
}

// Printer node.
typedef struct Ast_Print_Node Ast_Print_Node;
struct Ast_Print_Node {
    Ast_Print_Node *next;
    bool            done;
};

// Printer list.
typedef struct Ast_Print Ast_Print;
struct Ast_Print {
    Ast_Print_Node *head;
    Ast_Print_Node *tail;
};

static void
ast_node_print(Ast_Print *p, const Ast *a);

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
string_bool(bool b)
{
    return b ? "true" : "false";
}

static void
ast_literal_print(const Ast_Literal *l)
{
    Value_Literal v = l->value;
    switch (v.kind) {
    case VALUE_NIL:     printf("nil");                                 break;
    case VALUE_INVALID: printf("(invalid value)");                     break;
    case VALUE_BOOL:    printf("bool(%s)", string_bool(v.value_bool)); break;
    case VALUE_INT:     printf("i64(%" PRIi64 ")", v.value_int);       break;
    case VALUE_UINT:    printf("u64(%" PRIu64 ")", v.value_uint);      break;
    case VALUE_FLOAT:   printf("f64(%.14g)", v.value_float);           break;
    case VALUE_STRING:
        printf("string(");
        string_print(v.value_string);
        printf(")");
        break;
    }
}

static void
ast_ident_print(const Ast_Ident *i)
{
    printf("ident = ");
    string_print(i->token.lexeme);
}

static void
ast_print_arm_info(Ast_Print *p, const Ast *a, bool is_elbow, const char *info)
{
    Ast_Print_Node  next = {/*next=*/NULL, /*done=*/false};
    Ast_Print_Node *curr = p->tail;

    for (Ast_Print_Node *it = p->head; it != curr; it = it->next) {
        // If we already finished printing the headers for this ndoe
        // then avoid cluttering the output with pipe characters.
        char arm = (it->done) ? ' ' : '|';
        printf("%c   ", arm);
    }

    curr->done = is_elbow;
    printf("%c-> ", (is_elbow) ? '`' : '|');
    if (info) {
        printf("%s: ", info);
    }

    // Necessary so child recursive calls see the full list.
    curr->next = &next;
    p->tail    = &next;
    ast_node_print(p, a);

    // Prevent parent recursive calls from seeing invalid stack memory.
    p->tail    = curr;
    curr->next = NULL;

    if (!is_elbow) {
        printf("\n");
    }
}

static void
ast_print_arm(Ast_Print *p, const Ast *a, bool is_elbow)
{
    ast_print_arm_info(p, a, is_elbow, /*info=*/NULL);
}

static void
ast_paren_print(Ast_Print *p, const Ast_Paren *r)
{
    // Easier to read but may be misleading to omit the header?
    ast_node_print(p, r->expr);

    // printf("paren\n");
    // ast_print_arm(p, r->expr, /*is_elbow=*/true);
}

static void
ast_call_print(Ast_Print *p, const Ast_Call *c)
{
    bool has_arg = c->arg != NULL;
    printf("call\n");
    ast_print_arm_info(p, c->func, /*is_elbow=*/!has_arg, /*info=*/"func");
    if (has_arg) {
        ast_print_arm_info(p, c->arg,  /*is_elbow=*/true,  /*info=*/"arg");
    }
}

static void
ast_cast_print(Ast_Print *p, const Ast_Cast *c)
{
    printf("cast\n");
    ast_print_arm_info(p, c->type, /*is_elbow=*/false, /*info=*/"type");
    ast_print_arm_info(p, c->expr, /*is_elbow=*/true,  /*info=*/"expr");
}

static void
print_op(const char *header, Token_Kind k)
{
    printf("%s(%s)\n", header, token_kind_cstring(k));
}

static void
ast_decl_print(Ast_Print *p, const Ast_Decl *d)
{
    bool have_value = d->value != NULL;
    printf("decl\n");
    ast_print_arm_info(p, d->name,  /*is_elbow=*/false,       /*info=*/"name");
    ast_print_arm_info(p, d->type,  /*is_elbow=*/!have_value, /*info=*/"type");
    if (have_value) {
        ast_print_arm_info(p, d->value, /*is_elbow=*/true,  /*info=*/"value");
    }
}

static void
ast_assign_print(Ast_Print *p, const Ast_Assign *a)
{
    print_op("assign", a->op.kind);
    ast_print_arm_info(p, a->left,  /*is_elbow=*/false, /*info=*/"left");
    ast_print_arm_info(p, a->right, /*is_elbow=*/true,  /*info=*/"right");
}

static void
ast_unary_print(Ast_Print *p, const Ast_Unary *u)
{
    print_op("unary", u->op.kind);
    ast_print_arm(p, u->arg, /*is_elbow=*/true);
}

static void
ast_binary_print(Ast_Print *p, const Ast_Binary *b)
{
    print_op("binary", b->op.kind);
    ast_print_arm(p, b->left,  /*is_elbow=*/false);
    ast_print_arm(p, b->right, /*is_elbow=*/true);
}

static void
ast_node_print(Ast_Print *p, const Ast *a)
{
    switch (a->kind) {
    case AST_LITERAL: ast_literal_print(&a->literal);       break;
    case AST_IDENT:   ast_ident_print(&a->ident);           break;
    case AST_DECL:    ast_decl_print(p, &a->decl);          break;
    case AST_ASSIGN:  ast_assign_print(p, &a->assign_stmt); break;
    case AST_PAREN:   ast_paren_print(p, &a->paren_expr);   break;
    case AST_CALL:    ast_call_print(p, &a->call_expr);     break;
    case AST_CAST:    ast_cast_print(p, &a->cast_expr);     break;
    case AST_UNARY:   ast_unary_print(p, &a->unary_expr);   break;
    case AST_BINARY:  ast_binary_print(p, &a->binary_expr); break;
    default:
        printf("(Ast_Kind(%i) not yet implemented)", a->kind);
        break;
    }
}

LULU_INTERNAL_FUNC void
ast_print(const Ast *a)
{
    Ast_Print_Node n = {/*next=*/NULL, /*done=*/false};
    Ast_Print      p = {&n, &n};
    ast_node_print(&p, a);
    printf("\n");
}
