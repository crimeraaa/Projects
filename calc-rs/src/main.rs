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
        match run("stdin", s) {
            Ok(n) => println!("[INFO ] {n}"),
            Err(e) => {
                use parse::Error as E;
                let msg = match e.error {
                    E::Lex(e) => e.as_str(),
                    E::ExpectedAnExpression => "Expected an expression",
                    E::Expected(t) => &t.to_string(),
                };

                let file = e.file;
                let lex::Pos{line, col} = e.lexeme.pos;
                let view = e.lexeme.view;
                println!("[ERROR] {file}({line}:{col}): {msg} at '{view}'");
            }
        }
    }
}

fn run<'s>(file_name: &'s str, input: &'s str) -> parse::Result<'s, f64> {
    let program = parse::program(file_name, input)?;
    Ok(program.eval())
}
