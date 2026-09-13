#[derive(Copy, Clone, Debug)]
pub struct Pos {
    pub line: i32,
    pub col:  i32,
}

#[repr(u8)]
#[derive(Debug, Eq, PartialEq)]
pub enum TokenKind {
    Invalid,

    // Keywords
    // And, Break, Do, Else, Elseif, End, False, For, Function, Local,
    // If, In, Nil, Not, Or, Repeat, Return, Then, True, Until, While,

    //  (      )           [            ]             {          }
    OpenParen, CloseParen, OpenBracket, CloseBracket, OpenCurly, CloseCurly,

    // =
    Assign,

    //  Bitwise
    //  &      |     ^
    Ampersand, Pipe, Caret,

    // Arithmetic
    // +  -      *     /      %
    Plus, Minus, Star, Slash, Percent,

    // Comparison
    // ==       ~=          <         <=         >            >=
    EqualEqual, TildeEqual, LessThan, LessEqual, GreaterThan, GreaterEqual,

    // Integer,
    // Float,
    // String,
    // Ident,

    Eof,
}

#[derive(Debug)]
pub struct Token<'a> {
    pub kind:   TokenKind,
    pub lexeme: &'a str,
    pub pos:    Pos,
}

pub struct Lexer<'a> {
    input: &'a str,

    // Byte index, referring to the first index of the current lexeme.
    prev_offset: u32,

    // Byte index, referring to the current index of the cursor,
    // or the ending index of the current lexeme.
    curr_offset: u32,

    prev_col: i32,

    // Track the current line/column information.
    pos: Pos,
}

impl<'a> Lexer<'a> {
    pub fn new(input: &'a str) -> Self {
        Lexer{
            input,
            prev_offset: 0,
            curr_offset: 0,
            prev_col: 0,
            pos: Pos{line: 1, col: 1}
        }
    }

    pub fn scan_token(&mut self) -> Token<'a> {
        use TokenKind::*;

        let c = match self.skip_whitespace() {
            None    => return self.make_token(Eof),
            Some(c) => self.next(c),
        };

        let t = match c {
            '(' => OpenParen,
            ')' => CloseParen,
            '[' => OpenBracket,
            ']' => CloseBracket,
            '{' => OpenCurly,
            '}' => CloseCurly,
            '&' => Ampersand,
            '|' => Pipe,
            '^' => Caret,
            '+' => Plus,
            '-' => Minus,
            '*' => Star,
            '/' => Slash,
            '%' => Percent,
            '~' => if self.ate('=') { TildeEqual   } else { Invalid     },
            '=' => if self.ate('=') { EqualEqual   } else { Assign      },
            '<' => if self.ate('=') { LessEqual    } else { LessThan    },
            '>' => if self.ate('=') { GreaterEqual } else { GreaterThan },
            _   => Invalid,
        };

        self.make_token(t)
    }

    fn make_token(&self, kind: TokenKind) -> Token<'a> {
        let lexeme = self.to_string();
        let line   = self.pos.line;
        let col    = self.prev_col;
        Token{kind, lexeme, pos: Pos{line, col}}
    }

    fn to_string(&self) -> &'a str {
        &self.input[(self.prev_offset as usize)..(self.curr_offset as usize)]
    }

    fn skip_whitespace(&mut self) -> Option<char> {
        let mut r = None;
        while !self.is_eof() {
            let c = self.peek();
            // Whitespaces are not included the grammar, so trim all of them.
            if c.is_whitespace() {
                if c == '\n' {
                    self.pos.line += 1;
                    self.pos.col = 0;
                }
                self.pos.col += 1;
                self.next(c);
                continue;
            }

            if !self.skip_comment(c) {
                r = Some(c);
                break;
            }
        };

        self.prev_offset = self.curr_offset;
        self.prev_col    = self.pos.col;
        r
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

    fn next_while<F>(&mut self, f: F) -> i32 where F: Fn(char) -> bool {
        let mut n = 0;
        while !self.is_eof() {
            let c = self.peek();
            if !f(c) {
                break;
            }
            self.next(c);
            n += 1;
        }
        n
    }

    fn ate(&mut self, want: char) -> bool {
        let c  = self.peek();
        let ok = c == want;
        if ok {
            self.next(c);
        }
        ok
    }

    fn peek(&self) -> char {
        // https://users.rust-lang.org/t/for-parsing-charindices-peek-with-offset-and-as-str/122299/2
        self.peek_at(0).unwrap()
    }

    fn peek_at(&self, offset: usize) -> Option<char> {
        let start = (self.curr_offset as usize) + offset;
        if start >= self.input.len() {
            None
        } else {
            self.input[start..].chars().next()
        }
    }

    fn next(&mut self, prev: char) -> char {
        self.curr_offset  += prev.len_utf8() as u32;
        self.pos.col += 1;
        prev
    }

    fn is_eof(&self) -> bool {
        self.curr_offset as usize >= self.input.len()
    }
}

