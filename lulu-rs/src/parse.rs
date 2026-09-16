use std::{
    fmt::{self, Display},
    result,
};

use crate::{
    lex::{self, Lexer, Token, TokenKind, Pos},
};

pub struct Parser<'a> {
    lexer:     Lexer<'a>,
    token:     Token<'a>,
    file_name: &'a str,
}

/// Transforms a lexer error into a human-readable syntax error that knows
/// where it came from.
///
/// TODO(2026-09-16): Can we optimize the size of this representation, somehow?
/// Like hold an immutable reference to the parent `Parser` or seomthing...
/// We really only need the messages once the error string is constructed.
#[derive(Debug)]
pub struct Error<'a> {
    code:      lex::Error,
    file_name: &'a str,
    token:     Token<'a>,
}

/// The parser only cares about if an error occurred or not. So the empty
/// case is our `Ok`, and the non-empty case is our `Err`.
type Result<'a> = result::Result<(), Error<'a>>;

impl<'a> Display for Error<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let source = self.file_name;
        let Pos{line, col} = self.token.pos;

        let location = self.token.as_str();
        let message  = self.code.as_str();
        write!(f, "{source}({line}:{col}) {message} at '{location}'")
    }
}

impl<'a> Error<'a> {
    fn new(code: lex::Error, file_name: &'a str, token: Token<'a>) -> Self {
        Self {code, file_name, token}
    }
}

impl<'a> Parser<'a> {
    /// Lifetime-wise, the file name should live at least as long as the actual
    /// input string. The file name could be `'static`, for example, but that
    /// detail won't matter to us because more often than not the input's
    /// lifetime is a lot shorter than that.
    pub fn new(file_name: &'a str, input: &'a str) -> Self {
         Self {
            lexer: Lexer::new(input),
            token: Token::default(),
            file_name,
         }
    }

    /// The return type is stupid, but see:
    /// https://stackoverflow.com/questions/36878044/what-is-the-idiomatic-way-to-return-an-error-from-a-function-with-no-result-if-s
    pub fn parse(&mut self) -> Result<'a> {
        let name = self.file_name;
        loop {
            self.next_token()?;

            let t = self.token;
            let Pos{line, col} = t.pos;
            match t.kind {
                TokenKind::Eof => break Ok(()),
                _ => println!("{}:{}:{}: {:?}", name, line, col, t.kind),
            }
        }
    }

    /// Reads in the next token, returning it as well for immediate use.
    /// Note that EOF is not an error, you will have to check for it
    /// manually.
    fn next_token(&mut self) -> Result<'a> {
        let curr = self.lexer.scan_token();
        self.token = curr;
        match curr.kind {
            TokenKind::Invalid(code) => Err(Error::new(code, self.file_name, curr)),
            _ => Ok(()),
        }
    }
}

