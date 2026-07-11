#ifndef LULU_VALUE_H
#define LULU_VALUE_H

#include "internal.h"
#include "strings.h"
#include "lexer.h"

typedef enum ValueKind {
    Value_none = -1,
    Value_nil,
    Value_bool,
    Value_uint,
    Value_int,
    Value_real,
    Value_string,
} ValueKind;

typedef union Value Value;
union Value {
    lulu_uint u;
    lulu_int  i;
    lulu_real f;
    bool      b;
};

#define value_bool(v)   (v).b
#define value_uint(v)   (v).u
#define value_int(v)    (v).i
#define value_real(v)   (v).f

typedef struct TValue TValue;
struct TValue {
    ValueKind kind;
    Value    value;
};

#define tvalue_bool(tv) value_bool((tv).value)
#define tvalue_uint(tv) value_uint((tv).value)
#define tvalue_int(tv)  value_int ((tv).value)
#define tvalue_real(tv) value_real((tv).value)

LULU_INTERNAL_FUNC bool
tvalue_eq(TValue a, TValue b);

#endif // !LULU_VALUE_H
