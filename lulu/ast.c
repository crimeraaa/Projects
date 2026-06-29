// standard
#include <inttypes.h>
#include <stdio.h>

// local
#include "ast.h"
#include "state.h"
#include "arena.c"

static void *
state_arena_alloc(lulu_State *L, Arena *arena, size_t size)
{
    void *p = arena_alloc(arena, size);
    if (!p) {
        state_throw(L, LULU_MEMORY_ERROR);
    }
    return p;
}

#define ast_node_new(T, L, a) cast(T *)state_arena_alloc(L, a, sizeof(T))

LULU_INTERNAL_FUNC Ast_Node *
ast_literal_new(lulu_State *L,
    Arena          *arena,
    Ast_Literal_Kind kind,
    Value_Literal    value)
{
    Ast_Literal *n  = ast_node_new(Ast_Literal, L, arena);
    n->node_kind    = AST_LITERAL;
    n->literal_kind = kind;
    n->value        = value;
    return cast(Ast_Node *)n;
}

LULU_INTERNAL_FUNC Ast_Node *
ast_unary_new(lulu_State *L,
    Arena         *arena,
    Ast_Unary_Kind kind,
    Ast_Node      *arg)
{
    Ast_Unary *n  = ast_node_new(Ast_Unary, L, arena);
    n->node_kind  = AST_UNARY;
    n->unary_kind = kind;
    n->arg        = arg;
    return cast(Ast_Node *)n;
}

LULU_INTERNAL_FUNC Ast_Node *
ast_binary_new(lulu_State *L,
    Arena          *arena,
    Ast_Binary_Kind kind,
    Ast_Node       *left,
    Ast_Node       *right)
{
    Ast_Binary *n  = ast_node_new(Ast_Binary, L, arena);
    n->node_kind   = AST_BINARY;
    n->binary_kind = kind;
    n->left        = left;
    n->right       = right;
    return cast(Ast_Node *)n;
}

typedef struct Ast_Print_Node Ast_Print_Node;
struct Ast_Print_Node {
    Ast_Print_Node *next;
    bool            done;
};

typedef struct Ast_Print_List Ast_Print;
struct Ast_Print_List {
    Ast_Print_Node *head;
    Ast_Print_Node *tail;
};

static void
ast_node_print(Ast_Print *p, const Ast_Node *n);

static void
ast_literal_print(const Ast_Literal *n)
{
    switch (n->literal_kind) {
    case AST_UINT:  printf("u64(%" PRIu64 ")", n->value.u); break;
    case AST_INT:   printf("i64(%" PRIi64 ")", n->value.i); break;
    case AST_FLOAT: printf("f64(%.14g)", n->value.f);       break;
    }
}

static void
ast_print_arm(Ast_Print *p, const Ast_Node *n, bool is_elbow)
{
    Ast_Print_Node  next = {/*next=*/NULL, /*done=*/false};
    Ast_Print_Node *curr = p->tail;

    // Necessary so recursive calls see the full list.
    curr->next = &next;
    for (Ast_Print_Node *it = p->head; it != curr; it = it->next) {
        char arm = it->done ? ' ' : '|';
        printf("%c   ", arm);
    }

    printf("%c-> ", (is_elbow) ? '`' : '|');
    curr->done = is_elbow;

    p->tail = &next;
    ast_node_print(p, n);
    p->tail = curr;

    // Prevent parent calls from seeing invalid stack memory.
    curr->next = NULL;
    if (!is_elbow) {
        printf("\n");
    }
}

static void
ast_unary_print(Ast_Print *p, const Ast_Unary *n)
{
    const char *op = NULL;
    switch (n->unary_kind) {
    case AST_NEG: op = "neg"; break;
    }
    printf("%s\n", op);
    ast_print_arm(p, n->arg, /*is_elbow=*/true);
}

static void
ast_binary_print(Ast_Print *p, const Ast_Binary *n)
{
    const char *op = NULL;
    switch (n->binary_kind) {
    case AST_ADD: op = "add"; break;
    case AST_SUB: op = "sub"; break;
    case AST_MUL: op = "mul"; break;
    case AST_DIV: op = "div"; break;
    case AST_MOD: op = "mod"; break;
    case AST_POW: op = "pow"; break;
    }

    printf("%s\n", op);
    ast_print_arm(p, n->left,  /*is_elbow=*/false);
    ast_print_arm(p, n->right, /*is_elbow=*/true);
}


static void
ast_node_print(Ast_Print *p, const Ast_Node *n)
{
    switch (n->node_kind) {
    case AST_LITERAL: ast_literal_print(&n->literal);  break;
    case AST_UNARY:   ast_unary_print(p, &n->unary);   break;
    case AST_BINARY:  ast_binary_print(p, &n->binary); break;
    default:
        printf("(Ast_Kind(%i) not yet implemented)", n->node_kind);
        break;
    }
}

LULU_INTERNAL_FUNC void
ast_print(const Ast_Node *n)
{
    Ast_Print_Node n2 = {/*next=*/NULL, /*done=*/false};
    Ast_Print p  = {&n2, &n2};
    ast_node_print(&p, n);
    printf("\n");
}
