#include "type.h"
#include "memory.h"
#include "state.h"

#define type_size_of(T) offsetof(Type, atom) + sizeof(T)
#define type_new(L, T)  cast(Type *)mem_arena_alloc(L, type_size_of(T))

static Type *
type_atom_new(lulu_State *L, Type_Atom a)
{
    Type *t = type_new(L, Type_Atom);
    t->kind = TypeKind_Atom;
    t->hash = string_hash(a.name);
    t->atom = a;
    return t;
}


static const Type_Atom TYPE_ATOMS[] = {
    {Type_Atom_bool,   string_literal("bool")},
    {Type_Atom_uint,   string_literal("uint")},
    {Type_Atom_real,   string_literal("real")},
    {Type_Atom_string, string_literal("string")},
};

LULU_INTERNAL_FUNC void
type_env_init(lulu_State *L, TypeEnv *env)
{
    // Necessary as we read these for rehashing.
    env->data = nullptr;
    env->used = 0;
    env->cap  = 0;

    // Don't add the type info for 'None'.
    for (usize i = 0; i < count_of(TYPE_ATOMS); i++) {
        Type *t = type_atom_new(L, TYPE_ATOMS[i]);
        type_set(L, TYPE_ATOMS[i].name, t);
    }

    for (usize i = 0; i < env->cap; i++) {
        TypeEnv_Entry e = env->data[i];
        if (e.type) {
            LULU_LOGFLN("env[%zu]: type = %s", i, e.key.data);
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
type_eq(const Type *a, const Type *b)
{
    if (a->kind == b->kind) switch (a->kind) {
    case TypeKind_None: LULU_UNREACHABLE(); break;
    case TypeKind_Atom: return a->atom.kind == b->atom.kind;
    }
    return false;
}

static TypeEnv_Entry *
type_env_find_entry(TypeEnv_Entry *data, usize cap, String key, u32 hash)
{
    for (usize i = cast(usize)hash & (cap - 1);; i = (i + 1) & (cap - 1)) {
        TypeEnv_Entry *e = &data[i];
        if (!e->type) {
            return e;
        } else if (e->type->hash == hash && string_eq(e->key, key)) {
            return e;
        }
    }
    LULU_UNREACHABLE();
    return nullptr;
}

static void
type_env_rehash(lulu_State *L, TypeEnv *env, usize cap)
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

        dst  = type_env_find_entry(data, cap, src.key, src.type->hash);
        *dst = src;
    }

    // TODO(2026-07-06): Free old data when heap-allocated
    env->data = data;
    env->cap  = cap;
}

LULU_INTERNAL_FUNC const Type *
type_get(lulu_State *L, String key)
{
    TypeEnv *env  = &L->types;
    u32      hash = string_hash(key);
    LULU_ASSERT(env->cap > 0);
    return type_env_find_entry(env->data, env->cap, key, hash)->type;
}

LULU_INTERNAL_FUNC void
type_set(lulu_State *L, String key, Type *type)
{
    TypeEnv_Entry *p;
    TypeEnv *const env  = &L->types;
    const u32      hash = string_hash(key);

    // We require at least 1 empty slot in order for the search to work.
    if (env->used + 1 >= env->cap) {
        type_env_rehash(L, env, (env->cap > 8) ? env->cap * 2 : 8);
    }

    p = type_env_find_entry(env->data, env->cap, key, hash);
    if (!p->type) {
        env->used++;
    }
    p->key  = key;
    p->type = type;
}

#undef type_new
#undef type_size_of
