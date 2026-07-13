#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "strings.hpp"
#include "value.hpp"

/*
 Description:
    "basic" refers to any of the fundamental types we support.
 */
struct BasicType {
    ValueKind kind;
    u32       len;
    char      name[8];
};

enum TypeKind : u8 {
    TypeKind_None,
    TypeKind_Basic,
};

struct Type {
    TypeKind kind;
    union {
        BasicType basic;
    };
};

struct TypeEnv_Entry {
    String      key;
    u32         hash;
    Type const *type;
};

struct TypeEnv {
    Slice<TypeEnv_Entry> entries;
    usize used;
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

