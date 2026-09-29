use std::{fmt::{self, Write}, hint::unreachable_unchecked, str};

use crate::{
    expr::{Ast, FatExpr, Expr, Unary, Binary},
    lex::{self, Lexer, Token, FatToken, Lexeme, Pos},
    mem::{Arena},
};

struct Parser<'s, 'a> {
    lexer: Lexer<'s>,
    t:     FatToken<'s>,
    arena: Arena<'a>,
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
    pub(crate) fn as_str<'a>(&self, f: &'a mut FStr<'a>) -> &'a str {
        match self {
            Error::Lex(e)           => e.as_str(f),
            Error::ArenaOutOfMemory => "Arena out of memory",
            Error::ExpectedExpr     => "Expected an expression",
            Error::ExpectedToken(t) => {
                let s = t.as_str();

                // NOTE: `io::Write` is implemented for borrowed byte
                // slices, so this works.
                let _ = write!(f, "Expected '{s}'\0");
                f.as_str()
            }
        }
    }
}

pub(crate) struct FStr<'a> {
    /// The underlying buffer we wish to write to.
    /// Length thereof is our capacity.
    buf: &'a mut [u8],

    /// Number of bytes (not chars!) written so far.
    written: usize,
}

impl<'a> FStr<'a> {
    pub(crate) fn new(buf: &'a mut [u8]) -> Self {
        Self {buf, written: 0}
    }

    pub(crate) fn as_str(&self) -> &str {
        let n = self.written;
        let s = &self.buf[0..n];

        // SAFETY: We assume we only ever write ASCII strings. Since we sliced
        // exactly the written bounds, this should never fail
        unsafe { str::from_utf8_unchecked(s) }
    }

    fn used(&self) -> usize {
        self.written
    }

    fn left(&self) -> usize {
        self.buf.len()
    }
}

impl<'s> Write for FStr<'s> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let start = self.used();
        let stop  = start + s.len();
        if stop <= self.left() {
            let dst = &mut self.buf[start..stop];
            self.written = stop;
            dst.copy_from_slice(s.as_bytes());
            Ok(())
        } else {
            Err(fmt::Error)
        }
    }
}

/// Converts a source text to an AST. The AST is stored in the given arena.
pub(crate) fn program<'s, 'a>(input: &'s str, arena: Arena<'a>) -> Result<'s, Ast<'s, 'a>> {
    let mut p = Parser::new(input, arena);
    p.next_token()?;

    let tree = p.parse_expr()?;
    p.expect_token(Token::Eof)?;
    Ok(tree)
}

impl<'s, 'a> Parser<'s, 'a> {
    fn new(input: &'s str, arena: Arena<'a>) -> Self {
        Self {
            lexer: Lexer::new(input),
            t: FatToken {
                token:  Token::Eof,
                lexeme: Lexeme {
                    view: "",
                    pos: Pos {line: 0, col: 0}
                }
            },
            arena,
        }
    }

    fn parse_expr(&mut self) -> Result<'s, Ast<'s, 'a>> {
        self.parse_prec(1)
    }

    fn parse_unary(&mut self) -> Result<'s, Ast<'s, 'a>> {
        self.parse_prec(9)
    }

    fn parse_prec(&mut self, prec: i32) -> Result<'s, Ast<'s, 'a>> {
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

    fn parse_operand(&mut self) -> Result<'s, Ast<'s, 'a>> {
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

            Token::Minus => {
                // Skip the unary '-' operator.
                self.next_token()?;
                let arg  = self.parse_unary()?;
                self.alloc_expr(Expr::Unary { op: Unary::Neg, arg }, lexeme)?
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

            _ => return Err(FatError { error: Error::ExpectedExpr, lexeme }),
        };
        Ok(expr)
    }

    fn alloc_expr(&mut self, expr: Expr<'s, 'a>, lexeme: Lexeme<'s>) -> Result<'s, Ast<'s, 'a>> {
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
