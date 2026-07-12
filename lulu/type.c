#include "type.h"
#include "state.h"
#include "mem.h"
#include "strings.h"

#define type_size_of(T) offsetof(Type, basic) + sizeof(T)
#define type_new(L, T)  cast(Type *)mem_arena_alloc(L, type_size_of(T))

// Map ValueKind to Type. Don't add the type info for 'None'.
static Type const
ATOM_TYPES[] = {
#define basic_type_make(T)    {Value_##T, cast(u32)sizeof(#T) - 1, #T}
    {TypeKind_Basic, {basic_type_make(nil)}},
    {TypeKind_Basic, {basic_type_make(bool)}},
    {TypeKind_Basic, {basic_type_make(uint)}},
    {TypeKind_Basic, {basic_type_make(int)}},
    {TypeKind_Basic, {basic_type_make(real)}},
    {TypeKind_Basic, {basic_type_make(string)}},
#undef basic_type_make
};

LULU_INTERNAL_FUNC Type const *
basic_type_get(ValueKind k)
{
    LULU_ASSERT(k != Value_none);
    return &ATOM_TYPES[k];
}

LULU_INTERNAL_FUNC void
type_env_init(lulu_State *L, TypeEnv *env)
{
    // Necessary as we read these for rehashing.
    env->data = nullptr;
    env->used = 0;
    env->cap  = 0;

    for (usize i = 0; i < count_of(ATOM_TYPES); i++) {
        Type const *type = &ATOM_TYPES[i];
        String      key  = string_make(type->basic.name, type->basic.len);
        type_set(L, key, type);
    }

    for (usize i = 0; i < env->cap; i++) {
        TypeEnv_Entry e = env->data[i];
        if (e.type) {
            LULU_LOGF("env[%zu]: type = %s", i, e.key.data);
        }
    }
}

LULU_INTERNAL_FUNC void
type_env_destroy(lulu_State *L, TypeEnv *env)
{
    unused(L);
    unused(env);
}

LULU_INTERNAL_FUNC bool
type_eq(Type const *a, Type const *b)
{
    if (a->kind == b->kind) switch (a->kind) {
    case TypeKind_None: LULU_UNREACHABLE(); break;
    case TypeKind_Basic: return a->basic.kind == b->basic.kind;
    }
    return false;
}

static TypeEnv_Entry *
type_find_entry(TypeEnv_Entry *data, usize cap, String key, u32 hash)
{
    TypeEnv_Entry *tomb = nullptr;
    usize const    wrap = cap - 1;
    for (usize i = cast(usize)hash & wrap; /* empty */; i = (i + 1) & wrap) {
        TypeEnv_Entry *e = &data[i];
        if (!e->type) {
            if (!tomb) {
                tomb = e;
            } else {
                return (!tomb) ? e : tomb;
            }
        } else if (e->hash == hash && string_eq(e->key, key)) {
            return e;
        }
    }
    LULU_UNREACHABLE();
    return nullptr;
}

static void
type_rehash(lulu_State *L, TypeEnv *env, usize cap)
{
    TypeEnv_Entry *data;

    /*
    NOTE(2026-07-06):
        This is a dangerous assumption! For now, we don't allow users to
        define their own types so we know that the type environment is
        fixed-size. We can get away with storing it in an arena and just
        use a scratch arena in the parsing stage.

        However, if we decide the allow user types, we'll need to move
        to a heap-like permanent allocator of some kind.
     */
    data = mem_arena_alloc_array(L, TypeEnv_Entry, cap);

    // Zero-initialize the new block so we can safely read it later.
    for (usize i = 0; i < cap; i++) {
        data[i].key  = string_make(nullptr, 0);
        data[i].type = nullptr;
    }

    // Rehash old data into our new backing array.
    for (usize i = 0; i < env->cap; i++) {
        TypeEnv_Entry *dst;
        TypeEnv_Entry  src = env->data[i];
        if (!src.type) {
            continue;
        }

        dst  = type_find_entry(data, cap, src.key, src.hash);
        *dst = src;
    }

    // TODO(2026-07-06): Free old data when heap-allocated
    env->data = data;
    env->cap  = cap;
}

LULU_INTERNAL_FUNC Type const *
type_get(lulu_State *L, String key)
{
    TypeEnv *env  = &L->types;
    u32      hash = string_hash(key);
    LULU_ASSERT(env->cap > 0);
    return type_find_entry(env->data, env->cap, key, hash)->type;
}

LULU_INTERNAL_FUNC void
type_set(lulu_State *L, String key, Type const *type)
{
    TypeEnv_Entry *p    = nullptr;
    TypeEnv *const env  = &L->types;
    u32      const hash = string_hash(key);

    // We require at least 2 empty slots in order for the search to work.
    if (env->used + 2 >= env->cap) {
        type_rehash(L, env, (env->cap > 8) ? env->cap * 2 : 8);
    }

    p = type_find_entry(env->data, env->cap, key, hash);
    if (!p->type) {
        env->used++;
    } else {
        // Removing a type?
        if (!type) {
            LULU_ASSERT(env->used > 0);
            env->used--;
        }
    }
    p->key  = key;
    p->hash = hash;
    p->type = type;
}

#undef type_new
#undef type_size_of
