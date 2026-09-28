use std::{
    io::{self, Write},
};

mod mem;
mod lex;
mod expr;
mod parse;

use crate::lex::Lexeme;

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

        let s = &buf[..n];
        let mut buf = [0 as u8; 4096];
        let file = "stdin";
        match run(s, &mut buf) {
            Ok(n)  => println!("[INFO ] {n:?}"),
            Err(e) => {
                // TODO(2026-09-28): Can we make it so that the first buffer
                // can be re-used, as we know by now that all allocated ASTs
                // will go unused?
                let mut buf = [0 as u8; 256];
                let msg = e.as_str(&mut buf);
                let lex::Pos{line, col} = e.lexeme.pos;
                let view = e.lexeme.view;
                println!("[ERROR] {file}({line}:{col}): {msg} at '{view}'");
            }
        }
    }
}

fn run<'s>(input: &'s str, buf: &'s mut [u8]) -> Result<expr::Value, FatError<'s>> {
    let program = parse::program(input, buf)?;
    let result  = program.eval()?;
    Ok(result)
}

impl<'s> FatError<'s> {
    fn as_str(&self, buf: &'s mut [u8]) -> &'s str {
        match &self.error {
            Error::Parse(p)   => p.as_str(buf),
            Error::Runtime(r) => r.as_str(buf),
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

