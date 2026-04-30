#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

// Easier to grep for.
#define cast(T) (T)

typedef struct Raw_Dynamic_Array Raw_Dynamic_Array;
struct Raw_Dynamic_Array {
    mem_Allocator allocator;

    // Pointer to the first element owned by the dynamic array.
    //
    void *data;

    // Number of active elements in `data`.
    //
    size_t len;

    // Total number of allocated elements in `data`.
    //
    size_t cap;
};

void
rda_init_none(Raw_Dynamic_Array *d, mem_Allocator allocator)
{
    d->allocator = allocator;
    d->data      = NULL;
    d->len       = 0;
    d->cap       = 0;
}

mem_Allocator_Error
rda_init_len_cap(Raw_Dynamic_Array *d, size_t len, size_t cap, size_t type_size, size_t type_align, mem_Allocator allocator)
{
    mem_Allocator_Error error = MEM_OK;
    void *memory;

    memory = mem_raw_make(&error, type_size * cap, type_align, allocator);
    if (memory == NULL || error != MEM_OK) {
        rda_init_none(d, allocator);
    } else {
        d->allocator = allocator;
        d->data      = memory;
        d->len       = len;
        d->cap       = cap;
    }
    return error;
}

mem_Allocator_Error
rda_destroy(Raw_Dynamic_Array *d)
{
    mem_Allocator_Error error = MEM_OK;
    mem_delete(&error, d->data, d->cap, d->allocator);
    d->data = NULL;
    d->len  = 0;
    d->cap  = 0;
    return error;
}

mem_Allocator_Error
rda_reserve(Raw_Dynamic_Array *d, size_t new_cap, size_t type_size, size_t type_align)
{
    mem_Allocator_Error error = MEM_OK;
    void *new_data = NULL;
    size_t old_size, new_size;

    old_size = d->cap  * type_size;
    new_size = new_cap * type_size;
    new_data = mem_raw_resize(&error, d->data, old_size, new_size, type_align, d->allocator);
    if (new_data == NULL) {
        return MEM_OUT_OF_MEMORY;
    }
    d->data = new_data;
    // Clamp the length to prevent invalid indexes.
    if (d->len > new_cap) {
        d->len = new_cap;
    }
    d->cap = new_cap;
    return error;
}

mem_Allocator_Error
rda_resize(Raw_Dynamic_Array *d, size_t new_len, size_t type_size, size_t type_align)
{
    mem_Allocator_Error error;

    error  = rda_reserve(d, new_len, type_size, type_align);
    d->len = new_len;
    return error;
}

mem_Allocator_Error
rda_append(Raw_Dynamic_Array d, void *element, size_t type_size, size_t type_align)
{
    mem_Allocator_Error error = MEM_OK;

    if (d->len + 1 > d->cap) {
        error = rda_resize(d, d->len + 1, type_size, type_align);
        if (error != MEM_OK) {
            return error;
        }
    }
}
