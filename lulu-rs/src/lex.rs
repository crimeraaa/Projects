use std::{
    str::Chars,
};

pub struct Lexer<'a> {
    input: &'a str,

    /// Byte index, referring to the first index of the current lexeme.
    start_offset: usize,

    /// Byte index, referring to the current index of the cursor.
    /// This can also be the last index of the current lexeme.
    curr_offset: usize,

    /// We track the starting column of the lexeme to simplify token creation,
    /// especially for multi-line strings.
    start_col: i32,

    /// Track the current line/column information.
    pos: Pos,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Error {
    UnexpectedCharacter,
    UnterminatedString,
    ExcessUnderscores,
    InvalidRadix,
    InvalidDigit,
    InvalidExponent,
}

impl Error {
    pub fn as_str(self) -> &'static str {
        use Error::*;
        match self {
            UnexpectedCharacter => "Unexpected character",
            UnterminatedString  => "Unterminated string",
            ExcessUnderscores   => "Excess underscores",
            InvalidRadix        => "Invalid integer radix",
            InvalidDigit        => "Invalid digit",
            InvalidExponent     => "Invalid exponent",
        }
    }
}

/// Technically, `TokenKind` is all we need to implement a working parser,
/// and that could be just called `Token` instead. However, if we want to
/// report meaningful errors, then we need to track the location information
/// for each token.
#[derive(Debug, Clone, Copy, Default)]
pub struct Token<'a> {
    pub kind:   TokenKind<'a>,

    /// String view into the actual token as it appears in the input. This is
    /// mainly useful for reporting errors, especially since many of the
    /// terminals in `TokenKind` don't include a data payload for us to inspect.
    pub lexeme: &'a str,

    /// Like the lexeme, but instead tracks where the token occurs in the input.
    /// This is useful to report line/column information so users can better
    /// pinpoint where an error occurred.
    pub pos: Pos,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub enum TokenKind<'a> {
    /// End of input stream.
    #[default]
    Eof,

    /// Contains the error code. We include this here, rather than using a
    /// result type, because errors also need to know their locations.
    Invalid(Error),

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
    String(&'a str),
    Ident(&'a str),
}

#[derive(Debug, Clone, Copy, Default)]
pub struct Pos {
    /// 1-based line number.
    pub line: i32,

    /// 1-based col number.
    pub col: i32,
}

impl<'a> Token<'a> {
    pub fn as_str(&self) -> &str {
        match self.kind {
            // More often than not, EOF has an empty lexeme.
            TokenKind::Eof => "<eof>",

            // String literals' lexemes include the quotes, so use the payload
            // instead.
            TokenKind::String(s) | TokenKind::Ident(s) => s,
            _ => self.lexeme,
        }
    }
}

impl<'a> Lexer<'a> {
    pub fn new(input: &'a str) -> Self {
        Lexer {
            input,
            start_offset: 0,
            curr_offset:  0,
            start_col:    0,
            pos:          Pos {line: 1, col: 1}
        }
    }

    pub fn scan_token(&mut self) -> Token<'a> {
        // EOF is not invalid at this point; it just signifies that there is
        // no more input to be found. However, if we reach it prematurely in
        // some of the below calls, *that* may be an error.
        let c = match self.skip_whitespace() {
            None    => return self.make_token(TokenKind::Eof),
            Some(c) => self.next_char(c),
        };

        if c.is_alphabetic() || c == '_' {
            self.scan_ident()
        } else if c.is_numeric() {
            match self.scan_number(c) {
                Ok(tk) => self.make_token(tk),
                Err(e) => self.make_error_token(e),
            }
        } else {
            if let Some(t) = self.scan_char(c) {
                self.make_token(t)
            } else {
                self.make_error_token(Error::UnexpectedCharacter)
            }
        }
    }

    fn scan_char(&mut self, c: char) -> Option<TokenKind<'a>> {
        use TokenKind::*;
        Some(match c {
            '(' => OpenParen,
            ')' => CloseParen,

            // TODO(2026-09-14): Support multi-line strings, maybe
            '[' => OpenBracket,
            ']' => CloseBracket,
            '{' => OpenCurly,
            '}' => CloseCurly,
            '&' => Ampersand,
            ':' => Colon,
            ';' => Semicol,
            ',' => Comma,
            '.' => if self.peek().is_digit(10) {
                match self.scan_float(c) {
                    Ok(f)  => f,
                    Err(_) => return None,
                }
            } else {
                Period
            },
            '|' => Pipe,
            '^' => Caret,
            '+' => Plus,
            '-' => Minus,
            '*' => Star,
            '/' => Slash,
            '%' => Percent,
            '~' => if self.eat('=') { TildeEqual   } else { return None; },
            '=' => if self.eat('=') { EqualEqual   } else { Assign       },
            '<' => if self.eat('=') { LessEqual    } else { LessThan     },
            '>' => if self.eat('=') { GreaterEqual } else { GreaterThan  },
            '"' | '\'' => self.scan_string(c),
            _   => return None,
        })
    }

    fn scan_ident(&mut self) -> Token<'a> {
        use TokenKind::*;
        self.next_while_alphanumeric();

        let s = self.lexeme();
        self.make_token(match s {
            "and"      => And,
            "break"    => Break,
            "do"       => Do,
            "else"     => Else,
            "elseif"   => Elseif,
            "end"      => End,
            "false"    => False,
            "for"      => For,
            "function" => Function,
            "if"       => If,
            "in"       => In,
            "local"    => Local,
            "nil"      => Nil,
            "not"      => Not,
            "or"       => Or,
            "return"   => Return,
            "repeat"   => Repeat,
            "then"     => Then,
            "true"     => True,
            "until"    => Until,
            "while"    => While,
            _          => Ident(s),
        })
    }

    /// Assumes we already skipped the leader itself, meaning our current
    /// offset is at the first potential digit.
    fn scan_number(&mut self, leader: char) -> Result<TokenKind<'a>, Error> {
        let prefix = self.peek();
        let radix = if leader == '0' {
            match dbg!(prefix) {
                '.' | '0'..='9' => None, // No prefix
                'b' | 'B' => Some(2),  // Binary
                'd' | 'D' => Some(10), // Decimal
                'o' | 'O' => Some(8),  // Octal
                'x' | 'X' => Some(16), // Hexadecimal
                'z' | 'Z' => Some(12), // Dozenal
                _ => return Err(Error::InvalidRadix),
            }
        } else {
            None
        };

        if let Some(radix) = radix {
            // We already skipped '0', now skip the radix prefix.
            self.next_char(prefix);
            let i = self.scan_integer(None, radix)?;
            if self.next_while_alphanumeric() > 0 {
                return Err(Error::InvalidDigit);
            }
            Ok(TokenKind::Integer(i))
        } else {
            // No radix also accounts for decimal literals starting with '0',
            // e.g. "01234", and float literals starting with "0.".
            self.scan_float(leader)
        }
    }

    /// Assumes that our current offset is at the first digit, or a separator.
    /// The grammar thereof is as follows:
    ///
    /// int-literal = dec-digits
    ///     | "0b" SEP bin-digits
    ///     | "0o" SEP oct-digits
    ///     | "0d" SEP dec-digits
    ///     | "0x" SEP hex-digits
    ///     | "0z" SEP doz-digits
    ///     ;
    ///
    /// bin-digits = BIN-DIGIT SEP bin-digits | "" ;
    /// oct-digits = OCT-DIGIT SEP oct-digits | "" ;
    /// dec-digits = DEC-DIGIT SEP dec-digits | "" ;
    /// hex-digits = HEX-DIGIT SEP hex-digits | "" ;
    /// doz-digits = DOZ-DIGIT SEP doz-digits | "" ;
    ///
    /// BIN-DIGIT  = '0' | '1' ;
    /// OCT-DIGIT  = BIN-DIGIT | '2' | '3' | '4' | '5' | '6' | '7' ;
    /// DEC-DIGIT  = OCT-DIGIT | '8' | '9' ;
    /// DOZ-DIGIT  = DEC-DIGIT | A | B ;
    /// HEX-DIGIT  = DOZ-DIGIT | C | D | E | F ;
    ///
    /// SEP = '_' | ""  ;
    /// A   = 'A' | 'a' ;
    /// B   = 'B' | 'b' ;
    /// C   = 'C' | 'c' ;
    /// D   = 'D' | 'd' ;
    /// E   = 'E' | 'e' ;
    /// F   = 'F' | 'f' ;
    ///
    fn scan_integer(&mut self, first: Option<char>, radix: u32) -> Result<i64, Error> {
        let mut i = match first.unwrap_or('0').to_digit(radix) {
            Some(i) => i as i64,
            None    => return Err(Error::InvalidDigit),
        };

        // Track if we have consecutive underscores, e.g. "1__2". For consistency
        // we'll disallow this.
        let mut curr_have = false;
        for c in self.peek_rest().take_while(|c| c.is_digit(radix) || *c == '_') {
            self.next_char(c);

            let prev_have = curr_have;
            curr_have = c == '_';
            if curr_have {
                if prev_have {
                    return Err(Error::ExcessUnderscores);
                }
                continue;
            }

            if let Some(d) = c.to_digit(radix) {
                i *= radix as i64;
                i += d as i64;
            } else {
                return Err(Error::InvalidDigit);
            }
        }
        Ok(i)
    }

    /// The valid grammar for float literals is as follows.
    ///
    /// number    = digits fraction
    ///           | fraction
    ///           ;
    ///
    /// digits    = digit '_'? digits | "" ;
    /// digit     = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9' ;
    /// fraction  = '.' digits exponent?
    /// exponent  = Ee sign digits
    /// Ee        = 'E' | 'e' ;
    /// sign      = '+' | '-' | "" ;
    ///
    fn scan_float(&mut self, leader: char) -> Result<TokenKind<'a>, Error> {
        let int_part = if leader != '.' {
            Some(self.scan_integer(Some(leader), 10)?)
        } else {
            None
        };

        // TODO(2026-09-16): Determine if correct
        let frac_part = if leader == '.' || self.eat('.') {
            let mut numerator   = 0.0;
            let mut denominator = 1.0;

            let mut curr_have = false;

            // Don't call scan integer because there is a lot of custom
            // handling we need to do.
            for c in self.peek_rest().take_while(|c| c.is_digit(10) || *c == '_') {
                self.next_char(c);

                let prev = curr_have;
                curr_have = c == '_';
                if curr_have {
                    if prev {
                        return Err(Error::ExcessUnderscores);
                    }
                    continue;
                }

                if let Some(digit) = c.to_digit(10) {
                    denominator *= 10.0;
                    numerator   *= 10.0;
                    numerator   += digit as f64;
                } else {
                    return Err(Error::InvalidDigit)
                }
            }
            Some(numerator / denominator)
        } else {
            None
        };

        let exp_part = if self.eat('e') || self.eat('E') {
            let have_plus  = self.eat('+');
            let have_minus = self.eat('-');
            // Can have either or none, but not both!
            if have_plus && have_minus {
                return Err(Error::UnexpectedCharacter);
            }

            // Use default first digit so we don't duplicate the actual
            // first digit.
            match i32::try_from(self.scan_integer(None, 10)?) {
                Ok(i)  => Some(if have_minus { -i } else { i }),
                Err(_) => return Err(Error::InvalidExponent),
            }
        } else {
            None
        };

        Ok(match (int_part, frac_part, exp_part) {
            (Some(i), None,    None)    => TokenKind::Integer(i),
            (None,    Some(f), None)    => TokenKind::Float(f),
            (Some(i), Some(f), None)    => TokenKind::Float((i as f64) + f),
            // Can't have lone exponent, that would be parsed as an identifier.

            (Some(i), None,    Some(exp)) => {
                let mantissa = i as f64;
                let pow10    = 10f64.powi(exp);
                TokenKind::Float(mantissa * pow10)
            }


            (None,    Some(frac), Some(exp)) => {
                let mantissa = frac;
                let pow10    = 10f64.powi(exp);
                TokenKind::Float(mantissa * pow10)
            }


            (Some(i), Some(frac), Some(exp)) => {
                let mantissa = (i as f64) + frac;
                let pow10    = 10f64.powi(exp);
                TokenKind::Float(mantissa * pow10)
            }

            _ => todo!(),
        })
    }

    fn scan_string(&mut self, quote: char) -> TokenKind<'a> {
        let mut ok = false;

        // TODO(2026-09-14): Can we rewrite this using lambdas instead?
        for c in self.peek_rest() {
            // Don't consume the newline so we can better report the error.
            if c == '\n' {
                break;
            }

            // For everything else, consume everything, including the quotes,
            // as we need to know how much to trim off.
            if self.next_char(c) == quote {
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
            TokenKind::String(&s[i..j])
        } else {
            TokenKind::Invalid(Error::UnterminatedString)
        }
    }

    fn next_while_alphanumeric(&mut self) -> usize {
        self.next_while(|c| c.is_alphanumeric() || c == '_')
    }

    fn make_token(&mut self, kind: TokenKind<'a>) -> Token<'a> {
        let lexeme = self.lexeme();
        let line   = self.pos.line;
        let col    = self.start_col;
        Token {kind, lexeme, pos: Pos {line, col}}
    }

    fn make_error_token(&mut self, err: Error) -> Token<'a> {
        self.make_token(TokenKind::Invalid(err))
    }

    fn lexeme(&self) -> &'a str {
        let (i, j) = (self.start_offset, self.curr_offset);
        &self.input[i..j]
    }

    /// Skips whitespaces and comments. Returns `Some(char)` if there was any
    /// non-whitespace and non-comment character found, otherwise `None`.
    fn skip_whitespace(&mut self) -> Option<char> {
        let mut res = None;
        for c in self.peek_rest() {
            // Don't advance yet because if we terminate early, then we want
            // the starting offset to refer to this non-whitespace,
            // non-comment character.
            if !c.is_whitespace() && !self.skip_comment(c) {
                res = Some(c);
                break;
            }

            if c == '\n' {
                self.pos.line += 1;
                self.pos.col   = 0;
            }
            self.pos.col += 1;
            self.next_char(c);
        };

        self.start_offset = self.curr_offset;
        self.start_col    = self.pos.col;
        res
    }

    fn skip_comment(&mut self, c: char) -> bool {
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
        where F: Fn(char) -> bool
    {
        let mut n = 0;

        // It's important to call `next_char()` manually in case we have
        // UTF-8 input. Otherwise, if we just count the number of chars
        // iterated over, we will likely undershoot the increment of the
        // current offset.
        for c in self.peek_rest().take_while(|refc| f(*refc)) {
            self.next_char(c);
            n += 1;
        }
        n
    }

    /// Matches the charcter at the current offset with the expected one,
    /// advancing the current offset if the match is found.
    fn eat(&mut self, want: char) -> bool {
        let c  = self.peek();
        let ok = c == want;
        if ok {
            self.next_char(c);
        }
        ok
    }

    /// Returns the character at the current offset. If the input was exhausted,
    /// meaning we hit EOF, then the default value (i.e. `0 as char`) is returned
    /// as a safeguard. For simplicity we assume that no valid UTF-8 character will
    /// ever match the default value.
    fn peek(&self) -> char {
        // https://users.rust-lang.org/t/for-parsing-charindices-peek-with-offset-and-as-str/122299/2
        self.peek_at(0).unwrap_or_default()
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

    /// Returns an iterator to the remaining input.
    fn peek_rest(&self) -> Chars<'a> {
        self.input[self.curr_offset..].chars()
    }

    /// Advances the current offset by the UTF-8 length of the read character.
    /// Returns the given character so you can keep using it.
    fn next_char(&mut self, prev: char) -> char {
        self.curr_offset += prev.len_utf8();
        self.pos.col     += 1;
        prev
    }
}

