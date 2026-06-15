#pragma once

#include <type_traits>

#include "../common.h"

template<class T>
struct Slice {
    using value_type      = T;
    using pointer         = value_type *;
    using reference       = value_type &;
    using iterator        = pointer;
    using const_pointer   = const value_type *;
    using const_reference = const value_type &;
    using const_iterator  = const_pointer;

    pointer data;
    size_t len;

    template<class Index>
    reference
    operator[](Index i)
    {
        size_t i2 = cast(size_t)i;
        static_assert(std::is_integral_v<Index>, "fuck off");
        if (i2 >= this->len) {
            assert(false && "Out of bounds index!");
        }
        return this->data[i2];
    }

    template<class Index>
    const_reference
    operator[](Index i) const
    {
        // This is safe because we don't actually modify `this`.
        // The returned ref is also promoted to const anyway.
        return cast(Slice<T> *)(this)->operator[](i);
    }

    iterator
    begin()
    {
        return this->data;
    }

    iterator
    end()
    {
        return this->data + this->len;
    }

    const_iterator
    begin() const
    {
        return this->data;
    }

    const_iterator
    end() const
    {
        return this->data + this->len;
    }
};

template<class T>
global inline size_t
len(Slice<T> s)
{
    return s.len;
}

template<class T>
global inline T *
raw_data(Slice<T> s)
{
    return s.data;
}

template<class T>
global inline bool
slice_eq(Slice<T> a, Slice<T> b)
{
    if (len(a) != len(b)) {
        return false;
    }

    for (size_t i = 0; i < len(a); i += 1) {
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }
    return true;
}

template<class T, class Index>
global inline Slice<T>
slice_from(Slice<T> src, Index start)
{
    // `operator[]` will do the index validation for us.
    return {&src[start], len(src) - cast(size_t)start};
}
