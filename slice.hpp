#pragma once

#include <cstdlib>
#include <type_traits>

template <class T>
struct Slice {
    using value_type      = T;
    using self_type       = Slice<value_type>;
    using size_type       = std::size_t;
    using pointer         = value_type *;
    using reference       = value_type &;
    using const_pointer   = const value_type *;
    using const_reference = const value_type &;

    pointer   data;
    size_type length;


    template<class Index>
    reference
    operator[](Index index)
    {
        static_assert(std::is_integral<Index>::value, "'Index' must be an integer type");
        if (static_cast<size_type>(index) > this->length) {
            abort();
        }
        return this->data[index];
    }

    template<class Index>
    const_reference
    operator[](Index index) const
    {
        return unconst()->operator[](index);
    }

private:
    self_type *
    unconst() const
    {
        return const_cast<self_type *>(this);
    }
};
