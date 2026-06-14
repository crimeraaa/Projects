#ifndef STRINGS_H
#define STRINGS_H

#include "../common.h"

// Strings are read-only views into character arrays.
typedef struct String String;
struct String {
    const char *data;
    size_t len;
};

static inline String
string_make(const char *s, size_t n)
{
    String s2 = {s, n};
    return s2;
}

#endif // !STRINGS_H
