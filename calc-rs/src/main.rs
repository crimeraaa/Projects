use std::{
    io::{self, Write},
};

mod mem;
mod lex;
mod expr;
mod parse;

use crate::{lex::Lexeme, mem::Arena, parse::FStr};

struct FatError<'s> {
    error:  Error,
    lexeme: Lexeme<'s>,
}

enum Error {
    Parse(parse::Error),
    Runtime(expr::Error),
}

fn main() -> io::Result<()> {
    let mut buf = String::with_capacity(256);
    loop {
        buf.clear();
        print!(">>> ");
        io::stdout().flush()?;

        let n = io::stdin().read_line(&mut buf)?;
        // Eof?
        if n == 0 {
            break Ok(());
        }

        let s    = &buf[..n];
        let file = "stdin";
        // AST memory buffer, or error message buffer.
        let mut buf = [0; 4096];
        match run(s, &mut buf) {
            Ok(n)  => println!("[INFO ] {n:?}"),
            Err(e) => {
                let mut f = FStr::new(&mut buf);
                let msg = e.as_str(&mut f);
                let lex::Pos{line, col} = e.lexeme.pos;
                let view = e.lexeme.view;
                println!("[ERROR] {file}({line}:{col}): {msg} at '{view}'");
            }
        }
    }
}

/// The buffer is only borrowed within the function. Once the function is over,
/// we can reuse the buffer (e.g. for formatting error messages).
fn run<'s>(input: &'s str, buf: &mut [u8]) -> Result<expr::Value, FatError<'s>> {
    // TODO(2026-09-29): Make growable and deallocate only after eval?
    let arena   = Arena::new(buf);
    let program = parse::program(input, arena)?;
    let result  = program.eval()?;
    Ok(result)
}

impl<'a> FatError<'a> {
    fn as_str(&self, f: &'a mut FStr<'a>) -> &'a str {
        match &self.error {
            Error::Parse(p)   => p.as_str(f),
            Error::Runtime(r) => r.as_str(f),
        }
    }
}

impl<'s> From<parse::FatError<'s>> for FatError<'s> {
    fn from(value: parse::FatError<'s>) -> Self {
        Self {
            error: Error::Parse(value.error),
            lexeme: value.lexeme,
        }
    }
}

impl<'s> From<expr::FatError<'s>> for FatError<'s> {
    fn from(value: expr::FatError<'s>) -> Self {
        Self {
            error: Error::Runtime(value.error),
            lexeme: value.lexeme,
        }
    }
}

