#include "arena.hpp"

void
Arena::free_all()
{
    this->curr_index = 0;
    this->prev_index = 0;
}

size_t
Arena::align_forward(size_t align)
{
    uintptr_t base_addr, curr_addr, modulo;
    char *data;

    data      = raw_data(this->buffer);
    base_addr = cast(uintptr_t)data;
    curr_addr = base_addr + cast(uintptr_t)this->curr_index;

    // Fast modulo by power of 2.
    // Counts the number of padding bytes needed to align ourselves.
    modulo = curr_addr & cast(uintptr_t)(align - 1);
    if (modulo != 0) {
        // Not aligned so push it up to the next aligned address.
        curr_addr += align - modulo;
    }

    // Get the relative index into the arena's buffer.
    return cast(size_t)(curr_addr - base_addr);
}

void *
Arena::alloc_raw(size_t size, size_t align)
{
    size_t index = this->align_forward(align);
    if (index + size < len(this->buffer)) {
        void *ptr = &this->buffer[index];
        this->prev_index = index;
        this->curr_index = index + size;
        memset(ptr, 0, size);
        return ptr;
    }
    return nullptr;
}
