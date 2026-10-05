use std::ptr::NonNull;

pub(crate) enum Error {
    OutOfMemory,
}

pub(crate) struct Arena<'buf> {
    prev_offset: usize,
    curr_offset: usize,
    buffer: &'buf mut [u8],
}

impl<'buf> Arena<'buf> {
    pub fn new(buffer: &'buf mut [u8]) -> Self {
        Self {prev_offset: 0, curr_offset: 0, buffer}
    }

    pub fn allocate<T>(&mut self) -> Result<NonNull<T>, Error> {
        let size  = size_of::<T>();
        let align = align_of::<T>();

        // We assume the buffer is `usize`-aligned, so index 0 is always suitably
        // aligned for all types we care about. This means we don't need to use
        // pointer-to-integer casts when checking alignment, we can just check
        // the indexes themselves.
        let start  = self.curr_offset;
        let modulo = start & (align - 1);
        let start  = if modulo != 0 {
            // Current offset isn't aligned so bump it up to the first index that
            // represents the next suitably aligned address in the buffer.
            start + (align - modulo)
        } else {
            // Current offset is already aligned properly, or possibly even
            // overaligned. We don't really care either way.
            start
        };

        let stop = start + size;
        if stop <= self.buffer.len() {
            let ptr = &mut self.buffer[start] as *mut u8 as *mut T;
            self.prev_offset = start;
            self.curr_offset = stop;

            // SAFETY: If we pass the bounds check, we always have a valid
            // index and thus a valid pointer.
            Ok(unsafe { NonNull::new_unchecked(ptr) })
        } else {
            Err(Error::OutOfMemory)
        }
    }
}
