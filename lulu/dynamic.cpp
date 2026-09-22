#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "slice.cpp"
#include "mem.cpp"

template<class T>
class Dynamic {
    Slice<T> m_slice;
    usize    m_cap;

public:
    Dynamic() : m_slice{}, m_cap{0} {}

    // Defer to underlying Slice implementation.
    template<class N>
    T &
    operator[](N index) const { return this->m_slice[index]; }

    usize
    len()      const noexcept { return this->m_slice.len(); }

    usize
    cap()      const noexcept { return this->m_cap; }

    T *
    raw_data() const noexcept { return this->m_slice.raw_data(); }

    T *
    begin()    const noexcept { return this->m_slice.begin(); }

    T *
    end()      const noexcept { return this->m_slice.end(); }

    Slice<T>
    slice()    const noexcept { return this->m_slice; }

    void
    append(lulu_State *L, T const &value)
    {
        if (this->len() + 1 > this->cap()) {
            this->m_slice.m_data = mem_heap_grow(L, this->m_slice.m_data, &this->m_cap);
        }

        // Raw access because we assign to a (currently) out of bounds index.
        // Only once the length is updated can we use operator[] again.
        this->m_slice.m_data[this->m_slice.m_len++] = value;
    }

    void
    shrink(lulu_State *L)
    {
        usize n = this->len();
        this->m_slice.m_data = mem_heap_resize(L, this->raw_data(), this->cap(), /*new_cap=*/n);
        this->m_cap           = n;
    }

    void
    free(lulu_State *L)
    {
        mem_free_slice(L, this->m_slice);
    }
};

template<class T>
static inline RevSlice<T>
reverse(Dynamic<T> d)
{
    return reverse(d.slice());
}
