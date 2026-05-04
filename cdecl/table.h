#ifndef CDECL_TABLE_H
#define CDECL_TABLE_H

#include "cdecl.h"

#define SYMBOL_NAME_SIZE 24

typedef struct Symbol Symbol;
struct Symbol {
    // fuck off
    char name[SYMBOL_NAME_SIZE];
    uint8_t len;
    bool is_variable;
    uint32_t hash;

    // value
    const Type_Info *info;
};

typedef struct Table Table;
struct Table {
    mem_Allocator alloc;
    Symbol *data;
    size_t count, cap;
};

Type_Error
table_init(Table *t, mem_Allocator alloc);

void
table_destroy(Table *t);

const Symbol *
table_get(Table *t, const char *name, size_t len);

const Symbol *
table_get_basic(Table *t, Type_Kind_Basic k, uint8_t quals);

Symbol *
table_set(Table *t, const char *name, size_t len, Type_Error *out);

#endif // !CDECL_TABLE_H
