use std::{io::Write, hint::unreachable_unchecked, str};

use crate::{
    expr::{Ast, FatExpr, Expr, Unary, Binary},
    lex::{self, Lexer, Token, FatToken, Lexeme, Pos},
    mem::{Arena},
};

pub(crate) struct Parser<'s> {
    lexer: Lexer<'s>,
    t:     FatToken<'s>,
    arena: Arena<'s>,
}

pub(crate) type Result<'s, T> = std::result::Result<T, FatError<'s>>;

pub(crate) struct FatError<'s> {
    pub error:  Error,
    pub lexeme: Lexeme<'s>,
}

pub(crate) enum Error {
    Lex(lex::Error),
    ExpectedExpr,

    /// TODO(2026-09-28): Create a 'subset' enum so we don't have to store
    /// the number variant.
    ExpectedToken(Token),
    ArenaOutOfMemory,
}

impl Error {
    pub(crate) fn as_str<'s>(&self, mut buf: &'s mut [u8]) -> &'s str {
        match self {
            Error::Lex(e)           => e.as_str(buf),
            Error::ArenaOutOfMemory => "Arena out of memory",
            Error::ExpectedExpr     => "Expected an expression",
            Error::ExpectedToken(t) => {
                let s = t.as_str();

                // NOTE: `io::Write` is implemented for borrowed byte
                // slices, so this works.
                let _ = write!(buf, "Expected '{s}'");

                // SAFETY: We assume we only ever write ASCII strings,
                // so this should never fail because all ASCII is valid
                // UTF-8.
                unsafe { str::from_utf8_unchecked(buf) }
            }
        }
    }
}

pub(crate) fn program<'s>(input: &'s str, buf: &'s mut [u8]) -> Result<'s, Ast<'s>> {
    let mut p = Parser::new(input, buf);
    p.next_token()?;
    p.parse_expr()
}

impl<'s> Parser<'s> {
    fn new(input: &'s str, buf: &'s mut [u8]) -> Self {
        Self {
            lexer: Lexer::new(input),
            t: FatToken {
                token:  Token::Eof,
                lexeme: Lexeme {
                    view: "",
                    pos: Pos {line: 0, col: 0}
                }
            },
            arena: Arena::new(buf),
        }
    }

    fn parse_expr(&mut self) -> Result<'s, Ast<'s>> {
        self.parse_prec(1)
    }

    fn parse_unary(&mut self) -> Result<'s, Ast<'s>> {
        self.parse_prec(9)
    }

    fn parse_prec(&mut self, prec: i32) -> Result<'s, Ast<'s>> {
        // Expressions are just binary trees. We always hold the
        // root of our current expression.
        let mut tree = self.parse_operand()?;
        loop {
            let t = self.t;
            let lexeme = t.lexeme;
            let (lhs_prec, rhs_prec) = match t.token {
                Token::Plus
                    | Token::Minus => (4, 5),
                Token::Asterisk
                    | Token::Slash
                    | Token::Percent => (5, 6),
                Token::Caret => (8, 8),
                _ => break,
            };

            if lhs_prec < prec {
                break;
            }

            // Skip the operand.
            self.next_token()?;

            // Can result in right-recursion if the succeeding expression
            // is a higher precedence than us. That is intended.
            let rhs = self.parse_prec(rhs_prec)?;

            // Binary expressions take ownership of their arguments (a.k.a.
            // their child nodes).
            let lhs = tree;
            let expr = match t.token {
                Token::Plus     => Expr::Binary {op: Binary::Add, lhs, rhs},
                Token::Minus    => Expr::Binary {op: Binary::Sub, lhs, rhs},
                Token::Asterisk => Expr::Binary {op: Binary::Mul, lhs, rhs},
                Token::Slash    => Expr::Binary {op: Binary::Div, lhs, rhs},
                Token::Percent  => Expr::Binary {op: Binary::Mod, lhs, rhs},
                Token::Caret    => Expr::Binary {op: Binary::Pow, lhs, rhs},

                // SAFETY: We already checked the variants before parsing the rhs.
                _ => unsafe { unreachable_unchecked() },
            };

            // Our node is now the completed binary expression.
            // We take ownership said expression and its children.
            tree = self.alloc_expr(expr, lexeme)?;
        }
        Ok(tree)
    }

    fn parse_operand(&mut self) -> Result<'s, Ast<'s>> {
        let t      = self.t;
        let lexeme = t.lexeme;

        let expr = match t.token {
            Token::True => {
                self.next_token()?;
                self.alloc_expr(Expr::Bool(true), lexeme)?
            }

            Token::False => {
                self.next_token()?;
                self.alloc_expr(Expr::Bool(false), lexeme)?
            }

            Token::Number(f) => {
                // Skip the number.
                self.next_token()?;
                self.alloc_expr(Expr::Number(f), lexeme)?
            }

            Token::Plus => {
                // Skip the unary '+' operator.
                self.next_token()?;
                let arg  = self.parse_unary()?;
                self.alloc_expr(Expr::Unary {op: Unary::Plus, arg}, lexeme)?
            }

            Token::Minus => {
                // Skip the unary '-' operator.
                self.next_token()?;
                let arg  = self.parse_unary()?;
                self.alloc_expr(Expr::Unary {op: Unary::Neg, arg}, lexeme)?
            }

            Token::Not => {
                self.next_token()?;
                let arg = self.parse_unary()?;
                self.alloc_expr(Expr::Unary { op: Unary::Not, arg }, lexeme)?
            }

            Token::OpenParen => {
                // Skip the opening '('.
                self.next_token()?;
                let expr = self.parse_expr()?;
                self.expect_token(Token::CloseParen)?;
                expr
            }

            _ => return Err(FatError {error: Error::ExpectedExpr, lexeme}),
        };
        Ok(expr)
    }

    fn alloc_expr(&mut self, expr: Expr<'s>, lexeme: Lexeme<'s>) -> Result<'s, Ast<'s>> {
        let expr = FatExpr::new(expr, lexeme, &mut self.arena)
            .map_err(|_| FatError {
                error: Error::ArenaOutOfMemory,
                lexeme,
            })?;
        Ok(expr)
    }

    fn expect_token(&mut self, expected: Token) -> Result<'s, ()> {
        if self.t.token == expected {
            self.next_token()
        } else {
            let mut lexeme = self.t.lexeme;
            if self.t.token == Token::Eof  {
                lexeme.view = self.t.token.as_str();
            }

            Err(FatError {error:  Error::ExpectedToken(expected), lexeme})
        }
    }

    fn next_token(&mut self) -> Result<'s, ()> {
        self.t = self.lexer
            .scan_token()
            .map_err(|e| FatError {error:  Error::Lex(e.error), lexeme: e.lexeme})?;
        Ok(())
    }
} // impl Parser
