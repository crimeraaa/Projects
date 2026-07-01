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
typedef struct Ast_Pnode Ast_Pnode;
struct Ast_Pnode {
    Ast_Pnode *next;
    bool       done;
};

// Printer list.
typedef struct Ast_Plist Ast_Plist;
struct Ast_Plist {
    Ast_Pnode *head;
    Ast_Pnode *tail;
};

static void
ast_node_print(Ast_Plist *p, const Ast *a);

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
string_print(const char *prefix, String s)
{
    bool is_escaped = false;
    printf("%s(\"", prefix);
    for (usize i = 0; i < s.len; i++) {
        char c = s.data[i];
        if (is_escaped) {
            char buf[8];
            printf("%s", char_escape(c, buf));
            is_escaped = false;
            continue;
        }

        if (c == '\\') {
            is_escaped = true;
            continue;
        }
        printf("%c", c);
    }
    printf("\")");
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
    case VALUE_NONE:   printf("(invalid value)");                     break;
    case VALUE_BOOL:   printf("bool(%s)", string_bool(v.value_bool)); break;
    case VALUE_INT:    printf("u64(%" PRIi64 ")", v.value_int);       break;
    case VALUE_UINT:   printf("u64(%" PRIi64 ")", v.value_uint);      break;
    case VALUE_FLOAT:  printf("f64(%.14g)", v.value_float);           break;
    case VALUE_STRING: string_print("string", v.value_string);        break;
    }
}

static void
ast_ident_print(const Ast_Ident *i)
{
    string_print("ident", i->token.lexeme);
}

static void
ast_print_arm_info(Ast_Plist *p, const Ast *a, bool is_elbow, const char *info)
{
    Ast_Pnode  next = {/*next=*/NULL, /*done=*/false};
    Ast_Pnode *curr = p->tail;

    for (Ast_Pnode *it = p->head; it != curr; it = it->next) {
        // If we already finished printing the headers for this ndoe
        // then avoid cluttering the output with pipe characters.
        char arm = (it->done) ? ' ' : '|';
        printf("%c   ", arm);
    }

    curr->done = is_elbow;
    printf("%c-> ", (is_elbow) ? '`' : '|');
    if (info) {
        printf("%-6s = ", info);
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
ast_print_arm(Ast_Plist *p, const Ast *a, bool is_elbow)
{
    ast_print_arm_info(p, a, is_elbow, /*info=*/NULL);
}

static void
ast_paren_print(Ast_Plist *p, const Ast_Paren *r)
{
    printf("paren\n");
    ast_print_arm(p, r->expr, /*is_elbow=*/true);
}

static void
ast_decl_print(Ast_Plist *p, const Ast_Decl *d)
{
    printf("decl\n");
    ast_print_arm_info(p, d->name,  /*is_elbow=*/false, /*info=*/"name");
    ast_print_arm_info(p, d->type,  /*is_elbow=*/false, /*info=*/"type");
    ast_print_arm_info(p, d->value, /*is_elbow=*/true,  /*info=*/"value");
}

static void
ast_assign_print(Ast_Plist *p, const Ast_Assign *a)
{
    printf("assign(%s)\n", token_cstring(a->op.kind));
    ast_print_arm_info(p, a->left,  /*is_elbow=*/false, /*info=*/"left");
    ast_print_arm_info(p, a->right, /*is_elbow=*/true,  /*info=*/"right");
}

static void
ast_unary_print(Ast_Plist *p, const Ast_Unary *u)
{
    printf("unary(%s)\n", token_cstring(u->op.kind));
    ast_print_arm(p, u->arg, /*is_elbow=*/true);
}

static void
ast_binary_print(Ast_Plist *p, const Ast_Binary *b)
{
    printf("binary(%s)\n", token_cstring(b->op.kind));
    ast_print_arm(p, b->left,  /*is_elbow=*/false);
    ast_print_arm(p, b->right, /*is_elbow=*/true);
}

static void
ast_node_print(Ast_Plist *p, const Ast *a)
{
    switch (a->kind) {
    case AST_LITERAL: ast_literal_print(&a->literal);  break;
    case AST_IDENT:   ast_ident_print(&a->ident);      break;
    case AST_DECL:    ast_decl_print(p, &a->decl);     break;
    case AST_ASSIGN:  ast_assign_print(p, &a->assign); break;
    case AST_PAREN:   ast_paren_print(p, &a->paren);   break;
    case AST_UNARY:   ast_unary_print(p, &a->unary);   break;
    case AST_BINARY:  ast_binary_print(p, &a->binary); break;
    default:
        printf("(Ast_Kind(%i) not yet implemented)", a->kind);
        break;
    }
}

LULU_INTERNAL_FUNC void
ast_print(const Ast *a)
{
    Ast_Pnode n = {/*next=*/NULL, /*done=*/false};
    Ast_Plist p = {&n, &n};
    ast_node_print(&p, a);
    printf("\n");
}
