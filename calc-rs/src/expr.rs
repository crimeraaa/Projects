use std::fmt::Write;
use crate::{
    lex::{Lexeme},
    mem::{self, Alloc, Arena},
    parse::FStr,
};


pub(crate) type Ast<'s, 'a> = &'a mut FatExpr<'s, 'a>;

/// There are 2 lifetimes we are concerned with:
/// 's - This is the lifetime of the source code. This is important so we
///      can ensure the lexemes are always pointing to valid data.
///
/// 'a - This is the lifetime of the arena we were allocated with. This
///      ensures that, once the arena itself goes of score, we are no longer
///      valid.
pub(crate) struct FatExpr<'s, 'a> {
    pub expr:   Expr<'s, 'a>,
    pub lexeme: Lexeme<'s>,
}

pub(crate) enum Expr<'s, 'a> {
    // Terminals
    Bool(bool),
    Number(f64),

    // Non-terminals: Unary
    Unary {op: Unary, arg: Ast<'s, 'a>},

    // Non-terminals: Binary
    Binary {op: Binary, lhs: Ast<'s, 'a>, rhs: Ast<'s, 'a>},
}

#[derive(Clone, Copy)]
pub(crate) enum Unary {
    Neg, Not,
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

pub(crate) struct FatError<'a> {
    pub error:  Error,
    pub lexeme: Lexeme<'a>,
}

pub(crate) enum Error {
    Unary(Unary),
    Binary(Binary),
}

impl Error {
    pub(crate) fn as_str<'a, 'b>(&self, f: &'b mut FStr<'a>) -> &'b str {
        let op = match self {
            Error::Unary(op) => {
                match op {
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
        let _ = write!(f, "Attempt to perform {op} on a non-number");
        f.as_str()
    }
}

impl<'s, 'a> FatExpr<'s, 'a> {
    pub(crate) fn new(
        expr:   Expr<'s, 'a>,
        lexeme: Lexeme<'s>,
        arena:  &mut Arena<'a>,
    ) -> Result<Ast<'s, 'a>, mem::Error> {
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
            Expr::Unary { op, ref arg } => arg.unary(op),

            // Non-terminals: Binary operations
            Expr::Binary { op, ref lhs, ref rhs } => lhs.binary(op, rhs),
        }
    }

    fn unary(&self, op: Unary) -> Result<Value, FatError<'s>> {
        let lexeme = self.lexeme;
        let value  = match dbg!(self.eval()?) {
            Value::Bool(b) => {
                match op {
                    Unary::Not => Some(Value::Bool(!b)),
                    _ => None,
                }
            }

            Value::Number(f) => {
                match op {
                    Unary::Neg  => Some(Value::Number(-f)),
                    _ => None,
                }
            }
        };

        value.ok_or(FatError { error: Error::Unary(op), lexeme })
    }

    fn binary(&self, op: Binary, rhs: &Self) -> Result<Value, FatError<'s>> {
        let lhs = self;
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
}
