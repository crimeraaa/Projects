#ifndef LULU_TYPE_H
#define LULU_TYPE_H

#include "lulu.h"
#include "internal.h"
#include "strings.h"
#include "state.h"

typedef enum Type_Atom_Kind {
    Type_Atom_None,
    Type_Atom_bool,
    Type_Atom_uint,
    Type_Atom_real,
    Type_Atom_string,
} Type_Atom_Kind;

/*
 Description:
    "atom" refers to any of the fundamental types we support.
 */
typedef struct Type_Atom Type_Atom;
struct Type_Atom {
    Type_Atom_Kind kind;
    String         name;
};

typedef enum TypeKind {
    TypeKind_None,
    TypeKind_Atom,
} TypeKind;

typedef struct Type Type;
struct Type {
    TypeKind kind;
    u32      hash;
    union {
        Type_Atom atom;
    };
};

LULU_INTERNAL_FUNC void
type_env_init(lulu_State *L, TypeEnv *env);

LULU_INTERNAL_FUNC void
type_env_destroy(lulu_State *L, TypeEnv *env);

LULU_INTERNAL_FUNC bool
type_eq(const Type *a, const Type *b);

LULU_INTERNAL_FUNC const Type *
type_get(lulu_State *L, String key);

LULU_INTERNAL_FUNC void
type_set(lulu_State *L, String key, Type *type);

#endif  // LULU_TYPE_H
