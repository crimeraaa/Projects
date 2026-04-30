#ifndef PROJECTS_MEM_H
#define PROJECTS_MEM_H

#include "../common.h"

#define MEM_DEFAULT_ALIGN   sizeof(void *)

enum mem_Allocator_Mode {
    MEM_ALLOC,
    MEM_RESIZE,
    MEM_ALLOC_NON_ZEROED,
    MEM_RESIZE_NON_ZEROED,
    MEM_FREE,
    MEM_FREE_ALL,
};

enum mem_Allocator_Error {
    MEM_OK,
    MEM_NOT_IMPLEMENTED,
    MEM_OUT_OF_MEMORY,
    MEM_INVALID_ARGUMENT,
};

typedef enum mem_Allocator_Mode  mem_Allocator_Mode;
typedef enum mem_Allocator_Error mem_Allocator_Error;

typedef void *(*mem_Allocator_Fn)(
    void *user_data,
    mem_Allocator_Mode mode,
    void *memory,
    size_t old_size,
    size_t new_size,
    size_t align,
    mem_Allocator_Error *err
);

typedef struct mem_Allocator mem_Allocator;
struct mem_Allocator {
    mem_Allocator_Fn fn;
    void *user_data;
};


/** @brief Allocates a new memory block of `size` bytes, zero-initialized,
 *         aligned to an `align`-byte boundary.
   *
 *         If the allocation request could not be fulfilled, a null pointer
 *         is returned. In this case, if `err` itself is non-null then it
 *         pointee (`*err`) will contain the appropriate non-zero error code.
 */
#define mem_alloc_bytes_align(a, size, align, err)                             \
    ((a).fn((a).user_data,                                                     \
        MEM_ALLOC,                                                             \
        /*memory=*/NULL,                                                       \
        /*old_size=*/0,                                                        \
        size,                                                                  \
        align,                                                                 \
        err))


#define mem_alloc_bytes(a, size, err)                                          \
    mem_alloc_bytes_align(a,                                                   \
        size,                                                                  \
        MEM_DEFAULT_ALIGN,                                                     \
        err)


#define mem_alloc_bytes_non_zeroed_align(a, size, align, err)                  \
    ((a).fn((a).user_data,                                                     \
        MEM_ALLOC_NON_ZEROED,                                                  \
        /*memory=*/NULL,                                                       \
        /*old_size=*/0,                                                        \
        size,                                                                  \
        align,                                                                 \
        err))


#define mem_alloc_bytes_non_zeroed(a, size, err)                               \
    mem_alloc_bytes_non_zeroed_align(a,                                        \
        size,                                                                  \
        MEM_DEFAULT_ALIGN,                                                     \
        err)


/** @brief Resizes the memory block `memory` from `old_size` bytes to
 *         `new_size` bytes, aligned to a `align`-byte boundary.
 *
 *         If the resize request could not be fulfilled, then a null pointer
 *         is returned. In this case, if `err` itself is non-null then it
 *         pointee (`*err`) will contain the appropriate non-zero err code.
 */
#define mem_resize_bytes_align(a, memory, old_size, new_size, align, err)      \
    ((a).fn((a).user_data,                                                     \
        MEM_RESIZE,                                                            \
        memory,                                                                \
        old_size,                                                              \
        new_size,                                                              \
        align,                                                                 \
        err))


#define mem_resize_bytes(a, memory, old_size, new_size, err)                   \
    mem_resize_bytes_align(a,                                                  \
        memory,                                                                \
        old_size,                                                              \
        new_size,                                                              \
        MEM_DEFAULT_ALIGN,                                                     \
        err)


#define mem_resize_bytes_non_zeroed_align(a, memory, old_size, new_size, align, err) \
    ((a).fn((a).user_data,                                                     \
        MEM_RESIZE_NON_ZEROED,                                                 \
        memory,                                                                \
        old_size,                                                              \
        new_size,                                                              \
        align,                                                                 \
        err))


#define mem_resize_bytes_non_zeroed(a, memory, old_size, new_size, err)        \
    mem_resize_bytes_non_zeroed_align(a,                                       \
        memory,                                                                \
        old_size,                                                              \
        new_size,                                                              \
        MEM_DEFAULT_ALIGN,                                                     \
        err)


/** @brief Frees the `size` bytes pointed to by `memory`.
 *
 *         If the free request could not be fulfilled, and `err` is a
 *         non-null pointer, then the pointee of `err` (`*err`) will
 *         contain the appropriate non-zero err code.
 */
#define mem_free_bytes(a, memory, size, err)                                   \
    ((a).fn((a).user_data,                                                     \
        MEM_FREE,                                                              \
        memory,                                                                \
        size,                                                                  \
        /*new_size=*/ 0,                                                       \
        /*alignment=*/0,                                                       \
        err))


#define mem_alloc_array(T, a, count, err)                                      \
    cast(T *)(mem_alloc_bytes_align(a,                                         \
        sizeof(T) * (count),                                                   \
        alignof(T),                                                            \
        err))


#define mem_resize_array(T, a, memory, old_count, new_count, err)              \
    cast(T *)(mem_resize_bytes_align(a,                                        \
        memory,                                                                \
        sizeof(T) * (old_count),                                               \
        sizeof(T) * (new_count),                                               \
        alignof(T),                                                            \
        err))


#define mem_free_array(a, memory, count, err)                                  \
    mem_free_bytes(a,                                                          \
        memory,                                                                \
        sizeof(*(memory)) * (count),                                           \
        err)


#ifndef MEM_NO_GLOBAL_HEAP_ALLOCATOR

extern mem_Allocator
mem_global_heap_allocator(void);

#ifdef MEM_IMPLEMENTATION

#include <stdlib.h> // realloc, free
#include <string.h> // memset

static void *
mem_global_heap_allocator_fn(
    void *user_data,
    mem_Allocator_Mode mode,
    void *memory,
    size_t old_size,
    size_t new_size,
    size_t alignment,
    mem_Allocator_Error *err)
{
    void *new_memory = NULL;
    unused(user_data);
    unused(alignment);

    switch (mode) {
    case MEM_ALLOC:
    case MEM_RESIZE:
        new_memory = realloc(memory, new_size);
        if (new_memory == NULL) {
            if (err != NULL) {
                *err = MEM_OUT_OF_MEMORY;
            }
        } else if (new_size > old_size) {
            unsigned char *growth_begin;
            size_t growth_size;

            growth_begin = cast(unsigned char *)new_memory + old_size;
            growth_size  = new_size - old_size;
            memset(growth_begin, 0, growth_size);
        }
        break;
    case MEM_ALLOC_NON_ZEROED:
    case MEM_RESIZE_NON_ZEROED:
        new_memory = realloc(memory, new_size);
        if (new_memory == NULL && err != NULL) {
            *err = MEM_OUT_OF_MEMORY;
        }
        break;
    case MEM_FREE:
        free(memory);
        break;
    default:
        if (err != NULL) {
            *err = MEM_NOT_IMPLEMENTED;
        }
        break;
    }
    return new_memory;
}

extern mem_Allocator
mem_global_heap_allocator(void)
{
    static const mem_Allocator gpa = {mem_global_heap_allocator_fn, NULL};
    return gpa;
}

#endif // MEM_IMPLEMENTATION

#endif // !MEM_NO_GLOBAL_HEAP_ALLOCATOR

#endif // PROJECTS_MEM_H
