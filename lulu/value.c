#include "value.h"

LULU_INTERNAL_FUNC Value_Literal
value_literal_from_string(Token_Kind k, String s)
{
    Value_Literal v;
    switch (k) {
    case TOKEN_INT:
        v.kind = VALUE_UINT;
        if (!lexer_parse_u64(s, &v.value_uint)) {
            goto invalid;
        }
        break;
    case TOKEN_FLOAT:
        v.kind = VALUE_FLOAT;
        if (!lexer_parse_f64(s, &v.value_float)) {
            goto invalid;
        }
        break;
    case TOKEN_TRUE:
        v.kind       = VALUE_BOOL;
        v.value_bool = true;
        break;
    case TOKEN_FALSE:
        v.kind       = VALUE_BOOL;
        v.value_bool = false;
        break;
    default:
invalid:
        v.kind       = VALUE_NONE;
        v.value_uint = 0;
        break;
    }
    return v;
}

