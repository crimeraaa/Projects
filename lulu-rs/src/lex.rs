use std::{
    fmt,
    str::Chars,
};

#[derive(Clone, Copy, Debug)]
pub struct Pos {
    /// 1-based line number.
    pub line: i32,

    /// 1-based col number.
    pub col: i32,
}

#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum TokenKind<'a> {
    Invalid(&'static str),

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
    Ident,

    Eof,
}

#[derive(Clone, Copy, Debug)]
pub struct Token<'a> {
    pub kind:   TokenKind<'a>,
    pub lexeme: &'a str,
    pub pos:    Pos,
}

impl<'a> fmt::Display for Token<'a> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
        write!(f, "{}:{}: {:?} = \"{}\"", self.pos.line, self.pos.col, self.kind, self.lexeme)
    }
}

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
            Some(c) => self.next(c),
        };

        if c.is_alphabetic() || c == '_' {
            self.scan_ident(c)
        } else if c.is_numeric() {
            self.scan_number(c)
        } else {
            if let Some(t) = self.scan_char(c) {
                self.make_token(t)
            } else {
                self.make_error_token("Unexpected character")
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
            '.' => Period,
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

    fn scan_ident(&mut self, leader: char) -> Token<'a> {
        use TokenKind::*;

        self.next(leader);
        self.next_while(|c| c.is_alphanumeric() || c == '_');
        self.make_token(match self.to_string() {
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
            _          => Ident,
        })
    }

    fn scan_number(&mut self, leader: char) -> Token<'a> {
        if leader == '0' {
            self.scan_integer(leader)
        } else {
            self.scan_float(leader)
        }
    }

    fn scan_integer(&mut self, leader: char) -> Token<'a> {
        let mut have_radix = true;
        let radix: u32 = match self.peek() {
            'b' | 'B' => 2,  // Binary
            'd' | 'D' => 10, // Decimal
            'o' | 'O' => 8,  // Octal
            'x' | 'X' => 16, // Hexadecimal
            'z' | 'Z' => 12, // Dozenal
            _ => {
                if leader.is_digit(10) {
                    have_radix = false;
                    10
                } else {
                    return self.make_error_token("Invalid radix")
                }
            }
        };

        // Maximal munch, we don't care (yet) about validity. We'll check that
        // when we attempt to actually parse the integer.
        self.next_while(|c| c.is_alphanumeric() || c == '_');

        // If we had an explicit radix, skip them when parsing the string.
        let i = if have_radix { 2 } else { 0 };
        let s = &self.to_string()[i..];
        self.make_integer_token(s, radix)
    }

    fn make_integer_token(&mut self, s: &str, radix: u32) -> Token<'a> {
        let mut i: i64 = 0;

        // Track if we have consecutive underscores.
        let mut curr_have = false;
        for c in s.chars() {
            let prev_have = curr_have;
            curr_have = c == '_';
            if curr_have {
                if prev_have {
                    return self.make_error_token("Multiple consecutive underscores");
                }
                continue;
            }

            if let Some(d) = c.to_digit(radix) {
                i *= radix as i64;
                i += d as i64;
            } else {
                return self.make_error_token("Invalid radix digit");
            }
        }
        self.make_token(TokenKind::Integer(i))
    }

    fn scan_float(&mut self, leader: char) -> Token<'a> {
        let _ = leader;

        self.next_while_decimal();

        // Maximal munch, even for obviously-invalid literals like 1.2.3.4
        let mut frac_len = 0;
        let mut exp_len  = 0;
        let mut num_iter = 0;

        /*
         The valid grammar for floats is as follows:

         float     = digits frac-exp ;
         digits    = digit digit-sep digits
                   | "" ;

         digit     = '0' | '1' | '2' | '3' | '4'
                   | '5' | '6' | '7' | '8' | '9' ;

         digit-sep = '_'
                   | "" ;

         frac-exp  = fraction | exponent
         fraction  = '.' digits exponent0;
         exponent0 = exponent1 | "" ;
         exponent1 = Ee sign digits ;
         Ee        = 'e' | 'E' ;
         sign      = '+' | '-' | "" ;
         */
        while self.eat('.') {
            num_iter += 1;
            // Fraction
            frac_len += 1;
            frac_len += self.next_while_decimal();
            
            // Exponent
            if self.eat('e') || self.eat('E') {
                exp_len += 1;

                // Exponent's sign (optional)
                exp_len += self.eat('+') as usize;
                exp_len += self.eat('-') as usize;

                // Exponent must always have some number of decimal digits
                // after it.
                exp_len += self.next_while_decimal();
            }
        }

        // If we consumed multiple radix points and/or multiple exponents,
        // then we definitely have an invalid literal.
        if num_iter > 1 {
            return self.make_error_token("Malformed floating-point literal");
        }

        // TODO(2026-09-14): Can we implement this manually so we can allow underscores?
        if frac_len == 0 && exp_len == 0 {
            self.make_integer_token(self.to_string(), 10)
        } else {
            match self.to_string().parse::<f64>() {
                Ok(f) => self.make_token(TokenKind::Float(f)),

                // TODO(2026-09-14): Figure out how to get more meaningful error messages
                Err(_) => self.make_error_token("Invalid floating-point literal"),
            }
        }
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
            if self.next(c) == quote {
                ok = true;
                break;
            }
        }

        // If the loop terminated naturally, then we exhausted the rest of the
        // input string, indicating we hit EOF.
        if ok {
            // Skip the quotes.
            let s = self.to_string();
            let i = 1;
            let j = s.len() - i;
            TokenKind::String(&s[i..j])
        } else {
            TokenKind::Invalid("Unterminated string")
        }
    }

    fn next_while_decimal(&mut self) -> usize {
        self.next_while(|c| c.is_digit(10) || c == '_')
    }

    fn make_token(&mut self, kind: TokenKind<'a>) -> Token<'a> {
        let lexeme = self.to_string();
        let line   = self.pos.line;
        let col    = self.start_col;
        Token{kind, lexeme, pos: Pos{line, col}}
    }

    fn make_error_token(&mut self, message:  &'static str) -> Token<'a> {
        self.make_token(TokenKind::Invalid(message))
    }

    fn to_string(&self) -> &'a str {
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
            self.next(c);
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
                self.next(c);
                self.next(c);
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
        let n = self.input[self.curr_offset..]
            .chars()
            .take_while(|x| f(*x))
            .count();

        self.curr_offset += n;
        n
    }

    /// Matches the charcter at the current offset with the expected one,
    /// advancing the current offset if the match is found.
    fn eat(&mut self, want: char) -> bool {
        let c  = self.peek();
        let ok = c == want;
        if ok {
            self.next(c);
        }
        ok
    }

    /// Returns the character at the current offset. If the input was exhausted,
    /// then the default value is returned as a safeguard. We assume that no
    /// UTF-8 character will ever match the default value.
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
    fn next(&mut self, prev: char) -> char {
        self.curr_offset += prev.len_utf8();
        self.pos.col     += 1;
        prev
    }
}

