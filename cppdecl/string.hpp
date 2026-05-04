#pragma once

#include "slice.hpp"

using String = Slice<const char >;

extern inline bool
operator==(String a, String b)
{
    return slice_eq(a, b);
}

extern inline String
operator""_s(const char *data, size_t len)
{
    return {data, len};
}
