#include "value.hpp"

LULU_INTERNAL_FUNC bool
operator==(TValue a, TValue b)
{
    if (a.kind == b.kind) switch (a.kind) {
    case Value_nil:     return true;
    case Value_bool:    return a.get_bool() == b.get_bool();
    case Value_int:     return a.get_intr () == b.get_intr();
    case Value_real:    return a.get_real() == b.get_real();
    case Value_string:  LULU_UNREACHABLE(); break;
    }
    return false;
}

