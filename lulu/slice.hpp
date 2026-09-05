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
        return (cast(Slice<T> *)this)->operator[](index);
    }
};

template<class T> static inline T *   raw_data(Slice<T> s) { return s.data;            }
template<class T> static inline usize len     (Slice<T> s) { return s.len;             }
template<class T> static inline T *   begin   (Slice<T> s) { return raw_data(s);       }
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
slice_ptr(T *p, usize start, usize stop)
{
    usize n = stop - start;
    LULU_ASSERT(start <= stop);
    return {p[start], n};
}

template<class T, usize N>
static inline Slice<T>
slice_array(T (&a)[N], usize start = 0, usize stop = N)
{
    usize n = stop - start;
    LULU_ASSERT(start <= stop);
    LULU_ASSERT(stop  <= N);
    return {&a[start], n};
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

/*
 Description:
    Reverse iterator for use with range-based for loops. All it does is wrap raw pointers
    around a structure that replaces the prefix `++` with a decrement.
 */
template<class T>
struct RevIt {
    T *data = nullptr;

    inline bool
    operator!=(RevIt<T> other) const
    {
        return this->data != other.data;
    }

    inline T &
    operator*()
    {
        return *this->data;
    }

    inline void operator++()
    {
        this->data--;
    }
};

template<class T>
struct RevSlice {
    Slice<T> slice;
};


/*
 Usage in a range-based for loop, i.e. `for (T &e : reverse(s)) { ... }`
 is equivalent to the following code:

    auto __rev  = ::reverse(s)
    auto __it   = ::begin(__rev);
    auto __stop = ::end(__rev);
    for (; __it != __stop; ++__it) { ... }
 */
template<class T> inline RevSlice<T> reverse(Slice<T> s)    { return {s};                  }
template<class T> inline RevIt<T>    begin  (RevSlice<T> r) { return {end(r.slice)   - 1}; }
template<class T> inline RevIt<T>    end    (RevSlice<T> r) { return {begin(r.slice) - 1}; }

