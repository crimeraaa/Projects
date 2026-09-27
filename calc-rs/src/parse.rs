use std::hint::unreachable_unchecked;

use crate::{
    expr::{Ast, FatExpr, Expr, Arena},
    lex::{self, Lexer, Token, FatToken, Lexeme, Pos}
};

pub(crate) struct Parser<'s> {
    file:  &'s str,
    lexer: Lexer<'s>,
    t:     FatToken<'s>,
    arena: Arena<'s>,
}

pub(crate) type Result<'s, T> = std::result::Result<T, FatError<'s>>;

pub(crate) struct FatError<'s> {
    pub error:  Error,
    pub file:   &'s str,
    pub lexeme: Lexeme<'s>,
}

pub(crate) enum Error {
    Lex(lex::Error),
    ExpectedExpr,
    ExpectedToken(Token),
    ArenaOutOfMemory,
}

pub(crate) fn program<'s>(
    file:  &'s str,
    input: &'s str,
    buf:   &'s mut [u8],
) -> Result<'s, Ast<'s>> {
    let mut p = Parser::new(file, input, buf);
    p.next_token()?;
    p.parse_expr()
}


impl<'s> Parser<'s> {
    fn new(
        file:  &'s str,
        input: &'s str,
        buf:   &'s mut [u8]
    ) -> Self {
        Self {
            file,
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
        let mut lhs = self.parse_operand()?;
        loop {
            let t        = self.t;
            let lexeme   = t.lexeme;
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
            let rhs  = self.parse_prec(rhs_prec)?;
            let expr = match t.token {
                Token::Plus     => Expr::Add(lhs, rhs),
                Token::Minus    => Expr::Sub(lhs, rhs),
                Token::Asterisk => Expr::Mul(lhs, rhs),
                Token::Slash    => Expr::Div(lhs, rhs),
                Token::Percent  => Expr::Mod(lhs, rhs),
                Token::Caret    => Expr::Pow(lhs, rhs),

                // SAFETY: We already checked the variants before parsing the rhs.
                _ => unsafe { unreachable_unchecked() },
            };

            lhs = self.alloc_expr(expr, lexeme)?;
        }
        Ok(lhs)
    }

    fn parse_operand(&mut self) -> Result<'s, Ast<'s>> {
        let t      = self.t;
        let lexeme = t.lexeme;

        match t.token {
            Token::Number(f) => {
                // Skip the number.
                self.next_token()?;
                let expr = self.alloc_expr(Expr::Number(f), lexeme)?;
                Ok(expr)
            }
            Token::Plus => {
                // Skip the unary '+' operator.
                self.next_token()?;
                let arg  = self.parse_unary()?;
                let expr = self.alloc_expr(Expr::Plus(arg), lexeme)?;
                Ok(expr)
            }

            Token::Minus => {
                // Skip the unary '-' operator.
                self.next_token()?;
                let arg  = self.parse_unary()?;
                let expr = self.alloc_expr(Expr::Neg(arg), lexeme)?;
                Ok(expr)
            }

            Token::OpenParen => {
                // Skip the opening '('.
                self.next_token()?;
                let expr = self.parse_expr()?;
                self.expect_token(Token::CloseParen)?;
                Ok(expr)
            }

            _ => Err(FatError {
                error: Error::ExpectedExpr,
                file: self.file,
                lexeme
            }),
        }
    }

    fn alloc_expr(&mut self, expr: Expr<'s>, lexeme: Lexeme<'s>) -> Result<'s, Ast<'s>> {
        let expr = FatExpr::new(expr, lexeme, &mut self.arena)
            .map_err(|_| FatError {
                error: Error::ArenaOutOfMemory,
                file: self.file,
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

            Err(FatError {
                error:  Error::ExpectedToken(expected),
                file:   self.file,
                lexeme,
            })
        }
    }

    fn next_token(&mut self) -> Result<'s, ()> {
        self.t = self.lexer
            .scan_token()
            .map_err(|e| {
                FatError {
                    error:  Error::Lex(e.error),
                    file:   self.file,
                    lexeme: e.lexeme,
                }
            })?;
        Ok(())
    }
} // impl Parser
