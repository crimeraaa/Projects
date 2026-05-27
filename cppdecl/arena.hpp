#pragma once

#include "slice.hpp"

struct Arena {
private:
    Slice<char> m_buffer;
    size_t m_curr_index, m_prev_index;

public:
    Arena(Slice<char> buffer)
        : m_buffer{buffer}
        , m_curr_index{0}
        , m_prev_index{0}
    {}

    void
    free_all();

    void *
    alloc_raw(size_t size, size_t align);

    template<class T>
    T *
    alloc(size_t count = 1, size_t extra = 0)
    {
        size_t size  = (sizeof(T) * count) + extra;
        size_t align = alignof(T);
        return cast(T *)alloc_raw(size, align);
    }

private:
    // Returns an index to the first available aligned address.
    size_t
    align_forward(size_t align);
};

