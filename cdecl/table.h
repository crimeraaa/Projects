#ifndef CDECL_TABLE_H
#define CDECL_TABLE_H

#include "../mem/mem.h"
#include "cdecl.h"

#define SYMBOL_NAME_SIZE 16

struct Symbol {
    // fuck off
    char name[SYMBOL_NAME_SIZE];
    uint8_t len;
    bool is_variable;
    uint32_t hash;
    const struct Type_Info *info;
};

struct Table {
    mem_Allocator alloc;
    struct Symbol *data;
    size_t count, cap;
};

enum Type_Error
table_init(struct Table *t, mem_Allocator alloc);

void
table_destroy(struct Table *t);

const struct Symbol *
table_get(struct Table *t, const char *name, size_t name_len);

enum Type_Error
table_set(struct Table *t, const char *name, size_t name_len,
          struct Symbol **out);

#endif // !CDECL_TABLE_H
