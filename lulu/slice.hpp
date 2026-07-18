#pragma once

#include "internal.hpp"

template<class T>
struct Slice {
    T *   data = nullptr;
    usize len  = 0;

    template<class N>
    T &
    operator[](N index)
    {
        auto i = cast(usize)index;
        LULU_ASSERTF(i < this->len, "Out of bounds index %zu", i);
        return this->data[i];
    }

    template<class N>
    T const &
    operator[](N index) const
    {
        return cast(Slice<T> *)(this)->operator[](index);
    }
};

template<class T> static inline T *   raw_data(Slice<T> s) { return s.data; }
template<class T> static inline usize len     (Slice<T> s) { return s.len; }
template<class T> static inline T *   begin   (Slice<T> s) { return raw_data(s); }
template<class T> static inline T *   end     (Slice<T> s) { return begin(s) + len(s); }


template<class T>
static inline Slice<T>
slice(Slice<T> s, usize start, usize stop)
{
    usize n = stop - start;
    LULU_ASSERT(start <= stop);
    LULU_ASSERT(stop  <= len(s));
    return {&s[start], n};
}

template<class T>
static inline Slice<T>
slice_from(Slice<T> s, usize start)
{
    return slice(s, start, len(s));
}

template<class T>
static inline Slice<T>
slice_until(Slice<T> s, usize stop)
{
    return slice(s, 0, stop);
}
