#include <string.h>

#include "table.h"
#include "cdecl.h"

static struct Symbol *
table_find_symbol(struct Symbol *data, size_t data_len, const char *name,
                  size_t name_len, uint32_t hash)
{
    struct Symbol *tomb, *p;
    size_t i, wrap;

    tomb = NULL;
    wrap = data_len - 1;
    for (i = cast(size_t)hash & wrap; /* empty */; i = (i + 1) & wrap) {
        p = &data[i];
        // Empty key?
        if (p->len == 0) {
            // Completely empty entry?
            if (p->info == NULL) {
                return (tomb == NULL) ? p : tomb;
            }
            // No name with some data, meaning it's reusable.
            if (tomb == NULL) {
                tomb = p;
            }
        } else if (p->hash == hash && cast(size_t)p->len == name_len) {
            if (memcmp(name, p->name, p->len) == 0) {
                return p;
            }
        }
    }
    // Unreachable
    return NULL;
}

static enum Type_Error
table_resize(struct Table *t, size_t new_cap)
{
    struct Symbol *old_data, *new_data;
    size_t old_cap;
    mem_Allocator_Error err = MEM_OK;

    old_data = t->data;
    old_cap  = t->cap;
    new_data = mem_alloc_array(struct Symbol, t->alloc, new_cap, &err);
    if (new_data == NULL && err) {
        if (err == MEM_OUT_OF_MEMORY) {
            return TYPE_ERROR_OUT_OF_MEMORY;
        }
        return TYPE_ERROR_INVALID_ALLOCATOR;
    }

    // Rehash
    for (size_t i = 0; i < old_cap; i += 1) {
        struct Symbol *prev, *curr;

        prev = &old_data[i];
        // Skip empty entries and tombstones
        if (prev->len == 0 || prev->info == NULL) {
            continue;
        }
        curr = table_find_symbol(new_data, new_cap, prev->name,
                                 cast(size_t)prev->len, prev->hash);
        *curr = *prev;
    }
    mem_free_array(t->alloc, old_data, old_cap, NULL);
    t->data = new_data;
    t->cap  = new_cap;
    return TYPE_ERROR_NONE;
}

enum Type_Error
table_init(struct Table *t, mem_Allocator alloc)
{
    enum Type_Error err;

    t->alloc = alloc;
    t->data  = NULL;
    t->count = 0;
    t->cap   = 0;

    // Ensure this is a power of 2, always!!!
    err = table_resize(t, TYPE_KIND_BASIC_COUNT);
    if (!err) {
        for (enum Type_Info_Variant_Kind k = 0; k < TYPE_KIND_BASIC_COUNT; k += 1) {
            const struct Type_Info *info;
            struct Symbol *p;

            info = &TYPE_INFO_BASIC[k];
            err  = table_set(t, info->name, strlen(info->name), &p);
            if (err) {
                table_destroy(t);
                break;
            }

            // The type keywords are never variables.
            p->is_variable = false;
            p->info        = info;
        }
    }
    return err;
}

void
table_destroy(struct Table *t)
{
    mem_free_array(t->alloc, t->data, t->cap, NULL);
}

static uint32_t
table_hash_lstring(const char *s, size_t n)
{
    // FNV-1A. See: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV_offset_basis
    static const uint32_t
    PRIME  = 0x01000193,
    OFFSET = 0x811c9dc5;

    uint32_t hash = OFFSET;
    for (size_t i = 0; i < n; i += 1) {
        hash ^= cast(uint32_t)s[i];
        hash *= PRIME;
    }
    return hash;
}

const struct Symbol *
table_get(struct Table *t, const char *name, size_t name_len)
{
    if (t->count == 0) {
        return NULL;
    }

    uint32_t hash = table_hash_lstring(name, name_len);
    return table_find_symbol(t->data, t->cap, name, name_len, hash);
}

enum Type_Error
table_set(struct Table *t, const char *name, size_t name_len, struct Symbol **out)
{
    struct Symbol *p;
    uint32_t hash;

    if (t->count + 1 > t->cap * 3 / 4) {
        size_t new_cap;
        enum Type_Error err;

        new_cap = t->cap * 2;
        if (new_cap == 0) {
            new_cap = 8;
        }

        err = table_resize(t, new_cap);
        if (err) {
            return err;
        }
    }

    hash = table_hash_lstring(name, name_len);
    p    = table_find_symbol(t->data, t->cap, name, name_len, hash);

    // First time writing to this entry means adding to the active count.
    if (p->len == 0 && p->info == NULL) {
        t->count++;
    }

    memcpy(p->name, name, name_len);
    p->len  = cast(uint8_t)name_len;
    p->hash = hash;
    *out    = p;
    return TYPE_ERROR_NONE;
}
