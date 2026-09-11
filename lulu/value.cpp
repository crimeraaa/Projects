#include "value.hpp"

LULU_INTERNAL_FUNC bool
tvalue_eq(TValue a, TValue b)
{
    if (a.kind == b.kind) switch (a.kind) {
    case Value_nil:     return true;
    case Value_bool:    return tvalue_bool(a) == tvalue_bool(b);
    case Value_int:     return tvalue_int(a)  == tvalue_int(b);
    case Value_real:    return tvalue_real(a) == tvalue_real(b);
    case Value_string:  LULU_UNREACHABLE(); break;
    }
    return false;
}

