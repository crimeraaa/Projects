#ifndef PRIMER_LIST_H
#define PRIMER_LIST_H

#include "primer.h"

#define LIST_FIELDS(T)                                                         \
    T *data;                                                                   \
    size_t len, cap                                                            \

typedef struct String_List String_List;
struct String_List {
    LIST_FIELDS(String);
};

typedef struct Strand_List Strand_List;
struct Strand_List {
    LIST_FIELDS(Strand);
};

// Pseudo-methods

// Reserve `new_cap` number of elements.
// `list->cap` is set but not `list->len`.
#define list_reserve(T, list, new_cap)                                         \
do {                                                                           \
    size_t _new_cap = (new_cap);                                               \
    T *_new_data = realloc((list)->data, _new_cap);                            \
    if (_new_data == NULL) {                                                   \
        assert(false && "Buy more RAM lol");                                   \
    }                                                                          \
    (list)->data = _new_data;                                                  \
    (list)->cap  = _new_cap;                                                   \
} while (0)

// Write `item` to the first free slot in the list, resizing it if needed.
#define list_append(T, list, item)                                             \
do {                                                                           \
    size_t _cap = (list)->cap;                                                 \
    if ((list)->len + 1 > _cap) {                                              \
        list_reserve(T, list, (_cap > 8) ? _cap * 2 : 8);                      \
    }                                                                          \
    (list)->data[(list)->len++] = item;                                        \
} while (0)

#define list_delete(list)                                                      \
do {                                                                           \
    free((list)->data);                                                        \
} while (0)

#endif // !PRIMER_LIST_H
