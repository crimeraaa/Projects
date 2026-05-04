#include <string.h>

#include "table.h"
#include "cdecl.h"

#define X(T, kind, is_signed) {         \
    /*size      =*/ sizeof(T),          \
    /*align     =*/ alignof(T),         \
    /*name      =*/ #T,                 \
    /*name_len  =*/ sizeof(#T) - 1,     \
    /*kind      =*/ kind,               \
    /*quals     =*/ 0,                  \
    /*integer   =*/ {is_signed},        \
}

static const Type_Info
TYPE_INFO_BASIC[] = {
    /*TYPE_VOID     =*/ {0, 0, "void", 4, TYPE_VKIND_VOID, 0, {false}},
    /*TYPE_BOOL     =*/ X(bool,                 TYPE_VKIND_BOOL,      false),
    /*TYPE_CHAR     =*/ X(char,                 TYPE_VKIND_INTEGER,   false),
    /*TYPE_SCHAR    =*/ X(signed char,          TYPE_VKIND_INTEGER,   true),
    /*TYPE_SHORT    =*/ X(short,                TYPE_VKIND_INTEGER,   true),
    /*TYPE_INT      =*/ X(int,                  TYPE_VKIND_INTEGER,   true),
    /*TYPE_LONG     =*/ X(long,                 TYPE_VKIND_INTEGER,   true),
    /*TYPE_LLONG    =*/ X(long long,            TYPE_VKIND_INTEGER,   true),
    /*TYPE_UCHAR    =*/ X(unsigned char,        TYPE_VKIND_INTEGER,   false),
    /*TYPE_USHORT   =*/ X(unsigned short,       TYPE_VKIND_INTEGER,   false),
    /*TYPE_UINT     =*/ X(unsigned int,         TYPE_VKIND_INTEGER,   false),
    /*TYPE_ULONG    =*/ X(unsigned long,        TYPE_VKIND_INTEGER,   false),
    /*TYPE_ULLONG   =*/ X(unsigned long long,   TYPE_VKIND_INTEGER,   false),
    /*TYPE_FLOAT    =*/ X(float,                TYPE_VKIND_FLOATING,  false),
    /*TYPE_DOUBLE   =*/ X(double,               TYPE_VKIND_FLOATING,  false),
    /*TYPE_LDOUBLE  =*/ X(long double,          TYPE_VKIND_FLOATING,  false),
};

#undef X

static Symbol *
table_find_symbol(Symbol *data, size_t data_len, const char *name,
                  size_t name_len, uint32_t hash)
{
    Symbol *tomb, *p;
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

static Type_Error
table_resize(Table *t, size_t new_cap)
{
    Symbol *old_data, *new_data;
    size_t old_cap;
    mem_Allocator_Error err = MEM_OK;

    old_data = t->data;
    old_cap  = t->cap;
    new_data = mem_alloc_array(Symbol, t->alloc, new_cap, &err);
    if (new_data == NULL && err) {
        if (err == MEM_OUT_OF_MEMORY) {
            return TYPE_ERROR_OUT_OF_MEMORY;
        }
        return TYPE_ERROR_INVALID_ALLOCATOR;
    }

    // Rehash
    for (size_t i = 0; i < old_cap; i += 1) {
        Symbol *prev, *curr;

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

Type_Error
table_init(Table *t, mem_Allocator alloc)
{
    Type_Error err;

    t->alloc = alloc;
    t->data  = NULL;
    t->count = 0;
    t->cap   = 0;

    // Ensure this is a power of 2, always!!!
    err = table_resize(t, 32);
    if (!err) {
        for (int k = 0; k < TYPE_KIND_BASIC_COUNT; k += 1) {
            const Type_Info *info;
            Symbol *p;

            info = &TYPE_INFO_BASIC[k];
            p    = table_set(t, info->name, strlen(info->name), &err);
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
table_destroy(Table *t)
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

const Symbol *
table_get(Table *t, const char *name, size_t name_len)
{
    if (t->count == 0) {
        return NULL;
    }

    uint32_t hash = table_hash_lstring(name, name_len);
    return table_find_symbol(t->data, t->cap, name, name_len, hash);
}

Symbol *
table_set(Table *t, const char *name, size_t len, Type_Error *out)
{
    Symbol *p;
    uint32_t hash;

    if (t->count + 1 > t->cap * 3 / 4) {
        size_t new_cap = t->cap * 2;
        if (new_cap == 0) {
            new_cap = 8;
        }

        *out = table_resize(t, new_cap);
        if (*out) {
            return NULL;
        }
    }

    hash = table_hash_lstring(name, len);
    p    = table_find_symbol(t->data, t->cap, name, len, hash);

    // First time writing to this entry means adding to the active count.
    if (p->len == 0 && p->info == NULL) {
        t->count++;
    }

    memcpy(p->name, name, len);
    p->len  = cast(uint8_t)len;
    p->hash = hash;
    return p;
}
