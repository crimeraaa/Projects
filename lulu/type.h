#ifndef LULU_TYPE_H
#define LULU_TYPE_H

#include "lulu.h"
#include "internal.h"
#include "strings.h"
#include "value.h"

/*
 Description:
    "atom" refers to any of the fundamental types we support.
 */
typedef struct AtomType AtomType;
struct AtomType {
    ValueKind kind;
    String    name;
};

typedef enum TypeKind {
    TypeKind_None,
    TypeKind_Atom,
} TypeKind;

typedef struct Type Type;
struct Type {
    TypeKind kind;
    union {
        AtomType atom;
    };
};

typedef struct TypeEnv_Entry TypeEnv_Entry;
struct TypeEnv_Entry {
    String      key;
    u32         hash;
    const Type *type;
};

typedef struct TypeEnv TypeEnv;
struct TypeEnv {
    TypeEnv_Entry *data;
    usize          used;
    usize          cap;
};

static inline bool
type_is_atom(const Type *t)
{
    return t->kind == TypeKind_Atom;
}

LULU_INTERNAL_FUNC const Type *
atom_type_get(ValueKind k);

LULU_INTERNAL_FUNC void
type_env_init(lulu_State *L, TypeEnv *env);

LULU_INTERNAL_FUNC void
type_env_destroy(lulu_State *L, TypeEnv *env);

LULU_INTERNAL_FUNC bool
type_eq(const Type *a, const Type *b);

LULU_INTERNAL_FUNC const Type *
type_get(lulu_State *L, String key);

LULU_INTERNAL_FUNC void
type_set(lulu_State *L, String key, const Type *type);

#endif  // LULU_TYPE_H
