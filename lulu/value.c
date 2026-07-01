#include "value.h"

LULU_INTERNAL_FUNC Value_Literal
value_literal_from_string(Token_Kind k, String s)
{
    Value_Literal v = {VALUE_NONE, /*hash=*/0, {0}};
    switch (k) {
    case TOKEN_INT:
        if (lexer_parse_u64(s, &v.value_uint)) {
            v.kind = VALUE_UINT;
        }
        break;
    case TOKEN_FLOAT:
        if (lexer_parse_f64(s, &v.value_float)) {
            v.kind = VALUE_FLOAT;
        }
        break;
    case TOKEN_TRUE:
    case TOKEN_FALSE:
        v.kind       = VALUE_BOOL;
        v.value_bool = (k == TOKEN_TRUE);
        break;
    case TOKEN_STRING:
        v.kind         = VALUE_STRING;
        v.hash         = string_hash(s);
        v.value_string = s;
        break;
    default:
        break;
    }
    return v;
}

