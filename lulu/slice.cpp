#pragma once

#include "internal.hpp"

template<class T>
class Dynamic;

template<class T>
class Slice {
protected:
    // Forward declare instances of `Dynamic` as being able to access our
    // protected members, but only if they're templated with the same type.
    friend class Dynamic<T>;

    T *   m_data;
    usize m_len;

public:
    using Self = Slice<T>;

    constexpr
    Slice() : m_data{nullptr}, m_len{0} {}

    constexpr
    Slice(T *data, usize len) : m_data{data}, m_len{len} {}

    template<class N>
    T &
    operator[](N index) const
    {
        auto i = cast(usize)index;
        // You may opt to let ASAN help here to report the stack trace.
        LULU_ASSERTF(i < this->len(), "Out of bounds index %zu / %zu", i, this->len());
        return this->m_data[i];
    }

    T *
    raw_data() const noexcept { return this->m_data; }

    usize
    len()      const noexcept { return this->m_len; }

    T *
    begin()    const noexcept { return this->m_data; }

    T *
    end()      const noexcept { return this->begin() + this->len(); }

    Self
    slice(usize start, usize stop) const
    {
        usize n = stop - start;
        LULU_ASSERT(start <= stop);
        LULU_ASSERT(stop  <= this->len());
        // If `T` is const to begin with, then `Slice<T>` is equivalent to
        // `Slice<T const>`. The extra `const` is redundant.
        return {&this->m_data[start], n};
    }

    Self
    slice_from(usize start) { return this->slice(start, this->len()); }

    Self
    slice_until(usize stop) { return this->slice(0, stop); }

    bool
    has_ptr(T const *ptr) const noexcept
    {
        // We assume that pointers are comparable regardless if they're related
        // or not.
        uintptr addr       = cast(uintptr)ptr;
        uintptr start_addr = cast(uintptr)this->begin();
        uintptr stop_addr  = cast(uintptr)this->end();
        return start_addr <= addr && addr < stop_addr;
    }

    usize
    index_ptr_unsafe(T *ptr) noexcept
    {
        return cast(usize)(ptr - this->raw_data());
    }

    Option<usize>
    index_ptr(T *ptr) noexcept
    {
        if (this->has_ptr(ptr)) {
            return Some(this->index_ptr_unsafe(ptr));
        } else {
            return None{};
        }
    }
};

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
    LULU_ASSERT(n <= N);
    return {&a[start], n};
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

    inline void
    operator++()
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
template<class T>
static inline RevSlice<T>
reverse(Slice<T> s)  { return {s}; }

template<class T>
static inline RevIt<T>
begin(RevSlice<T> r) { return {r.slice.end()   - 1}; }

template<class T>
static inline RevIt<T>
end(RevSlice<T> r)   { return {r.slice.begin() - 1}; }

