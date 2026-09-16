use std::{
    // Needed so we can call flush.
    io::{self, Write},
};

mod value;
mod code;
mod lex;
mod parse;
mod run;

fn main() {
    repl(">>> ");
}

fn repl(prompt: &'static str) {
    // It sucks that we basically *have* to use the heap
    let mut buf = String::with_capacity(256);
    loop {
        buf.clear();
        print!("{}", prompt);
        io::stdout().flush().unwrap();
        
        match io::stdin().read_line(&mut buf) {
            Ok(n) => {
                // Reached EOF?
                if n == 0 {
                    println!();
                    break;
                }
                println!("[INFO ] Read {n} bytes.");
            },
            Err(err) => {
                println!("[ERROR]: {err}");
                break;
            },
        };

        let s = buf.trim();
        println!("Input: \"{s}\"");

        let mut p = parse::Parser::new("stdin", s);
        if let Err(e) = p.parse() {
            println!("{e}");
        }
    }
}

