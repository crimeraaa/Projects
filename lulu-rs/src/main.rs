use std::{
    io::{self, Write},
};

mod lex;

fn tokenize(name: &str, input: &str) {
    let mut x = lex::Lexer::new(input);
    println!("Parsing \"{name}\"...");
    loop {
        let t = x.scan_token();
        match t.kind {
            lex::TokenKind::Eof => break,
            lex::TokenKind::Invalid(_) => {
                println!("{}:{}", name, t);
                break;
            }
            _ => println!("{}:{}", name, t),
        }
    }
}

fn main() {
    // It sucks that we basically *have* to use the heap
    let mut buf = String::with_capacity(256);
    loop {
        buf.clear();
        print!(">>> ");
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
        tokenize("stdin", s);
    }
}
