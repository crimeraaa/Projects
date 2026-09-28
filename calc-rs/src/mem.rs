use std::{
    ptr::NonNull,
    result,
};

pub(crate) enum Error {
    OutOfMemory,
}

pub(crate) type Result<T> = result::Result<T, Error>;

/// An allocator interface suitable for *our* needs.
/// Inspired by: https://doc.rust-lang.org/std/alloc/trait.Allocator.html
#[allow(dead_code)]
pub(crate) trait Alloc {
    /// Low-level allocation API function. All other allocation functions are
    /// implemented in terms of this.
    fn allocate_bytes(&mut self, size: usize, align: usize) -> Result<NonNull<[u8]>>;

    /// Low-level deallocation function. All other deallocation functions are
    /// implemented in terms of this.
    fn deallocate_bytes(&mut self, ptr: NonNull<u8>, size: usize);

    /// Allocates a single `T` instance.
    fn allocate<T>(&mut self) -> Result<NonNull<T>> {
        let size  = size_of::<T>();
        let align = align_of::<T>();
        let ptr   = self.allocate_bytes(size, align)?;
        Ok(ptr.cast::<T>())
    }

    /// Deallocates a single `T` instance.
    fn deallocate<T>(&mut self, ptr: NonNull<T>) {
        // We assume all pointers can be casted to byte pointers.
        let ptr   = ptr.cast::<u8>();
        let size  = size_of::<T>();
        self.deallocate_bytes(ptr, size);
    }

    /// Allocates `n` number of `T` instances.
    fn allocate_array<T>(&mut self, n: usize) -> Result<NonNull<[T]>> {
        let size  = size_of::<T>() * n;
        let align = align_of::<T>();
        let ptr   = self
            .allocate_bytes(size, align)?
            .cast::<T>();
        Ok(NonNull::slice_from_raw_parts(ptr, n))
    }

    fn deallocate_array<T>(&mut self, ptr: NonNull<[T]>) {
        let size  = size_of::<T>() * ptr.len();
        let ptr   = ptr.cast::<u8>();
        self.deallocate_bytes(ptr, size);
    }
}

pub(crate) struct Arena<'s> {
    buf: &'s mut [u8],
    curr_offset: usize,
}

impl<'s> Arena<'s> {
    pub(crate) fn new(buf: &'s mut [u8]) -> Self {
        Self{curr_offset: 0, buf}
    }
}

impl<'s> Alloc for Arena<'s> {
    fn allocate_bytes(&mut self, size: usize, align: usize) -> Result<NonNull<[u8]>> {
        let curr_offset = self.curr_offset;

        // Align forward
        let modulo      = curr_offset & (align - 1);
        let padding     = if modulo == 0 { 0 } else { align - modulo };
        let curr_offset = curr_offset + padding;

        let next_offset = curr_offset + size;
        if next_offset >= self.buf.len() {
            Err(Error::OutOfMemory)
        } else {
            let ptr = NonNull::from_ref(&self.buf[curr_offset..next_offset]);
            self.curr_offset = next_offset;
            dbg!(curr_offset, next_offset);
            Ok(ptr)
        }
    }

    /// Arenas can't deallocate single items.
    fn deallocate_bytes(&mut self, _ptr: NonNull<u8>, _size: usize) {}
}
