#pragma once

#include "slice.hpp"

struct Arena {
    using Buffer = Slice<char>;

    Buffer buffer;
    size_t curr_index, prev_index;

    Arena(Buffer buffer)
        : buffer{buffer}
        , curr_index{0}
        , prev_index{0}
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
        return cast(T *)this->alloc_raw(size, align);
    }

private:
    // Returns an index to the first available aligned address.
    size_t
    align_forward(size_t align);
};

