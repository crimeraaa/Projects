use crate::lex;

pub(crate) type Ast<'s> = Box<FatExpr<'s>>;

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

impl<'s> FatExpr<'s> {
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
