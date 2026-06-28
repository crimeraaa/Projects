#pragma once

#include <cassert>

#include "common.hpp"
#include "traits.hpp"

template<class T, class Z = std::size_t>
struct Slice {
    static_assert(traits::is_integer<Z>, "What?");
    using value_type      = T;
    using size_type       = Z;
    using pointer         = value_type *;
    using reference       = value_type &;
    using iterator        = pointer;
    using const_pointer   = const value_type *;
    using const_reference = const value_type &;
    using const_iterator  = const_pointer;

    // Iterator crap
    struct Pair;
    struct Forward_Iterator;
    struct Reverse_Iterator;

    pointer   data;
    size_type len;

    template<class Index>
    reference
    operator[](Index index)
    {
        // Don't use floats, pointers, and structs, dummmy!
        static_assert(traits::is_integer<Index>, "bruh");
        auto ii = cast(size_type)index;
        assert(0 <= ii && ii < this->len);
        return this->data[ii];
    }

    template<class Index>
    const_reference
    operator[](Index index) const
    {
        auto &s = *const_cast<Slice<T> *>(this);
        return s[index];
    }

    // ugly
    iterator begin() noexcept { return this->data; }
    iterator end()   noexcept { return this->data + this->len; }

    // even uglier
    const_iterator begin() const noexcept { return this->data; }
    const_iterator end()   const noexcept { return this->data + this->len; }

}; // Slice

template<class T, class Z>
struct Slice<T, Z>::Pair {

    size_type  index;
    value_type value;
}; // Pair

template<class T, class Z>
struct Slice<T, Z>::Forward_Iterator {
    size_type index;
    Slice<T>  state;

    Forward_Iterator
    begin() const noexcept
    {
        return {/*index =*/0, this->state};
    }

    Forward_Iterator
    end() const noexcept
    {
        return {/*index =*/this->state.len, this->state};
    }

    bool
    operator==(const Forward_Iterator &stop) const noexcept
    {
        return this->index == stop.index;
    }

    bool
    operator!=(const Forward_Iterator &stop) const noexcept
    {
        return !(*this == stop);
    }

    void
    operator++() noexcept
    {
        this->index++;
    }

    Pair
    operator*() const noexcept
    {
        auto i = this->index;
        auto v = this->state[i];
        return {/*index =*/i, /*value =*/v};
    }
}; // Forward_Iterator

template<class T, class Z>
struct Slice<T, Z>::Reverse_Iterator {
    size_type index;
    Slice<T>  state;

    Forward_Iterator
    begin() const noexcept
    {
        return {/*index =*/this->state.len, this->state};
    }

    Forward_Iterator
    end() const noexcept
    {
        return {/*index =*/0, this->state};
    }

    bool
    operator==(const Reverse_Iterator &stop) const noexcept
    {
        return this->index == stop.index;
    }

    bool
    operator!=(const Reverse_Iterator &stop) const noexcept
    {
        return !(*this == stop);
    }

    void
    operator++() noexcept
    {
        this->index--;
    }

    Pair
    operator*() const noexcept
    {
        auto i = this->index - 1;
        auto v = this->state[i];
        return {/*index =*/i, /*value =*/v};
    }
}; // Reverse_Iterator

template<class T, class Z>
typename Slice<T, Z>::Forward_Iterator
enumerate(Slice<T, Z> s)
{
    return {/*index =*/0, /*state =*/s};
}

template<class T, class Z>
typename Slice<T, Z>::Reverse_Iterator
reversed(Slice<T, Z> s)
{
    return {/*index =*/s.len, /*state =*/s};
}
