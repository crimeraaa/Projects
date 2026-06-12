#ifndef PRIMER_LIST_H
#define PRIMER_LIST_H

#include "../mem/mem.h"

#define LIST_FIELDS(T)                                                         \
    T *data;                                                                   \
    size_t len, cap                                                            \

// Pseudo-methods

#define list_resize(T, list, new_len, allocator)                               \
do {                                                                           \
    size_t _new_len = (new_len);                                               \
    if (_new_len > (list)->cap) {                                              \
        list_reserve(T, list, (_new_len > 8) ? _new_len * 2 : 8, allocator);   \
    }                                                                          \
    (list)->len = _new_len;                                                    \
} while (0)

// Reserve `new_cap` number of elements, potentially setting the cap.
#define list_reserve(T, list, new_cap, allocator)                              \
do {                                                                           \
    size_t _old_cap = (list)->cap;                                             \
    size_t _new_cap = (new_cap);                                               \
    T *_new_data = mem_resize_array(T, allocator, (list)->data,                \
                                    _old_cap, _new_cap, NULL);                 \
    if (_new_data == NULL) {                                                   \
        assert(false && "Buy more RAM lol");                                   \
    }                                                                          \
    if (_new_cap > _old_cap) {                                                 \
        memset(&_new_data[_old_cap], 0, sizeof(T) * (_new_cap - _old_cap));    \
    }                                                                          \
    (list)->data = _new_data;                                                  \
    (list)->cap  = _new_cap;                                                   \
} while (0)

// Write `item` to the first free slot in the list, resizing it if needed.
#define list_append(T, list, item, allocator)                                  \
do {                                                                           \
    size_t _cap = (list)->cap;                                                 \
    if ((list)->len + 1 > _cap) {                                              \
        list_reserve(T, list, (_cap > 8) ? _cap * 2 : 8, allocator);           \
    }                                                                          \
    (list)->data[(list)->len++] = item;                                        \
} while (0)

#define list_delete(list, allocator)                                           \
do {                                                                           \
    mem_free_array(allocator, (list)->data, (list)->cap, NULL);                \
    (list)->data = NULL;                                                       \
    (list)->len  = 0;                                                          \
    (list)->cap  = 0;                                                          \
} while (0)

#endif // !PRIMER_LIST_H
