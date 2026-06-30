// standard
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

// local
#include "ast.h"
#include "state.h"

static void *
state_arena_alloc(lulu_State *L, size_t size)
{
    void *p = arena_alloc(&L->arena, size);
    if (!p) {
        state_throw(L, LULU_MEMORY_ERROR);
    }
    return p;
}

#define ast_node_new(T, L) cast(T *)state_arena_alloc(L, sizeof(T))

LULU_INTERNAL_FUNC Ast_Node *
ast_literal_new(lulu_State *L, const Token *token)
{
    Ast_Literal *n = ast_node_new(Ast_Literal, L);
    n->kind  = AST_LITERAL;
    n->token = *token;
    n->value = value_literal_from_string(token->kind, token->lexeme);
    return cast(Ast_Node *)n;
}

LULU_INTERNAL_FUNC Ast_Node *
ast_ident_new(lulu_State *L, const Token *token)
{
    Ast_Ident *n = ast_node_new(Ast_Ident, L);
    n->kind  = AST_IDENT;
    n->hash  = string_hash(token->lexeme);
    n->token = *token;
    return cast(Ast_Node *)n;
}

LULU_INTERNAL_FUNC Ast_Node *
ast_paren_new(lulu_State *L,
    const Token *open,
    const Token *close,
    Ast_Node    *expr)
{
    Ast_Paren *n = ast_node_new(Ast_Paren, L);
    n->kind  = AST_PAREN;
    n->open  = *open;
    n->close = *close;
    n->expr  = expr;
    return cast(Ast_Node *)n;
}

LULU_INTERNAL_FUNC Ast_Node *
ast_unary_new(lulu_State *L, const Token *op, Ast_Node *arg)
{
    Ast_Unary *n  = ast_node_new(Ast_Unary, L);
    n->kind  = AST_UNARY;
    n->op    = *op;
    n->arg   = arg;
    return cast(Ast_Node *)n;
}

LULU_INTERNAL_FUNC Ast_Node *
ast_binary_new(lulu_State *L,
    const Token *op,
    Ast_Node    *left,
    Ast_Node    *right)
{
    Ast_Binary *n  = ast_node_new(Ast_Binary, L);
    n->kind  = AST_BINARY;
    n->op    = *op;
    n->left  = left;
    n->right = right;
    return cast(Ast_Node *)n;
}

// Printer node.
typedef struct Ast_Pnode Ast_Pnode;
struct Ast_Pnode {
    Ast_Pnode *next;
    bool       done;
};

// Printer list.
typedef struct Ast_Plist Ast_Print;
struct Ast_Plist {
    Ast_Pnode *head;
    Ast_Pnode *tail;
};

static void
ast_node_print(Ast_Print *p, const Ast_Node *n);

static void
string_print(const char *prefix, String s)
{
    printf("%s(\"", prefix);
    for (size_t i = 0; i < s.len; i++) {
        printf("%c", s.data[i]);
    }
    printf("\")");
}

static const char *
string_bool(bool b)
{
    return b ? "true" : "false";
}

static void
ast_literal_print(const Ast_Literal *n)
{
    Value_Literal v = n->value;
    switch (v.kind) {
    case VALUE_NONE:   printf("(invalid value)");                     break;
    case VALUE_BOOL:   printf("bool(%s)", string_bool(v.value_bool)); break;
    case VALUE_INT:    printf("u64(%" PRIi64 ")", v.value_int);       break;
    case VALUE_UINT:   printf("u64(%" PRIi64 ")", v.value_uint);      break;
    case VALUE_FLOAT:  printf("f64(%.14g)", v.value_float);           break;
    case VALUE_STRING: string_print("string", v.value_string);        break;
    }
}

static void
ast_ident_print(const Ast_Ident *n)
{
    string_print("ident", n->token.lexeme);
}

static void
ast_print_arm(Ast_Print *p, const Ast_Node *n, bool is_elbow)
{
    Ast_Pnode  next = {/*next=*/NULL, /*done=*/false};
    Ast_Pnode *curr = p->tail;

    for (Ast_Pnode *it = p->head; it != curr; it = it->next) {
        char arm = it->done ? ' ' : '|';
        printf("%c   ", arm);
    }

    printf("%c-> ", (is_elbow) ? '`' : '|');
    curr->done = is_elbow;

    // Necessary so recursive calls see the full list.
    curr->next = &next;
    p->tail    = &next;
    ast_node_print(p, n);
    p->tail    = curr;

    // Prevent parent calls from seeing invalid stack memory.
    curr->next = NULL;
    if (!is_elbow) {
        printf("\n");
    }
}

static void
ast_paren_print(Ast_Print *p, const Ast_Paren *n)
{
    printf("paren\n");
    ast_print_arm(p, n->expr, /*is_elbow=*/true);
}

static void
ast_unary_print(Ast_Print *p, const Ast_Unary *n)
{
    printf("unary(%s)\n", token_cstring(n->op.kind));
    ast_print_arm(p, n->arg, /*is_elbow=*/true);
}

static void
ast_binary_print(Ast_Print *p, const Ast_Binary *n)
{
    printf("binary(%s)\n", token_cstring(n->op.kind));
    ast_print_arm(p, n->left,  /*is_elbow=*/false);
    ast_print_arm(p, n->right, /*is_elbow=*/true);
}


static void
ast_node_print(Ast_Print *p, const Ast_Node *n)
{
    switch (n->kind) {
    case AST_LITERAL: ast_literal_print(&n->literal);  break;
    case AST_IDENT:   ast_ident_print(&n->ident);      break;
    case AST_PAREN:   ast_paren_print(p, &n->paren);   break;
    case AST_UNARY:   ast_unary_print(p, &n->unary);   break;
    case AST_BINARY:  ast_binary_print(p, &n->binary); break;
    default:
        printf("(Ast_Kind(%i) not yet implemented)", n->kind);
        break;
    }
}

LULU_INTERNAL_FUNC void
ast_print(const Ast_Node *n)
{
    Ast_Pnode n2 = {/*next=*/NULL, /*done=*/false};
    Ast_Print p  = {&n2, &n2};
    ast_node_print(&p, n);
    printf("\n");
}
