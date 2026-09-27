use std::{ptr::NonNull};

use crate::lex;

pub(crate) type Ast<'s> = &'s mut FatExpr<'s>;

pub(crate) struct FatExpr<'s> {
    pub expr:   Expr<'s>,
    pub lexeme: lex::Lexeme<'s>,
}

pub(crate) enum Expr<'s> {
    // Terminals
    Number(f64),

    // Non-terminals: Unary
    Plus(Ast<'s>),
    Neg(Ast<'s>),

    // Non-terminals: Binary
    Add(Ast<'s>, Ast<'s>),
    Sub(Ast<'s>, Ast<'s>),
    Mul(Ast<'s>, Ast<'s>),
    Div(Ast<'s>, Ast<'s>),
    Mod(Ast<'s>, Ast<'s>),
    Pow(Ast<'s>, Ast<'s>),
}

pub(crate) struct Arena<'s> {
    buf: &'s mut [u8],
    prev_offset: usize,
    curr_offset: usize,
}

impl<'s> FatExpr<'s> {
    pub(crate) fn new(
        expr:   Expr<'s>,
        lexeme: lex::Lexeme<'s>,
        arena:  &mut Arena,
    ) -> Result<Ast<'s>, AllocatorError> {
        // SAFETY: Since we our happy path is a non-null pointer, we assume
        // it can be treated as a reference.
        let fat = unsafe { arena.allocate::<Self>()?.as_mut() };
        *fat = Self {expr, lexeme};
        Ok(fat)
    }

    pub(crate) fn eval(&self) -> f64 {
        match &self.expr {
            Expr::Number(f) => dbg!(*f),
            Expr::Plus(arg) => dbg!(arg.eval()),
            Expr::Neg(arg)  => dbg!(-arg.eval()),

            // Non-terminals: Binary
            Expr::Add(left, right) => dbg!(left.eval() + right.eval()),
            Expr::Sub(left, right) => dbg!(left.eval() - right.eval()),
            Expr::Mul(left, right) => dbg!(left.eval() * right.eval()),
            Expr::Div(left, right) => dbg!(left.eval() / right.eval()),
            Expr::Mod(left, right) => dbg!(left.eval() % right.eval()),
            Expr::Pow(left, right) => dbg!(left.eval().powf(right.eval())),
        }
    }
}

impl<'s> Arena<'s> {
    pub(crate) fn new(buf: &'s mut [u8]) -> Self {
        Self{prev_offset: 0, curr_offset: 0, buf}
    }
}

pub(crate) enum AllocatorError {
    OutOfMemory,
}

impl<'s> Arena<'s> {
    pub(crate) fn allocate<T>(&mut self) -> Result<NonNull<T>, AllocatorError> {
        let size  = size_of::<T>();
        let align = align_of::<T>();
        let ptr   = self.allocate_bytes(size, align)?;
        let ptr   = ptr.cast::<T>();
        Ok(ptr)
    }

    fn allocate_bytes(&mut self, size: usize, align: usize) -> Result<NonNull<[u8]>, AllocatorError> {
        let curr_offset = self.curr_offset;

        // Align forward
        let modulo      = curr_offset & (align - 1);
        let padding     = if modulo == 0 { 0 } else { align - modulo };
        let curr_offset = curr_offset + padding;

        let next_offset = curr_offset + size;
        if next_offset >= self.buf.len() {
            Err(AllocatorError::OutOfMemory)
        } else {
            let ptr = NonNull::from_ref(&self.buf[curr_offset..next_offset]);
            self.prev_offset = curr_offset;
            self.curr_offset = next_offset;
            dbg!(curr_offset, next_offset);
            Ok(ptr)
        }
    }
}
