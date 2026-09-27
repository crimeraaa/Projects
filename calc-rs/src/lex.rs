use std::{fmt::{self, Display}, num::TryFromIntError};

pub(crate) struct Lexer<'s> {
    input: &'s str,
    
    /// Starting index of the potential token.
    prev_offset: usize,

    /// Current index of our cursor. Often 1 past the last definite index
    /// of the potential token.
    curr_offset: usize,

    /// Starting line and column of the potential token's lexeme.
    /// It's useful to keep this around, especially for multi-line strings,
    /// as we cannot guarantee that we can calculate the starting column when
    /// multiple lines are involves.
    prev_pos: Pos,

    /// Line and column of the current cursor index.
    curr_pos: Pos,
}

#[derive(Debug, Clone, Copy)]
pub(crate) struct FatToken<'s> {
    pub token:  Token,
    pub lexeme: Lexeme<'s>,
}

pub(crate) struct FatError<'s> {
    pub error:  Error,
    pub lexeme: Lexeme<'s>,
}

#[repr(u8)]
pub(crate) enum Error {
    UnexpectedCharacter,
    InvalidBase,
    InvalidBinaryDigit,
    InvalidOctalDigit,
    InvalidDecimalDigit,
    InvalidDozenalDigit,
    InvalidHexadecimalDigit,
    IntegerOverflow,
    InvalidExponent,
    ExcessUnderscores,
}

impl Error {
    pub(crate) fn as_str(self) -> &'static str {
        match self {
            Self::UnexpectedCharacter     => "Unexpected character",
            Self::InvalidBase             => "Invalid integer base",
            Self::InvalidBinaryDigit      => "Invalid binary (base-2) digit",
            Self::InvalidOctalDigit       => "Invalid octal (base-8) digit",
            Self::InvalidDecimalDigit     => "Invalid decimal (base-10) digit",
            Self::InvalidDozenalDigit     => "Invalid dozenal (base-12) digit",
            Self::InvalidHexadecimalDigit => "Invalid hexadecimal (base-16) digit",
            Self::IntegerOverflow         => "Integer overflow",
            Self::InvalidExponent         => "Invalid exponent digit",
            Self::ExcessUnderscores       => "Excess underscores",
        }
    }
}

impl From<TryFromIntError> for Error {
    /// # Assumptions
    /// -   We only do this for integer-integer conversions. The only possible
    ///     error in these cases is if the destination type is smaller than the
    ///     source type and cannot hold the source value (i.e. a lossy conversion).
    fn from(_value: TryFromIntError) -> Self {
        Error::IntegerOverflow
    }
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub(crate) enum Token {
    /// End-of-file. Not an actual token that can be viewed. If you received
    /// this token, that means the input stream was 'naturally' exhausted
    /// and that there are no more tokens to get.
    Eof,

    /// Terminals: Groupings
    /// `( ) [ ] { }`
    OpenParen, CloseParen, OpenBracket, CloseBracket, OpenCurly, CloseCurly,

    /// Terminals: Separators
    Colon, Semicolon, Comma, Period,

    /// Terminals: arithmetic operators
    /// `+ - * / % ^`
    Plus, Minus, Asterisk, Slash, Percent, Caret,

    /// Non-terminals
    Number(f64),
}

#[derive(Clone, Copy)]
enum Radix {
    Binary      = 2,
    Octal       = 8,
    Decimal     = 10,
    Dozenal     = 12,
    Hexadecimal = 16,
}

impl Display for Token {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            Self::Eof          => "Eof(<eof>)",

            // Terminals: Groupings
            Self::OpenParen    => "OpenParen('(')",
            Self::CloseParen   => "CloseParen(')')",
            Self::OpenBracket  => "OpenBracket('[')",
            Self::CloseBracket => "CloseBracket(']')",
            Self::OpenCurly    => "OpenCurly('{')",
            Self::CloseCurly   => "CloseCurly('}')",

            // Terminals: Separators
            Self::Colon        => "Colon(':')",
            Self::Semicolon    => "Semicolon(';')",
            Self::Comma        => "Comma(',')",
            Self::Period       => "Period('.')",

            // Terminals: Arithmetic operators
            Self::Plus         => "Plus('+')",
            Self::Minus        => "Minus('-')",
            Self::Asterisk     => "Asterisk('*')",
            Self::Slash        => "Slash('/')",
            Self::Percent      => "Percent('%')",
            Self::Caret        => "Caret('^')",
            Self::Number(n)    => &format!("Number({n})"),
        };
        write!(f, "{s}")
    }
} // impl Display for Token

impl Token {
    pub(crate) fn as_str(&self) -> &'static str {
        match self {
            Self::Eof          => "<eof>",

            // Terminals: Groupings
            Self::OpenParen    => "(",
            Self::CloseParen   => ")",
            Self::OpenBracket  => "[",
            Self::CloseBracket => "]",
            Self::OpenCurly    => "{",
            Self::CloseCurly   => "}",

            // Terminals: Separators
            Self::Colon        => ":",
            Self::Semicolon    => ";",
            Self::Comma        => ",",
            Self::Period       => ".",

            // Terminals: Arithmetic operators
            Self::Plus         => "+",
            Self::Minus        => "-",
            Self::Asterisk     => "*",
            Self::Slash        => "/",
            Self::Percent      => "%",
            Self::Caret        => "^",
            Self::Number(_)    => "<number>",
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub(crate) struct Lexeme<'s> {
    pub view: &'s str,
    pub pos: Pos,
}

#[derive(Debug, Clone, Copy)]
pub(crate) struct Pos {
    pub line: i32,
    pub col: i32,
}

/// Check if the given integer can be safely converted to the desired floating
/// point type without loss of information.
///
/// # Returns
/// * `Ok`  - The integer converted to a float.
/// * `Err` - A lexer error indicating what went wrong.
fn f64_try_from_i64(i: i64) -> Result<f64, Error> {
    let f = i as f64;
    if f as i64 == i {
        Ok(f)
    } else {
        Err(Error::IntegerOverflow)
    }
}

impl<'s> Lexer<'s> {
    /// Creates a new lexer (a.k.a. token scanner) on the given input. Note that
    /// we are bound only to the input's lifetime or shorter.
    pub(crate) fn new(input: &'s str) -> Self {
        let pos = Pos{line: 1, col: 1};
        Self {
            input,
            prev_offset: 0,
            curr_offset: 0,
            prev_pos:    pos,
            curr_pos:    pos
        }
    }

    pub(crate) fn scan_token(&mut self) -> Result<FatToken<'s>, FatError<'s>> {
        self.skip_whitespace();

        // Don't advance yet- when we scan numbers, we need to see the first
        // digit.
        let c = match self.peek() {
            None    => return Ok(self.make_token(Token::Eof)),
            Some(c) => c,
        };

        if c.is_ascii_digit() {
            match self.scan_number(c) {
                Ok(k)  => Ok(self.make_token(k)),
                Err(e) => Err(self.make_error(e)),
            }
        } else {
            self.advance();
            let token = match c {
                // Terminals: Groupings
                '(' => Token::OpenParen,
                ')' => Token::CloseParen,
                '[' => Token::OpenBracket,
                ']' => Token::CloseBracket,
                '{' => Token::OpenCurly,
                '}' => Token::CloseCurly,

                // Terminals: Separators
                ':' => Token::Colon,
                ';' => Token::Semicolon,
                ',' => Token::Comma,
                '.' =>
                    if self.peek().is_some_and(|c| c.is_ascii_digit()) {
                        self.scan_number(c)
                            .map_err(|e| self.make_error(e))?
                    } else {
                        Token::Period
                    }

                // Terminals: Arithmetic Operators
                '+' => Token::Plus,
                '-' => Token::Minus,
                '*' => Token::Asterisk,
                '/' => Token::Slash,
                '%' => Token::Percent,
                '^' => Token::Caret,
                _ => return Err(self.make_error(Error::UnexpectedCharacter))
            };
            Ok(self.make_token(token))
        }
    }

    fn scan_number(&mut self, leader: char) -> Result<Token, Error> {
        if leader == '0' {
            // No matter the digit sequence, we don't use leading zeroes for
            // anything meaningful other than indicating we *might* have a
            // prefixed integer.
            self.advance();
            let prefix = match self.peek() {
                None => return Ok(Token::Number(0.0)),
                Some(c) => c,
            };

            let radix = match prefix {
                'b' | 'B' => Some(Radix::Binary),
                'd' | 'D' => Some(Radix::Decimal),
                'o' | 'O' => Some(Radix::Octal),
                'x' | 'X' => Some(Radix::Hexadecimal),
                'z' | 'Z' => Some(Radix::Dozenal),
                _ => if prefix.is_ascii_digit() || prefix == '.' {
                    None
                } else {
                    return Err(Error::InvalidBase)
                }
            };

            if let Some(radix) = radix {
                // Skip the prefix character because we know it's definitely part
                // of an explicit integer base. Its digit value is not used.
                self.advance();
                let i = self.scan_integer(radix)?;
                let f = f64_try_from_i64(i)?;
                return Ok(Token::Number(f));
            }
        }

        let whole = if leader != '.' {
            let i = self.scan_integer(Radix::Decimal)?;
            let f = f64_try_from_i64(i)?;
            Some(f)
        } else {
            None
        };

        let fraction = if leader == '.' || self.consumed('.') {
            Some(self.scan_fraction()?)
        } else {
            None
        };

        let exponent = if self.consumed('e') || self.consumed('E') {
            let have_plus  = self.consumed('+');
            let have_minus = self.consumed('-');
            
            // We can have one of '+' or '-' (or neither), but not both.
            if have_plus && have_minus {
                return Err(Error::InvalidExponent);
            }

            let e = self.scan_integer(Radix::Decimal)?;
            Some(i32::try_from(e)?)
        } else {
            None
        };

        let mantissa = whole.unwrap_or(0.0) + fraction.unwrap_or(0.0);
        let exponent = exponent.map_or(1.0, |i| 10f64.powi(i));
        Ok(Token::Number(mantissa * exponent))
    }

    fn scan_integer(&mut self, radix: Radix) -> Result<i64, Error> {
        let mut integer: i64 = 0;
        let mut prev_have;
        let mut curr_have = false;
        for c in
            self.remaining()
                .chars()
                .take_while(|c| c.is_digit(radix as u32) || *c == '_')
        {
            self.advance();
            prev_have = curr_have;
            curr_have = c == '_';
            if curr_have {
                if prev_have {
                    return Err(Error::ExcessUnderscores);
                } else {
                    continue;
                }
            }

            // We end up relying on the Rust implementation of UTF-8, so the
            // iterator eagerly accepts many different kinds of numeric characters.
            // However we can only convert the ASCII range.
            if let Some(digit) = c.to_digit(radix as u32) {
                // Perform `i = i*radix + digit`, checking for overflow on each
                // operation.
                //
                // SAFETY: We assume the `as` casts are always safe because
                // we go from smaller integer types to larger ones.
                integer = integer
                    .checked_mul(radix as i64)
                    .and_then(|i| i.checked_add(digit as i64))
                    .ok_or(Error::IntegerOverflow)?;
            } else {
                return Err(match radix {
                    Radix::Binary      => Error::InvalidBinaryDigit,
                    Radix::Octal       => Error::InvalidOctalDigit,
                    Radix::Decimal     => Error::InvalidDecimalDigit,
                    Radix::Dozenal     => Error::InvalidDozenalDigit,
                    Radix::Hexadecimal => Error::InvalidHexadecimalDigit,
                });
            }
        };
        Ok(integer)
    }

    /// Scans a base-10 fractional sequence.
    /// # Assumptions
    /// -   We just consumed a '.' character.
    fn scan_fraction(&mut self) -> Result<f64, Error> {
        let radix = 10;
        let mut numerator   = 0.0;
        let mut denominator = 1.0;
        let mut prev_have;
        let mut curr_have = false;
        for c in
            self.remaining()
                .chars()
                .take_while(|c| c.is_ascii_digit() || *c == '_')
        {
            self.advance();
            prev_have = curr_have;
            curr_have = c == '_';
            if curr_have {
                if prev_have {
                    return Err(Error::ExcessUnderscores);
                } else {
                    continue;
                }
            }

            // SAFETY: The iterator takes only ASCII digits (i.e. '0'..='9') or
            // underscores. We already checked for underscores, so the only
            // remaining possibilities are the base-10 characters.
            let digit = unsafe { c.to_digit(radix).unwrap_unchecked() };

            // TODO(2026-09-27): Check for validity/overflow?
            numerator   *= radix as f64;
            numerator   += digit as f64;
            denominator *= radix as f64;
        }
        Ok(numerator / denominator)
    }

    fn make_token(&self, token: Token) -> FatToken<'s> {
        let view = &self.input[self.prev_offset..self.curr_offset];
        let pos  = self.prev_pos;
        let lexeme = Lexeme {pos, view};
        FatToken {token, lexeme}
    }

    fn make_error(&self, error: Error) -> FatError<'s> {
        let view = &self.input[self.prev_offset..self.curr_offset];
        let view = if view.len() == 0 { "<eof>" } else { view };
        let Pos{line, col} = self.curr_pos;
        let col = col - 1;
        let lexeme = Lexeme {pos: Pos {line, col}, view};
        FatError {error, lexeme}
    }

    /// # Assumptions
    /// -   The current offset can be validly used to slice to the end of the
    ///     input string (i.e. it can be exactly the length).
    fn skip_whitespace(&mut self) {
        for c in self.remaining().chars() {
            if c.is_whitespace() {
                if c == '\n' {
                    self.curr_pos.line += 1;
                    // Will be incremented to 1 on the advance.
                    self.curr_pos.col = 0;
                }
                self.advance();
                continue;
            }
            break;
        }

        self.prev_pos    = self.curr_pos;
        self.prev_offset = self.curr_offset;
    }

    /// Returns an immutable view into the remainder of the input string, starting
    /// at the current cursor offset. This is useful so we don't have to constantly
    /// check for EOF at every loop iteration.
    fn remaining(&self) -> &'s str {
        &self.input[self.curr_offset..]
    }

    fn consumed(&mut self, wanted: char) -> bool {
        let ok = self.peek().is_some_and(|c| c == wanted);
        if ok {
            self.advance();
        }
        ok
    }

    fn advance(&mut self) {
        self.curr_offset  += 1;
        self.curr_pos.col += 1;
    }

    fn peek(&self) -> Option<char> {
        self.peek_at(0)
    }

    fn peek_at(&self, offset: usize) -> Option<char> {
        let offset = self.curr_offset + offset;
        if offset >= self.input.len() {
            None
        } else {
            self.input[offset..].chars().next()
        }
    }
} // impl Lexer
