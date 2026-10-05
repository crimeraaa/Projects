use crate::{
    lex::{FatToken, Lexeme},
    mem::{self, Arena},
};

pub(crate) struct FatExpr<'src, 'buf> {
    expr: Expr<'src, 'buf>,

    // Points to the starting location of the first character of this expression.
    lexeme: Lexeme<'src>,
}

#[repr(u8)]
#[derive(Clone, Copy)]
pub(crate) enum Expr<'src, 'buf>
{
    // Literals
    Nil,
    True,
    False,
    Integer(i64),
    Float(f64),
    String(&'src str),

    Unary {
        op:  Unary,
        arg: &'buf FatExpr<'src, 'buf>,
    },

    Binary {
        op:    Binary,
        left:  &'buf FatExpr<'src, 'buf>,
        right: &'buf FatExpr<'src, 'buf>,
    }
}

#[repr(u8)]
#[derive(Clone, Copy)]
pub(crate) enum Unary {
    Neg, Not,
}

#[repr(u8)]
#[derive(Clone, Copy)]
pub(crate) enum Binary {
    Add, Sub, Mul, Div, Mod, Pow,
    Eq, Neq, Lt, Leq, Gt, Geq,
}

impl<'src, 'buf> FatExpr<'src, 'buf> {
    pub fn new(
        expr:   &Expr<'src, 'buf>,
        lexeme: Lexeme<'src>,
        arena:  &mut Arena<'buf>
    ) -> Result<&'buf mut Self, mem::Error> {
        let tmp = arena.allocate()?.as_ptr();

        // SAFETY: The pointer is always valid, so we can write to it.
        unsafe { *tmp = Self { expr: *expr, lexeme }; }

        // SAFETY: The pointer is always valid, and it's now initialized,
        // so we can get a mutable reference.
        Ok(unsafe { tmp.as_mut_unchecked() })
    }

    pub fn eval(&self) {
        self.eval_n(0);
    }

    fn eval_n(&self, tab_count: i32) {
        for _ in 0..tab_count {
            print!("\t");
        }

        match self.expr {
            Expr::Nil        => println!("Nil"),
            Expr::True       => println!("True"),
            Expr::False      => println!("False"),
            Expr::Integer(i) => println!("Integer({i})"),
            Expr::Float(f)   => println!("Float({f:.14})"),
            Expr::String(s)  => println!("String({s})"),
            Expr::Unary { op: _, arg } => {
                arg.eval_n(tab_count + 1);
            }
            Expr::Binary { op: _, left, right } => {

                left.eval_n(tab_count + 1);
                right.eval_n(tab_count + 1);
            }
        }
    }
}

