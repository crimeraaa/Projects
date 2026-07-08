#ifndef LULU_VALUE_H
#define LULU_VALUE_H

#include "internal.h"
#include "strings.h"
#include "lexer.h"

typedef enum Value_Kind {
    Value_Invalid = -1,
    Value_nil,
    Value_bool,
    Value_uint,
    Value_int,
    Value_real,
    Value_string,
} Value_Kind;

typedef union Value Value;
union Value {
    lulu_uint u;
    lulu_int  i;
    lulu_real f;
    bool      b;
};

typedef struct TValue TValue;
struct TValue {
    Value_Kind kind;
    Value      value;
};

LULU_INTERNAL_FUNC bool
tvalue_eq(TValue a, TValue b);

#endif // !LULU_VALUE_H
