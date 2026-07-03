#include "value.h"

LULU_INTERNAL_FUNC Value_Literal
value_literal_from_string(Token_Kind k, String s)
{
    Value_Literal v = {VALUE_INVALID, /*hash=*/0, {0}};
    switch (k) {
    case Token_nil:
        v.kind = VALUE_NIL;
        break;
    case Token_Int:
        if (lexer_parse_u64(s, &v.value_uint)) {
            v.kind = VALUE_UINT;
        }
        break;
    case Token_Float:
        if (lexer_parse_f64(s, &v.value_float)) {
            v.kind = VALUE_FLOAT;
        }
        break;
    case Token_true:
    case Token_false:
        v.kind       = VALUE_BOOL;
        v.value_bool = (k == Token_true);
        break;
    case Token_String:
        v.kind         = VALUE_STRING;
        v.hash         = string_hash(s);
        v.value_string = s;
        break;
    default:
        break;
    }
    return v;
}

