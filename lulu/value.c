#include "value.h"

LULU_INTERNAL_FUNC bool
tvalue_eq(TValue a, TValue b)
{
    if (a.kind == b.kind) switch (a.kind) {
    case Value_none:    LULU_UNREACHABLE(); break;
    case Value_nil:     return true;
    case Value_bool:    return a.value.b == b.value.b;
    case Value_uint:    return a.value.u == b.value.u;
    case Value_int:     return a.value.i == b.value.i;
    case Value_real:    return a.value.f == b.value.f;
    case Value_string:  LULU_UNREACHABLE(); break;
    }
    return false;
}

