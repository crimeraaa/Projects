#ifndef MEM_STACK_H
#define MEM_STACK_H

#include <stdint.h> // uint8_t

#include "mem.h"

typedef struct mem_Stack mem_Stack;
struct mem_Stack {
    unsigned char *buffer;
    size_t offset;
    size_t cap;
};

typedef struct mem_Stack_Allocation_Header Stack_Header;
struct mem_Stack_Allocation_Header {
    uint8_t padding;
};

void
mem_stack_init(mem_Stack *s, void *buffer, size_t cap);

#define MEM_STACK_IMPLEMENTATION
#ifdef MEM_STACK_IMPLEMENTATION

void
mem_stack_init(mem_Stack *s, void *buffer, size_t cap)
{
    s->buffer = cast(unsigned char *)buffer;
    s->offset = 0;
    s->cap    = cap;
}

static size_t
mem_stack_calc_padding_with_header(uintptr_t address, uintptr_t align, size_t header_size)
{
    uintptr_t modulo, padding, needed_space;

    modulo       = address & (align - 1);
    padding      = 0;
    needed_space = 0;

    // Address is not aligned to an `align`-byte boundary, so get the padding
    // needed to 'bump' it up.
    if (modulo != 0) {
        padding = align - modulo;
    }

    needed_space = cast(uintptr_t)header_size;
    if (padding < needed_space) {
        needed_space -= padding;
        if ((needed_space & (align - 1)) != 0) {
            padding += align * (1 + (needed_space / align));
        } else {
            padding += align * (needed_space / align);
        }
    }
    return cast(size_t)padding;

}

void *
mem_stack_alloc_bytes_align(mem_Stack *s, size_t size, size_t align)
{
    uintptr_t curr_addr, next_addr;
    size_t padding;
    mem_Stack_Allocation_Header *header;

    if (align > 128) {
        align = 128;
    }

    curr_addr = cast(uintptr_t)s->buffer + cast(uintptr_t)s->offset;
    padding   = mem_stack_calc_padding_with_header(curr_addr, align, sizeof(*header));

    // Stack allocator couldn't fit the aligned allocation?
    if (s->offset + padding + size > s->cap) {
        return NULL;
    }
    s->offset += padding;

    next_addr = curr_addr + cast(uintptr_t)padding;
    header    = cast(mem_Stack_Allocation_Header *)(next_addr - sizeof(*header));
    header->padding = cast(uint8_t)padding;
    
    s->offset += size;
    return cast(void *)next_addr;
}


#endif // !MEM_STACK_IMPLEMENTATION

#endif // !MEM_STACK_H
