#ifndef LULU_VALUE_H
#define LULU_VALUE_H

#include "internal.h"
#include "strings.h"
#include "lexer.h"

enum Value_Literal_Kind {
    VALUE_INVALID,
    VALUE_NIL,
    VALUE_BOOL,
    VALUE_UINT, VALUE_INT,  // TODO(2026-06-30): Use a bigint instead
    VALUE_FLOAT,
    VALUE_STRING,
};

typedef enum   Value_Literal_Kind Value_Literal_Kind;
typedef struct Value_Literal      Value_Literal;
struct Value_Literal {
    Value_Literal_Kind kind;
    u32 hash;
    union {
        u64    value_uint;
        i64    value_int;
        f64    value_float;
        bool   value_bool;
        String value_string;
    };
};

LULU_INTERNAL_FUNC Value_Literal
value_literal_from_string(Token_Kind k, String s);

#endif // !LULU_VALUE_H
