#include "type.hpp"
#include "slice.hpp"
#include "state.hpp"
#include "mem.hpp"
#include "strings.hpp"

#define type_size_of(T) offsetof(Type, basic) + sizeof(T)
#define type_new(L, T)  cast(Type *)mem_arena_alloc_bytes(L, type_size_of(T))

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

LULU_INTERNAL_FUNC Type const *
basic_type_get(ValueKind k)
{
    return &BASIC_TYPES[k];
}

LULU_INTERNAL_FUNC char const *
type_cstring(Type const *t)
{
    switch (t->kind) {
    case TypeKind_None:  break;
    case TypeKind_Basic: return value_kind_cstring(t->basic.kind);
    }
    LULU_PANICF("No type string for TypeKind(%i)", t->kind);
    return nullptr;
}

LULU_INTERNAL_FUNC void
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

LULU_INTERNAL_FUNC void
type_env_destroy(lulu_State *L, TypeEnv *env)
{
    unused(L);
    unused(env);
}

static TypeEnv_Entry *
type_find_entry(Slice<TypeEnv_Entry> entries, String key, u32 hash)
{
    TypeEnv_Entry *tomb = nullptr;
    usize const    wrap = len(entries) - 1;
    for (usize i = cast(usize)hash & wrap; /* empty */; i = (i + 1) & wrap) {
        TypeEnv_Entry *e = &entries[i];
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
type_rehash(lulu_State *L, TypeEnv *env, usize cap)
{
    auto new_hash = mem_alloc_slice<TypeEnv_Entry>(L, cap);
    auto old_hash = env->entries;

    // Zero-initialize the new block so we can safely read it later.
    for (TypeEnv_Entry &e : new_hash) {
        e = {};
    }

    // Rehash old data into our new backing array.
    usize new_used = 0;
    for (TypeEnv_Entry src : old_hash) {
        TypeEnv_Entry *dst;
        if (!src.type) {
            continue;
        }

        dst  = type_find_entry(new_hash, src.key, src.hash);
        *dst = src;
        new_used++;
    }

    mem_free_slice(L, old_hash);
    env->entries = new_hash;
    env->used    = new_used;
}

LULU_INTERNAL_FUNC Type const *
type_get(lulu_State *L, String key)
{
    TypeEnv *env  = &L->types;
    u32      hash = string_hash(key);
    LULU_ASSERT(len(env->entries) > 0);
    return type_find_entry(env->entries, key, hash)->type;
}

LULU_INTERNAL_FUNC void
type_set(lulu_State *L, String key, Type const *type)
{
    TypeEnv_Entry *p    = nullptr;
    TypeEnv *const env  = &L->types;
    u32      const hash = string_hash(key);

    // We require at least 2 empty slots in order for the search to work.
    if (env->used + 2 >= len(env->entries)) {
        usize old_cap = len(env->entries);
        usize new_cap = max(old_cap * 2, usize(8));
        type_rehash(L, env, new_cap);
    }

    p = type_find_entry(env->entries, key, hash);
    if (!p->type) {
        env->used++;
    }
    p->key  = key;
    p->hash = hash;
    p->type = type;
}

#undef type_new
#undef type_size_of
