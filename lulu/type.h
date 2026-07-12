#ifndef LULU_TYPE_H
#define LULU_TYPE_H

#include "lulu.h"
#include "internal.h"
#include "strings.h"
#include "value.h"

/*
 Description:
    "basic" refers to any of the fundamental types we support.
 */
typedef struct BasicType BasicType;
struct BasicType {
    ValueKind kind;
    u32       len;
    char      name[8];
};

typedef enum TypeKind {
    TypeKind_None,
    TypeKind_Basic,
} TypeKind;

typedef struct Type Type;
struct Type {
    TypeKind kind;
    union {
        BasicType basic;
    };
};

typedef struct TypeEnv_Entry TypeEnv_Entry;
struct TypeEnv_Entry {
    String      key;
    u32         hash;
    Type const *type;
};

typedef struct TypeEnv TypeEnv;
struct TypeEnv {
    TypeEnv_Entry *data;
    usize          used;
    usize          cap;
};

static inline bool
type_is_basic(Type const *t)
{
    return t->kind == TypeKind_Basic;
}

LULU_INTERNAL_FUNC Type const *
basic_type_get(ValueKind k);

LULU_INTERNAL_FUNC void
type_env_init(lulu_State *L, TypeEnv *env);

LULU_INTERNAL_FUNC void
type_env_destroy(lulu_State *L, TypeEnv *env);

LULU_INTERNAL_FUNC bool
type_eq(Type const *a, Type const *b);

LULU_INTERNAL_FUNC Type const *
type_get(lulu_State *L, String key);

LULU_INTERNAL_FUNC void
type_set(lulu_State *L, String key, Type const *type);

#endif  // LULU_TYPE_H
