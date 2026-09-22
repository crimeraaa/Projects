#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "slice.cpp"
#include "strings.cpp"
#include "mem.cpp"
#include "value.cpp"

#define type_size_of(T) offsetof(Type, basic) + sizeof(T)
#define type_new(L, T)  cast(Type *)mem_arena_alloc_bytes(L, type_size_of(T))

/*
 Description:
    "basic" refers to any of the fundamental types we support.
 */
struct BasicType {
    ValueKind kind;
    u8        len;
    char      name[8];
};

enum TypeKind : u8 {
    TypeKind_None,
    TypeKind_Basic, // Concrete type: `ValueKind`
};

struct Type {
    TypeKind kind;
    union {
        BasicType basic;
    };
};

struct TypeEnvEntry {
    String      key;
    u32         hash = 0;
    Type const *type = nullptr;
};

struct TypeEnv {
    Slice<TypeEnvEntry> entries;
    usize               used    = 0;
};

// Necessary evil for unity builds. Defined in `state.cpp`.
TypeEnv &
state_type_env(lulu_State *L);

static inline bool
type_is_basic(Type const *t)
{
    return t->kind == TypeKind_Basic;
}


// Maps `ValueKind` to a fundamental `Type`.
static Type const
BASIC_TYPES[] = {
#define basic_type_make(T)    {Value_##T, cast(u32)sizeof(#T) - 1, #T}
    {TypeKind_Basic, {basic_type_make(nil)   } },
    {TypeKind_Basic, {basic_type_make(bool)  } },
    {TypeKind_Basic, {basic_type_make(int)   } },
    {TypeKind_Basic, {basic_type_make(real)  } },
    {TypeKind_Basic, {basic_type_make(string)} },
#undef basic_type_make
};

Type const *
basic_type_get(ValueKind k)
{
    return &BASIC_TYPES[k];
}

char const *
type_cstring(Type const *t)
{
    switch (t->kind) {
    case TypeKind_None:  break;
    case TypeKind_Basic: return t->basic.name;
    }
    LULU_PANICF("No type string for TypeKind(%i)", t->kind);
    return nullptr;
}

static TypeEnvEntry *
type_find_entry(Slice<TypeEnvEntry> entries, String key, u32 hash)
{
    TypeEnvEntry *tomb = nullptr;
    usize const    wrap = entries.len() - 1;
    for (usize i = cast(usize)hash & wrap; /* empty */; i = (i + 1) & wrap) {
        TypeEnvEntry *e = &entries[i];
        if (!e->type) {
            if (!tomb) {
                tomb = e;
            } else {
                return (!tomb) ? e : tomb;
            }
        } else if (e->hash == hash && e->key == key) {
            return e;
        }
    }
    LULU_UNREACHABLE();
    return nullptr;
}

static void
type_rehash(lulu_State *L, TypeEnv &env, usize cap)
{
    auto new_hash = mem_alloc_slice<TypeEnvEntry>(L, cap);
    auto old_hash = env.entries;

    // Zero-initialize the new block so we can safely read it later.
    for (TypeEnvEntry &e : new_hash) {
        e = {};
    }

    // Rehash old data into our new backing array.
    usize new_used = 0;
    for (TypeEnvEntry src : old_hash) {
        TypeEnvEntry *dst;
        if (!src.type) {
            continue;
        }

        dst  = type_find_entry(new_hash, src.key, src.hash);
        *dst = src;
        new_used++;
    }

    mem_free_slice(L, old_hash);
    env.entries = new_hash;
    env.used    = new_used;
}

void
type_set(lulu_State *L, String key, Type const *type)
{
    TypeEnvEntry *p    = nullptr;
    TypeEnv &     env  = state_type_env(L);
    u32           hash = string_hash(key);

    // We require at least 2 empty slots in order for the search to work.
    if (env.used + 2 >= env.entries.len()) {
        usize old_cap = env.entries.len();
        usize new_cap = max(old_cap * 2, usize(8));
        type_rehash(L, env, new_cap);
    }

    p = type_find_entry(env.entries, key, hash);
    if (!p->type) {
        env.used++;
    }
    p->key  = key;
    p->hash = hash;
    p->type = type;
}

void
type_env_init(lulu_State *L, TypeEnv *env)
{
    // Necessary as we read these for rehashing.
    env->entries = {nullptr, 0};
    env->used    = 0;

    for (Type const &type : slice_array(BASIC_TYPES)) {
        String key  = {type.basic.name, cast(usize)type.basic.len};
        type_set(L, key, &type);
    }
}

void
type_env_destroy(lulu_State *L, TypeEnv *env)
{
    unused(L);
    unused(env);
}

Option<Type const *>
type_get(lulu_State *L, String key)
{
    TypeEnv &env  = state_type_env(L);
    u32      hash = string_hash(key);
    LULU_ASSERT(env.entries.len() > 0);

    TypeEnvEntry *e = type_find_entry(env.entries, key, hash);
    if (e->key == key) {
        return Some(e->type);
    } else {
        return None{};
    }
}

#undef type_new
#undef type_size_of
