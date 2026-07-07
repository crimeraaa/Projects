#ifndef LULU_PARSER_H
#define LULU_PARSER_H

#include "lulu.h"
#include "lexer.h"
#include "chunk.h"
#include "mem.h"
#include "value.h"
#include "type.h"

#define PARSER_CONSTANT_FOLDING 0

// If you exceed this, you should probably rethink what you did!
#define PARSER_MAX_RECURSIONS   250

// Defined in `compiler.h`.
typedef struct Compiler Compiler;
typedef struct Parser   Parser;
struct Parser {
    // Shared state.
    lulu_State *L;
    Compiler *  compiler;

    // Parser state.
    Lexer lexer;
    Token token;

    // Just in case we want to allocate a message.
    Scratch *scratch;

    // Tracked to prevent stack overflow.
    int recursions;
};


typedef struct Parser_Data Parser_Data;
struct Parser_Data {
    String  path, input;
    Chunk   chunk;
    Scratch scratch;
};

typedef enum ExprKind {
    Expr_None,

    // Literal expressions
    Expr_nil,
    Expr_Literal,

    // Expression has been set to a register `reg`.
    Expr_Discharged,

    // Expression has an instruction in `pc` but needs the destination
    // register to be finalized.
    Expr_Pending,
} ExprKind;

typedef struct Expr Expr;
struct Expr {
    ExprKind    kind;
    const Type *type;
    Token       token;
    union {
        u64  literal_uint;
        i64  literal_int;
        f64  literal_real;
        bool literal_bool;

        // Absolute index of a value in the chunk's constants array.
        u32 constant;

        // Absolute index of an instruction in the chunk's bytecode array.
        i32 pc;

        // Absolute stack slot.
        u8 reg;
    };
};

static inline Expr
expr_make(ExprKind kind, const Type *type, const Token *token)
{
    Expr expr = {kind, type, *token, {0}};
    return expr;
}


static inline Expr
expr_make_none(void)
{
    const Token token = token_make_none();
    return expr_make(Expr_None, nullptr, &token);
}

static inline Expr
expr_make_nil(const Token *token)
{
    return expr_make(Expr_nil, nullptr, token);
}

static inline Expr
expr_make_literal(lulu_State *L, Type_Atom_Kind kind, const Token *token)
{
    const Type *type = type_atom_get(L, kind);
    Expr        expr = expr_make(Expr_Literal, type, token);
    return expr;
}

static inline Expr
expr_make_bool(lulu_State *L, const Token *token)
{
    Expr expr = expr_make_literal(L, Type_Atom_bool, token);
    expr.literal_bool = (token->kind == Token_true);
    return expr;
}

static inline Expr
expr_make_uint(lulu_State *L, const Token *token, u64 literal)
{
    Expr expr = expr_make_literal(L, Type_Atom_uint, token);
    expr.literal_uint = literal;
    return expr;
}

static inline Expr
expr_make_real(lulu_State *L, const Token *token, f64 literal)
{
    Expr expr = expr_make_literal(L, Type_Atom_real, token);
    expr.literal_real = literal;
    return expr;
}

static inline bool
expr_is_numeric(Expr *e)
{
    if (e->type->kind == TypeKind_Atom) switch (e->type->atom.kind) {
    case Type_Atom_uint:
    case Type_Atom_int:
    case Type_Atom_real: return true;
    default:
        break;
    }
    return false;
}

LULU_INTERNAL_FUNC Chunk *
parser_parse(lulu_State *L, Parser_Data *data);

// The most common case is to error at the current token.
LULU_INTERNAL_FUNC LULU_NORETURN void
parser_error(Parser *p, const char *info);

LULU_INTERNAL_FUNC LULU_NORETURN void
parser_error_at(Parser *p, const char *info, const Token *t);

#endif // !LULU_PARSER_H
