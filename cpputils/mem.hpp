#pragma once

#include "slice.hpp"

namespace mem {

struct Allocator {
    enum class Mode : u8 {
        Alloc,
        Resize,
        Free,
        Free_All,
    };

    enum class Error : u8 {
        Ok,
        Mode_Not_Implemented,
        Invalid_Argument,
        Out_Of_Memory,
    };

    using Fn = void *(*)(Mode mode,
        void *user_data,
        void *old_mem, std::size_t old_size,
        std::size_t new_size,
        std::size_t align,
        Error *err);

    Fn fn;
    void *user_data;
};

extern inline void *
alloc_bytes(Allocator allocator,
    std::size_t size,
    std::size_t align,
    Allocator::Error *err = nullptr)
{
    return allocator.fn(Allocator::Mode::Alloc,
        /*user_data =*/allocator.user_data,
        /*old_mem   =*/nullptr, /*old_size =*/0,
        /*new_size  =*/size,
        /*align     =*/align,
        err);
}

extern inline void *
resize_bytes(Allocator allocator,
    void *old_mem, std::size_t old_size,
    std::size_t new_size,
    std::size_t align,
    Allocator::Error *err = nullptr)
{
    return allocator.fn(Allocator::Mode::Resize,
        allocator.user_data,
        old_mem, old_size,
        new_size,
        align,
        err);
}

extern inline Allocator::Error
free_bytes(Allocator allocator, void *mem, std::size_t size)
{
    auto err = Allocator::Error::Ok;
    allocator.fn(Allocator::Mode::Free,
        /*user_data =*/allocator.user_data,
        /*old_mem   =*/mem, /*old_size =*/size,
        /*new_size  =*/0,
        /*align     =*/0,
        &err);
    return err;
}

template<class T>
extern inline T *
alloc_array(Allocator allocator,
    std::size_t count,
    Allocator::Error *err = nullptr)
{
    return cast(T *)alloc_bytes(allocator,
        /*size  =*/sizeof(T) * count,
        /*align =*/alignof(T),
        /*err   =*/err);
}

template<class T>
extern inline T *
resize_array(Allocator allocator,
    T *old_mem, std::size_t old_count,
    std::size_t new_count,
    Allocator::Error *err = nullptr)
{
    std::size_t old_size, new_size;

    old_size = sizeof(T) * old_count;
    new_size = sizeof(T) * new_count;
    return cast(T *)resize_bytes(allocator,
        old_mem, old_size,
        new_size,
        alignof(T),
        err);
}

template<class T>
extern inline Allocator::Error
free_array(Allocator allocator, T *mem, std::size_t count)
{
    return free_bytes(allocator, mem, sizeof(T) * count);
}

extern inline void *
zero(void *data, std::size_t len)
{
    auto p = cast(unsigned char *)data;
    for (std::size_t i = 0; i < len; i++) {
        *p++ = 0;
    }
    return p;
}


template<class T, class Z = std::size_t>
extern inline void
zero_slice(Slice<T, Z> a)
{
    for (auto &v : a) {
        zero(&v, sizeof(T));
    }
}

};
