use std::{
    // Needed so we can call flush.
    io::{self, Write},
    fmt,
};

mod value;
mod code;
mod lex;
mod parse;
mod mem;
mod expr;
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

        let src = buf.trim();
        println!("Input: \"{src}\"");

        let mut buf = [0 as u8; 4096];
        let file    = "stdin";
        let mut p   = parse::Parser::new(src, &mut buf);
        if let Err(e) = p.parse() {
            let mut f = FStr::new(&mut buf);
            let msg   = e.as_str(&mut f);
            println!("{file}:{msg}");
        }
    }
}

pub(crate) struct FStr<'buf> {
    buffer: &'buf mut [u8],
    written: usize,
}

impl<'buf> FStr<'buf> {
    fn new(buffer: &'buf mut [u8]) -> Self {
        Self {buffer, written: 0}
    }

    pub(crate) fn as_str(&self) -> &str {
        // SAFETY: We only every write ASCII strings.
        unsafe { str::from_utf8_unchecked(&self.buffer[..self.written]) }
    }
}

impl<'buf> fmt::Write for FStr<'buf> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let i   = self.written;
        let j   = i + s.len();
        let cap = self.buffer.len();
        if j <= cap {
            let dst = &mut self.buffer[i..j];
            let src = s.as_bytes();
            dst.copy_from_slice(src);
            self.written = j;
            Ok(())
        } else {
            Err(fmt::Error{})
        }
    }
}
