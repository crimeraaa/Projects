use std::io::Write;
use crate::lex::{Lexeme};
use crate::mem::{self, Alloc, Arena};

pub(crate) type Ast<'s> = &'s mut FatExpr<'s>;

pub(crate) struct FatExpr<'s> {
    pub expr:   Expr<'s>,
    pub lexeme: Lexeme<'s>,
}

pub(crate) enum Expr<'s> {
    // Terminals
    Bool(bool),
    Number(f64),

    // Non-terminals: Unary
    Unary {op: Unary, arg: Ast<'s>},

    // Non-terminals: Binary
    Binary {op: Binary, lhs: Ast<'s>, rhs: Ast<'s>},
}

#[derive(Clone, Copy)]
pub(crate) enum Unary {
    Plus, Neg, Not,
}

#[derive(Clone, Copy)]
pub(crate) enum Binary {
    Add, Sub, Mul, Div, Mod, Pow,
}

#[derive(Debug)]
pub(crate) enum Value {
    Bool(bool),
    Number(f64),
}

pub(crate) struct FatError<'s> {
    pub error:  Error,
    pub lexeme: Lexeme<'s>,
}

pub(crate) enum Error {
    Unary(Unary),
    Binary(Binary),
}

impl Error {
    pub(crate) fn as_str<'s>(&self, buf: &'s mut [u8]) -> &'s str {
        let op = match self {
            Error::Unary(op) => {
                match op {
                    Unary::Plus => "unary plus",
                    Unary::Neg  => "unary negation",
                    Unary::Not  => "unary 'not'",
                }
            }

            Error::Binary(op) => {
                match op {
                    Binary::Add => "addition",
                    Binary::Sub => "subtraction",
                    Binary::Mul => "multiplication",
                    Binary::Div => "division",
                    Binary::Mod => "modulo",
                    Binary::Pow => "exponentiation",
                }
            }
        };

        // We assume this operation never fails.
        let _ = write!(&mut buf[..], "Attempt to perform {op} on a non-number");

        // SAFETY: The string is always ASCII.
        unsafe { std::str::from_utf8_unchecked(buf) }
    }
}

impl<'s> FatExpr<'s> {
    pub(crate) fn new(
        expr:   Expr<'s>,
        lexeme: Lexeme<'s>,
        arena:  &mut Arena<'s>,
    ) -> Result<Ast<'s>, mem::Error> {
        // SAFETY: Since we our happy path is a non-null, well-aligned,
        // non-dangling pointer, we assume it can be treated as a reference.
        let fat = unsafe { arena.allocate()?.as_mut() };
        *fat = Self {expr, lexeme};
        Ok(fat)
    }

    pub(crate) fn eval(&self) -> Result<Value, FatError<'s>> {
        match self.expr {
            // Terminals: Literal values
            Expr::Bool(b)   => Ok(Value::Bool(b)),
            Expr::Number(f) => Ok(Value::Number(f)),

            // Non-terminals: Unary operations
            Expr::Unary { op, ref arg } => unary(op, arg),

            // Non-terminals: Binary operations
            Expr::Binary { op, ref lhs, ref rhs } => binary(op, lhs, rhs),
        }
    }
}

fn unary<'s>(op: Unary, arg: &FatExpr<'s>) -> Result<Value, FatError<'s>> {
    let lexeme = arg.lexeme;
    let value  = match dbg!(arg.eval()?) {
        Value::Bool(b) => {
            match op {
                Unary::Not => Some(Value::Bool(!b)),
                _ => None,
            }
        }

        Value::Number(f) => {
            match op {
                Unary::Plus => Some(Value::Number( f)),
                Unary::Neg  => Some(Value::Number(-f)),
                _ => None,
            }
        }
    };

    value.ok_or_else(|| FatError { error: Error::Unary(op), lexeme })
}

fn binary<'s>(
    op: Binary,
    lhs: &FatExpr<'s>,
    rhs: &FatExpr<'s>
) -> Result<Value, FatError<'s>> {
    let a = dbg!(lhs.eval()?);
    let b = dbg!(rhs.eval()?);
    match (a, b) {
        (Value::Number(a), Value::Number(b)) => {
            let res = match op {
                Binary::Add => dbg!(a + b),
                Binary::Sub => dbg!(a - b),
                Binary::Mul => dbg!(a * b),
                Binary::Div => dbg!(a / b),
                Binary::Mod => dbg!(a % b),
                Binary::Pow => dbg!(a.powf(b)),
            };
            Ok(Value::Number(res))
        }

        // Only lhs is correct?
        (Value::Number(_), _) => {
            let lexeme = rhs.lexeme;
            Err(FatError { error: Error::Binary(op), lexeme })
        }

        // Only rhs is correct, or neither are correct?
        (_, Value::Number(_))
            | (_, _) => {
            let lexeme = lhs.lexeme;
            Err(FatError { error: Error::Binary(op), lexeme })
        }
    }

}
