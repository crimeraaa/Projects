use std::hint::unreachable_unchecked;

use crate::{expr::{Ast, FatExpr, Expr}, lex::{self, Lexer, Token, FatToken, Lexeme, Pos}};

pub(crate) struct Parser<'s> {
    file:  &'s str,
    lexer: Lexer<'s>,
    t: FatToken<'s>,
}

pub(crate) type Result<'s, T> = std::result::Result<T, FatError<'s>>;

pub(crate) struct FatError<'s> {
    pub error:  Error,
    pub file:   &'s str,
    pub lexeme: Lexeme<'s>,
}

pub(crate) enum Error {
    Lex(lex::Error),
    ExpectedAnExpression,
    Expected(Token),
}

pub(crate) fn program<'s>(file: &'s str, input: &'s str) -> Result<'s, Ast<'s>> {
    let mut p = Parser::new(file, input);
    p.next_token()?;
    p.parse_expr()
}


impl<'s> Parser<'s> {
    fn new(file: &'s str, input: &'s str) -> Self {
        Self {
            file,
            lexer: Lexer::new(input),
            t: FatToken {
                token: Token::Eof,
                lexeme: Lexeme {
                    view: "",
                    pos: Pos {line: 0, col: 0}
                }
            }
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
            let lhs_prec = match t.token {
                Token::Plus | Token::Minus => 4,
                Token::Asterisk | Token::Slash | Token::Percent => 5,
                Token::Caret => 8,
                _ => break,
            };

            if lhs_prec < prec {
                break;
            }

            // Enforce right-associativity for exponentiation. All other binary
            // operations are left-associative.
            let rhs_prec = if t.token == Token::Caret { lhs_prec } else {lhs_prec + 1 };

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

            lhs = Ast::new(FatExpr {expr, lexeme});
        }

        Ok(lhs)
    }

    fn parse_operand(&mut self) -> Result<'s, Ast<'s>> {
        let t      = self.t;
        let lexeme = t.lexeme;

        // Skip the number, operator, or open grouping.
        self.next_token()?;
        match t.token {
            Token::Number(f) => Ok(Ast::new(FatExpr {expr: Expr::Number(f), lexeme})),
            Token::Plus => {
                let arg  = self.parse_unary()?;
                let expr = Ast::new(FatExpr {expr: Expr::Plus(arg), lexeme});
                Ok(expr)
            }

            Token::Minus => {
                let arg  = self.parse_unary()?;
                let expr = Ast::new(FatExpr {expr: Expr::Neg(arg), lexeme});
                Ok(expr)
            }

            Token::OpenParen => {
                let expr = self.parse_expr()?;
                self.expect_token(Token::CloseParen)?;
                Ok(expr)
            }

            _ => Err(FatError {
                error: Error::ExpectedAnExpression,
                file: self.file,
                lexeme
            }),
        }
    }

    fn expect_token(&mut self, expected: Token) -> Result<'s, ()> {
        if self.t.token == expected {
            self.next_token()
        } else {
            Err(FatError {
                error:  Error::Expected(self.t.token),
                file:   self.file,
                lexeme: self.t.lexeme,
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
}
