use std::{
    io::{self, Write},
};

mod lex;
mod value;
mod chunk;

use chunk::{
    OpCode,
    Instruction,
    Chunk
};

use value::Value;

fn main() {
    let mut c = Chunk::new();

    // expr: 1 + 2*3 - 4/-5
    c.add_instruction(Instruction::make_AsBx(OpCode::IntI, 0, 1));
    c.add_instruction(Instruction::make_AsBx(OpCode::IntI, 1, 2));
    c.add_instruction(Instruction::make_AsBx(OpCode::IntI, 2, 3));
    c.add_instruction(Instruction::make_ABC (OpCode::Mul,  1, 2, 1));
    c.add_instruction(Instruction::make_ABC (OpCode::Add,  0, 0, 1));

    c.add_instruction(Instruction::make_AsBx(OpCode::IntI, 1, 4));
    c.add_instruction(Instruction::make_AsBx(OpCode::IntI, 2, 5));
    c.add_instruction(Instruction::make_ABC (OpCode::Neg,  2, 2, 0));
    c.add_instruction(Instruction::make_ABC (OpCode::Div,  1, 1, 2));
    c.add_instruction(Instruction::make_ABC (OpCode::Sub,  0, 0, 1));
    c.disassemble_all();
    execute(&c);
}

fn execute(c: &Chunk) {
    let mut r = [Value::Nil; 255];
    type Op = OpCode;
    let k = c.constants.as_slice();
    for pc in c.code.iter() {
        let a = pc.A() as usize;
        match pc.Op() {
            Op::Move   => r[a] = r[pc.B() as usize],
            Op::IntI   => r[a] = Value::Int(pc.sBx() as i64),
            Op::IntK   => r[a] = k[pc.Bx() as usize],
            Op::FloatI => r[a] = Value::Float(pc.sBx() as f64),
            Op::FloatK => r[a] = k[pc.Bx() as usize],
            _ => todo!(),
        }
    }
}

#[allow(unused)]
fn repl() {
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

#[allow(unused)]
fn tokenize(name: &str, input: &str) {
    let mut x = lex::Lexer::new(input);
    println!("Parsing \"{name}\"...");
    loop {
        let t = x.scan_token();
        match t.kind {
            lex::TokenKind::Eof => break,
            lex::TokenKind::Invalid(e) => {
                println!("{}:{} ({})", name, t, e);
                break;
            }
            _ => println!("{}:{}", name, t),
        }
    }
}
