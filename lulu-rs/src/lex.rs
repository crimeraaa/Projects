use std::fmt::Write;

use crate::FStr;

pub(crate) struct Lexer<'src> {
    input: &'src str,

    /// Byte index, referring to the first index of the current lexeme.
    prev_offset: usize,

    /// Byte index, referring to the current index of the cursor.
    /// This can also be the last index of the current lexeme.
    curr_offset: usize,

    /// Track the starting line/column of the lexeme. This helps greatly
    /// simply token creation, particularly for multiline strings.
    prev_pos: Position,

    /// Track the current line/column of our current offset. This helps
    /// us track where exactly in the file we are. It's also useful for
    /// pinpointing the exact location of a lexing error.
    curr_pos: Position,
}

/// A sort of fat token that instead contains the error code, which can be
/// mapped to a static string for human readability.
#[derive(Debug)]
pub(crate) struct FatError<'src> {
    pub kind:   Error,
    pub lexeme: Lexeme<'src>,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum Error {
    UnexpectedCharacter,
    UnterminatedString,
    ExcessUnderscores,
    InvalidRadix,
    InvalidDigit(Radix),
    InvalidExponent,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum Radix {
    Binary      = 2,
    Octal       = 8,
    Decimal     = 10,
    Dozenal     = 12,
    Hexadecimal = 16,
}

/// Combines a token kind (and associated data) and its source code lexeme.
#[derive(Debug, Clone, Copy, Default)]
pub(crate) struct FatToken<'src> {
    pub kind:   Token<'src>,
    pub lexeme: Lexeme<'src>,
}

/// Describes sequences of characters, as they appear in the source code, which
/// represents valid syntactic constructs in Lulu. Note that we don't include
/// location information (i.e. the raw lexeme)- use the fat version instead.
#[repr(u8)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub(crate) enum Token<'src> {
    /// End of input stream.
    #[default]
    Eof,

    /// Keywords
    And, Break, Do, Else, Elseif, End, False, For, Function, Local,
    If, In, Nil, Not, Or, Repeat, Return, Then, True, Until, While,

    /// Balanced pairs
    /// (      )           [            ]             {          }
    OpenParen, CloseParen, OpenBracket, CloseBracket, OpenCurly, CloseCurly,

    /// Punctuations
    /// :  ;        ,      .       =   
    Colon, Semicol, Comma, Period, Assign,

    /// Bitwise operators
    /// &      |     ^
    Ampersand, Pipe, Caret,

    /// Arithmetic operators
    /// + -      *     /      %
    Plus, Minus, Star, Slash, Percent,

    /// Comparison operators
    /// ==      ~=          <         <=         >            >=
    EqualEqual, TildeEqual, LessThan, LessEqual, GreaterThan, GreaterEqual,

    Integer(i64),
    Float(f64),

    /// TODO(2026-09-14): Change to object pointer
    String(&'src str),
    Ident(&'src str),
}

/// Contains the position inforation and actual string view of a lexeme as it
/// appears in the input.
#[derive(Debug, Clone, Copy, Default)]
pub struct Lexeme<'input> {
    /// Tracks where the lexeme occurs in the input. This is useful to allow us
    /// to report line/column information so users can better pinpoint where an
    /// error occurred.
    pub pos: Position,

    /// String view into the actual token as it appears in the input. This is
    /// mainly useful for reporting errors, especially since many of the
    /// terminals in `TokenKind` don't include a data payload for us to inspect.
    pub view: &'input str,
}

/// A 2D coordinate of the line and column numbers.
#[derive(Debug, Clone, Copy, Default)]
pub(crate) struct Position {
    /// 1-based line number.
    pub line: i32,

    /// 1-based column number.
    pub col: i32,
}

impl<'src> Lexeme<'src> {
    fn new(pos: Position, view: &'src str) -> Self {
        let view = if view.is_empty() { "<eof>" } else { view };
        Self {pos, view}
    }
}

impl Error {
    pub fn as_str<'buf>(self, f: &'buf mut FStr<'buf>) -> &'buf str {
        use Error as E;
        match self {
            E::UnexpectedCharacter => "Unexpected character",
            E::UnterminatedString  => "Unterminated string",
            E::ExcessUnderscores   => "Excess underscores",
            E::InvalidRadix        => "Invalid integer radix",
            E::InvalidDigit(radix) => {
                let radix = radix as u32;
                let _     = write!(f, "Invalid base-{radix} digit");
                f.as_str()
            }
            E::InvalidExponent => "Invalid exponent",
        }
    }
}

impl<'src> Lexer<'src> {
    pub fn new(input: &'src str) -> Self {
        Lexer {
            input,
            prev_offset: 0,
            curr_offset: 0,
            prev_pos:    Position {line: 1, col: 1},
            curr_pos:    Position {line: 1, col: 1}
        }
    }

    pub fn scan_token(&mut self) -> Result<FatToken<'src>, FatError<'src>> {
        // EOF is not invalid at this point; it just signifies that there is
        // no more input to be found. However, if we reach it prematurely in
        // some of the below calls, *that* may be an error.
        let Some(c) = self.take_whitespace() else {
            return Ok(self.make_token(Token::Eof))
        };

        if c.is_alphabetic() || c == '_' {
            self.next_char(c);
            Ok(self.take_ident())
        } else if c.is_numeric() {
            match self.take_some_number(c) {
                Ok(k)  => Ok(self.make_token(k)),
                Err(e) => Err(self.make_error(e)),
            }
        } else {
            self.next_char(c);
            match self.take_char(c) {
                Ok(k)  => Ok(self.make_token(k)),
                Err(e) => Err(self.make_error(e)),
            }
        }
    }

    fn take_char(&mut self, c: char) -> Result<Token<'src>, Error> {
        use Token as T;
        Ok(match c {
            '(' => T::OpenParen,
            ')' => T::CloseParen,

            // TODO(2026-09-14): Support multi-line strings, maybe
            '[' => T::OpenBracket,
            ']' => T::CloseBracket,
            '{' => T::OpenCurly,
            '}' => T::CloseCurly,
            '&' => T::Ampersand,
            ':' => T::Colon,
            ';' => T::Semicol,
            ',' => T::Comma,
            '.' =>
                // We don't allow an underscore immediately after the period,
                // as in Python. (try `print(._1234)`, for example)
                if self.peek_curr().is_some_and(|c| c.is_digit(10)) {
                    return self.take_number(c);
                } else {
                    T::Period
                },
            '|' => T::Pipe,
            '^' => T::Caret,
            '+' => T::Plus,
            '-' => T::Minus,
            '*' => T::Star,
            '/' => T::Slash,
            '%' => T::Percent,
            '~' => if self.take_if('=') {
                T::TildeEqual
            } else {
                return Err(Error::UnexpectedCharacter);
            }
            '=' => if self.take_if('=') { T::EqualEqual   } else { T::Assign       },
            '<' => if self.take_if('=') { T::LessEqual    } else { T::LessThan     },
            '>' => if self.take_if('=') { T::GreaterEqual } else { T::GreaterThan  },
            '"' | '\'' => self.take_string(c)?,
            _   => return Err(Error::UnexpectedCharacter),
        })
    }

    fn take_ident(&mut self) -> FatToken<'src> {
        use Token as T;
        self.next_while_alphanumeric();

        let s = self.lexeme();
        self.make_token(match s {
            "and"      => T::And,
            "break"    => T::Break,
            "do"       => T::Do,
            "else"     => T::Else,
            "elseif"   => T::Elseif,
            "end"      => T::End,
            "false"    => T::False,
            "for"      => T::For,
            "function" => T::Function,
            "if"       => T::If,
            "in"       => T::In,
            "local"    => T::Local,
            "nil"      => T::Nil,
            "not"      => T::Not,
            "or"       => T::Or,
            "return"   => T::Return,
            "repeat"   => T::Repeat,
            "then"     => T::Then,
            "true"     => T::True,
            "until"    => T::Until,
            "while"    => T::While,
            _          => T::Ident(s),
        })
    }

    /// Consumes either a prefixed integer, a non-prefixed integer literal,
    /// or a float literal.
    ///
    /// Assumes we haven't necessarily skipped the leading character.
    fn take_some_number(&mut self, leader: char) -> Result<Token<'src>, Error> {
        let radix = if leader == '0' {
            // We can always safely skip '0' since it adds nothing to the
            // whole portion of an integer or a float.
            self.next_char(leader);

            let Some(prefix) = self.peek_curr() else {
                return Ok(Token::Integer(0));
            };

            let radix = match prefix {
                '.' | '0'..='9' => None, // No prefix
                'b' | 'B' => Some(Radix::Binary),
                'd' | 'D' => Some(Radix::Decimal),
                'o' | 'O' => Some(Radix::Octal),
                'x' | 'X' => Some(Radix::Hexadecimal),
                'z' | 'Z' => Some(Radix::Dozenal),
                _ => return Err(Error::InvalidRadix),
            };

            // If we have a radix, then skip the prefix character as it's not
            // part of the integer value. Otherwise, we can't know for certain
            // we need to skip it as it could be a significant digit.
            if radix.is_some() {
                self.next_char(prefix);
            }

            radix
        } else {
            None
        };

        if let Some(radix) = radix {
            let i = self.take_integer(radix)?;
            // Any trailing characters?
            if self.next_while_alphanumeric() > 0 {
                return Err(Error::InvalidDigit(radix));
            }
            Ok(Token::Integer(i))
        } else {
            // No radix also accounts for decimal literals starting with '0',
            // e.g. "01234", and float literals starting with "0.".
            self.take_number(leader)
        }
    }

    /// Assumes that our current offset is at the first digit, or a separator.
    fn take_integer(&mut self, radix: Radix) -> Result<i64, Error> {
        let radix = radix as u32;
        let mut i = 0;

        // Track if we have consecutive underscores, e.g. "1__2". For consistency
        // we'll disallow this.
        let mut prev_have;
        let mut curr_have = false;
        for c in self.peek_while(|c| c.is_digit(radix) || c == '_') {
            self.next_char(c);

            prev_have = curr_have;
            curr_have = c == '_';
            if curr_have {
                if prev_have {
                    return Err(Error::ExcessUnderscores);
                } else {
                    continue;
                }
            }

            let digit = c.to_digit(radix);

            // SAFETY: We only take digits of the specified radix OR underscores,
            // and we just tossed out underscores.
            let digit = unsafe { digit.unwrap_unchecked() };
            i *= radix as i64;
            i += digit as i64;
        }
        Ok(i)
    }

    fn take_fraction(&mut self, radix: Radix) -> Result<f64, Error> {
        let mut numerator   = 0.0;
        let mut denominator = 1.0;

        self.take_while_radix(radix, |digit, radix| {
            numerator   *= radix as f64;
            numerator   += digit as f64;
            denominator *= radix as f64;
        })?;

        Ok(numerator / denominator)
    }

    fn take_while_radix<F>(&mut self, radix: Radix, mut f: F) -> Result<(), Error>
        where F: FnMut(u32, u32)
    {
        let radix = radix as u32;
        let mut prev_have;
        let mut curr_have = false;

        // Don't call scan integer because there is a lot of custom
        // handling we need to do.
        for c in self.peek_while(|c| c.is_digit(radix) || c == '_') {
            self.next_char(c);

            prev_have = curr_have;
            curr_have = c == '_';
            if curr_have {
                if prev_have {
                    return Err(Error::ExcessUnderscores);
                }
                continue;
            }

            let digit = c.to_digit(radix as u32);

            // SAFETY: We only take digits of the specified radix OR underscores,
            // and we just tossed out underscores.
            let digit = unsafe { digit.unwrap_unchecked() };
            f(digit, radix);
        };
        Ok(())
    }

    fn take_number(&mut self, leader: char) -> Result<Token<'src>, Error> {
        let radix = Radix::Decimal;
        let whole = if leader != '.' {
            Some(self.take_integer(radix)?)
        } else {
            None
        };

        // TODO(2026-09-16): Determine if correct
        let fraction = if leader == '.' || self.take_if('.') {
            Some(self.take_fraction(radix)?)
        } else {
            None
        };

        let exponent = if self.take_if('e') || self.take_if('E') {
            let have_plus  = self.take_if('+');
            let have_minus = self.take_if('-');
            // Can have either or none, but not both!
            if have_plus && have_minus {
                return Err(Error::InvalidExponent);
            }

            // Use default first digit so we don't duplicate the actual
            // first digit.
            match i32::try_from(self.take_integer(radix)?) {
                Ok(i)  => Some(if have_minus { -i } else { i }),
                Err(_) => return Err(Error::InvalidExponent),
            }
        } else {
            None
        };

        // At this point, any trailing characters would mean we definitely have
        // an invalid number.
        if self.next_while_alphanumeric() > 0 {
            return Err(Error::InvalidDigit(radix));
        }

        // Only special case are decimal literals without any trailing chars-
        // e.g. "1234".
        if let Some(i) = whole && fraction.is_none() && exponent.is_none() {
            return Ok(Token::Integer(i));
        }

        let whole    = whole.unwrap_or(0) as f64;
        let fraction = fraction.unwrap_or(0.0);
        let exponent = exponent.map_or(1.0, |exp| 10f64.powi(exp));
        Ok(Token::Float((whole + fraction) * exponent))
    }

    fn take_string(&mut self, quote: char) -> Result<Token<'src>, Error> {
        let mut ok = false;

        // TODO(2026-09-14): Can we rewrite this using lambdas instead?
        // Don't consume the newline so we can better report the error.
        for c in self.peek_while(|c| c != '\n') {
            // For everything else, consume everything, including the quotes,
            // as we need to know how much to trim off.
            self.next_char(c);
            if c == quote {
                ok = true;
                break;
            }
        }

        // If the loop terminated naturally, then we exhausted the rest of the
        // input string, indicating we hit EOF.
        if ok {
            // Skip the quotes.
            let s = self.lexeme();
            let i = 1;
            let j = s.len() - i;
            Ok(Token::String(&s[i..j]))
        } else {
            Err(Error::UnterminatedString)
        }
    }

    fn next_while_alphanumeric(&mut self) -> usize {
        self.next_while(|c| c.is_alphanumeric() || c == '_')
    }

    fn make_token(&mut self, kind: Token<'src>) -> FatToken<'src> {
        let pos    = self.prev_pos;
        let lexeme = Lexeme::new(pos, self.lexeme());
        FatToken {kind, lexeme}
    }

    fn make_error(&mut self, err: Error) -> FatError<'src> {
        let pos    = self.curr_pos;
        let lexeme = Lexeme::new(pos, self.lexeme());
        FatError {kind: err, lexeme}
    }

    fn lexeme(&self) -> &'src str {
        let (i, j) = (self.prev_offset, self.curr_offset);
        &self.input[i..j]
    }

    /// Skips whitespaces and comments. Returns a `Some` of the first
    /// non-whitespace and non-comment character found, otherwise `None`.
    fn take_whitespace(&mut self) -> Option<char> {
        let mut res = None;
        for c in self.peek_rest() {
            // Don't advance yet because if we terminate early, then we want
            // the starting offset to refer to this non-whitespace,
            // non-comment character.
            if !c.is_whitespace() && !self.take_comment(c) {
                res = Some(c);
                break;
            }

            if c == '\n' {
                self.curr_pos.line += 1;
                self.curr_pos.col   = 0;
            }
            self.curr_pos.col += 1;
            self.next_char(c);
        };

        self.prev_offset = self.curr_offset;
        self.prev_pos    = self.curr_pos;
        res
    }

    fn take_comment(&mut self, c: char) -> bool {
        // Check to see if it's the start of a comment.
        if c == '-' && let Some(c) = self.peek_at(c.len_utf8()) {
            // We definitely have a comment of some kind?
            if c == '-' {
                // Skip both '-'.
                self.next_char(c);
                self.next_char(c);
                self.next_while(|x| x != '\n');
                return true;
            }
        }

        // Wasn't a comment, so it's a lone '-'.
        false
    }

    /// Advances the current offset as long as the given predicate for
    /// the current (iterated) character is satisfied.
    fn next_while<F>(&mut self, f: F) -> usize
        where F: Fn(char) -> bool + 'src
    {
        let mut n = 0;

        // It's important to call `next_char()` manually in case we have
        // UTF-8 input. Otherwise, if we just count the number of chars
        // iterated over, we will likely undershoot the increment of the
        // current offset.
        for c in self.peek_while(f) {
            self.next_char(c);
            n += 1;
        }
        n
    }

    /// Matches the charcter at the current offset with the expected one,
    /// advancing the current offset if the match is found.
    fn take_if(&mut self, want: char) -> bool {
        self.peek_curr()
        .is_some_and(|c| {
            let ok = c == want;
            if ok {
                self.next_char(c);
            }
            ok
        })
    }

    /// Returns the character at the current offset. If the input was exhausted,
    /// meaning we hit EOF, then `None` is returned.
    fn peek_curr(&self) -> Option<char> {
        // https://users.rust-lang.org/t/for-parsing-charindices-peek-with-offset-and-as-str/122299/2
        self.peek_at(0)
    }

    /// Returns the charater at 1 past the current offset.
    /// This operation may fail if we reach EOF.
    fn peek_at(&self, offset: usize) -> Option<char> {
        let start = self.curr_offset + offset;

        // E.g. if '-' is the very last character, don't panic!
        if start >= self.input.len() {
            None
        } else {
            self.input[start..].chars().next()
        }
    }

    /// Returns a character-wise (NOT byte-wise) iterator to the remainder
    /// of the string that satisfies the given predicate.
    ///
    /// The iterator will prematurely end at the first `false` it encounters,
    /// otherwise it will end 'naturally' once the input string is exhausted.
    fn peek_while<F>(&self, f: F) -> impl Iterator<Item = char> + 'src
        where F: Fn(char) -> bool + 'src
    {
        // The returned iterator needs to take ownership of the closure.
        self.peek_rest().take_while(move |refc| f(*refc))
    }


    /// Returns a character-wise (NOT byte-wise!) iterator to the remaining
    /// input string.
    fn peek_rest(&self) -> impl Iterator<Item = char> + 'src {
        self.input[self.curr_offset..].chars()
    }

    /// Advances the current offset by the UTF-8 length of the read character.
    fn next_char(&mut self, prev: char) {
        self.curr_offset  += prev.len_utf8();
        self.curr_pos.col += 1;
    }
}

