use std::{
    io::{self, Write},
};

mod lex;
mod expr;
mod parse;

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
        match run("stdin", s, &mut buf) {
            Ok(n)  => println!("[INFO ] {n}"),
            Err(e) => {
                use parse::Error as E;
                let msg = match e.error {
                    E::Lex(e)           => e.as_str(),
                    E::ArenaOutOfMemory => "Arena out of memory",
                    E::ExpectedExpr     => "Expected an expression",
                    E::ExpectedToken(t) => &format!("Expected '{}'", t.as_str()),
                };

                let file = e.file;
                let lex::Pos{line, col} = e.lexeme.pos;
                let view = e.lexeme.view;
                println!("[ERROR] {file}({line}:{col}): {msg} at '{view}'");
            }
        }
    }
}

fn run<'s>(file_name: &'s str, input: &'s str, buf: &'s mut [u8]) -> parse::Result<'s, f64> {
    let program = parse::program(file_name, input, buf)?;
    Ok(program.eval())
}
