use std::fmt::Write;

use crate::{
    FStr, expr::{Expr, FatExpr}, lex::{self, FatToken, Lexeme, Lexer, Position, Token}, mem::Arena, 
};

type Ast<'src, 'buf> = &'buf mut FatExpr<'src, 'buf>;

pub(crate) struct Parser<'src, 'buf> {
    lexer: Lexer<'src>,
    token: FatToken<'src>,
    arena: Arena<'buf>,
}

/// Transforms a lexer error into a human-readable syntax error that knows
/// where it came from.
///
/// TODO(2026-09-16): Can we optimize the size of this representation, somehow?
/// Like hold an immutable reference to the parent `Parser` or seomthing...
/// We really only need the messages once the error string is constructed.
pub(crate) struct FatError<'src> {
    error:  Error,
    lexeme: Lexeme<'src>,
}

#[derive(Clone, Copy)]
pub(crate) enum Error {
    Lex(lex::Error),
    ArenaOutOfMemory,
    ExpectedToken(Expect),
    ExpectedExpr,
}

#[repr(u8)]
#[derive(Clone, Copy)]
pub(crate) enum Expect {
    Eof,
    CloseParen,
}

impl<'src> FatError<'src> {
    pub(crate) fn as_str<'buf>(&self, f: &'buf mut FStr<'buf>) -> &'buf str {
        use Error as E;
        let mut tmp = [0 as u8; 64];
        let mut tmp = FStr::new(&mut tmp);
        let msg = match self.error {
            E::Lex(e)           => e.as_str(&mut tmp),
            E::ArenaOutOfMemory => "Arena out of memory",
            E::ExpectedExpr     => "Expected an expression",
            E::ExpectedToken(e) => {
                let s = e.as_str();
                let _ = write!(&mut tmp, "Expected '{s}'");
                tmp.as_str()
            }
        };

        let Position{line, col} = self.lexeme.pos;
        let view = self.lexeme.view;

        // SAFETY: We assume that formatting error messages never fails.
        let _ = write!(f, "({line}:{col}): {msg} at '{view}'");
        f.as_str()
    }
}

impl Expect {
    fn as_token(&self) -> Token<'_> {
        match self {
            Expect::Eof        => Token::Eof,
            Expect::CloseParen => Token::CloseParen,
        }
    }

    fn as_str(&self) -> &'static str {
        match self {
            Expect::Eof        => "<eof>",
            Expect::CloseParen => ")",
        }
    }
}

impl<'src, 'buf> Parser<'src, 'buf>
where
    'src: 'buf
{
    /// Lifetime-wise, the file name should live at least as long as the actual
    /// input string. The file name could be `'static`, for example, but that
    /// detail won't matter to us because more often than not the input's
    /// lifetime is a lot shorter than that.
    pub(crate) fn new(input: &'src str, buffer: &'buf mut [u8]) -> Self {
         Self {
            lexer: Lexer::new(input),
            token: FatToken::default(),
            arena: Arena::new(buffer),
         }
    }

    pub(crate) fn parse(&mut self) -> Result<(), FatError<'src>> {
        self.next_token()?;
        let expr = self.parse_expr()?;
        self.expect_token(Expect::Eof)?;
        expr.eval();
        Ok(())
    }

    fn parse_expr(&mut self) -> Result<Ast<'src, 'buf>, FatError<'src>> {
        self.parse_operand()
    }

    fn parse_operand(&mut self) -> Result<Ast<'src, 'buf>, FatError<'src>> {
        let token = self.token;
        self.next_token()?;

        let expr = match token.kind {
            Token::Nil       => Expr::Nil,
            Token::True      => Expr::True,
            Token::False     => Expr::False,
            Token::OpenParen => {
                let expr = self.parse_expr()?;
                self.expect_token(Expect::CloseParen)?;
                return Ok(expr);
            }

            Token::Integer(i) => Expr::Integer(i),
            Token::Float(f)   => Expr::Float(f),
            Token::String(s)  => Expr::String(s),
            Token::Ident(_s)  => todo!(),

            _ => return Err(FatError {
                error:  Error::ExpectedExpr,
                lexeme: token.lexeme
            })
        };

        Ok(self.alloc_expr(&token, &expr)?)
    }

    fn alloc_expr(
        &mut self,
        token: &FatToken<'src>,
        expr:  &Expr<'src, 'buf>
    ) -> Result<Ast<'src, 'buf>, FatError<'src>> {
        let error  = Error::ArenaOutOfMemory;
        let lexeme = token.lexeme;
        let expr   = FatExpr::new(expr, lexeme, &mut self.arena)
            .map_err(|_| FatError { error, lexeme })
            ?;
        Ok(expr)
    }

    fn expect_token(&mut self, expected: Expect) -> Result<(), FatError<'src>> {
        if self.token.kind == expected.as_token() {
            Ok(())
        } else {
            let error  = Error::ExpectedToken(expected);
            let lexeme = self.token.lexeme;
            Err(FatError {error, lexeme})
        }
    }

    /// Reads in the next token, returning it as well for immediate use.
    /// Note that EOF is not an error, you will have to check for it
    /// manually.
    fn next_token(&mut self) -> Result<(), FatError<'src>> {
        let curr = self.lexer
            .scan_token()
            .map_err(|e| FatError {
                error:  Error::Lex(e.kind),
                lexeme: e.lexeme
            })?;

        self.token = curr;
        Ok(())
    }
}

