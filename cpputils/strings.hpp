#pragma once

#include "slice.hpp"

namespace string {

static constexpr auto NOT_FOUND = ~cast(std::size_t)0;

using View = Slice<const char>;

extern inline View
view_from_cstring(const char *s)
{
    size_t n = 0;
    while (s[n] != 0) {
        n++;
    }
    return {s, n};
}

extern inline View
operator""_sv(const char *s, std::size_t n)
{ return {s, n}; }

extern inline bool
operator==(View a, View b)
{
    if (a.len != b.len) {
        return false;
    }

    if (a.data == b.data) {
        return true;
    }

    // memcmp
    for (auto [i, v] : enumerate(a)) {
        if (v != b[i]) {
            return false;
        }
    }
    return true;
}

extern inline std::size_t
index_char(View s, char c)
{
    for (auto [i, v] : enumerate(s)) {
        if (v == c) {
            return i;
        }
    }
    return NOT_FOUND;
}

extern inline std::size_t
last_index_char(View s, char c)
{
    for (auto [i, v] : reversed(s)) {
        if (v == c) {
            return i;
        }
    }
    return NOT_FOUND;
}

};
