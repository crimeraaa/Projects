use std::{
    io::{self, Write}, ops::{Neg, Add, Div, Mul, Rem, Sub},
};

mod lex;
mod value;
mod code;

use code::{
    Op,
    Code,
    Chunk
};

use value::{Value, Stack};

fn main() {
    let mut c = Chunk::new();

    // expr: 1 + 2*3 - 4/-5
    c.add_code(Code::make_AsBx(Op::LoadInt, 0, 1));
    c.add_code(Code::make_AsBx(Op::LoadInt, 1, 2));
    c.add_code(Code::make_AsBx(Op::LoadInt, 2, 3));
    c.add_code(Code::make_ABC (Op::Mul,     1, 2, 1));
    c.add_code(Code::make_ABC (Op::Add,     0, 0, 1));

    c.add_code(Code::make_AsBx(Op::LoadInt, 1, 4));
    c.add_code(Code::make_AsBx(Op::LoadInt, 2, 5));
    c.add_code(Code::make_ABC (Op::Neg,     2, 2, 0));
    c.add_code(Code::make_ABC (Op::Div,     1, 1, 2));
    c.add_code(Code::make_ABC (Op::Sub,     0, 0, 1));
    c.add_code(Code::make_ABC (Op::Return,  0, 1, 0));

    // must always be added
    c.add_code(Code::make_ABC (Op::Return0, 0, 0, 0));
    c.disassemble_all();
    execute(&c);
}

macro_rules! arith_int {
    ($r:ident, $op:ident, $a:expr, $b:expr) => ({
        let args = $r.load2($a, $b);
        match args {
            (Value::Int(lhs), Value::Int(rhs)) => {
                let res = lhs.$op(*rhs);
                $r.store_int($a, res);
            }
            _ => {
                panic!("Can't call {} on non-integers {args:?}", stringify!($op));
            }
        }
    });
}

macro_rules! arith_real {
    ($r:ident, $op:ident, $a:expr, $b:expr) => ({
        let args = $r.load2($a, $b);
        match args {
            (Value::Real(lhs), Value::Real(rhs)) => {
                let res = lhs.$op(*rhs);
                $r.store_real($a, res);
            }
            _ => {
                panic!("Can't call {} on non-reals {args:?}", stringify!($op));
            }
        }
    });
}

fn execute(c: &Chunk) {
    let mut tmp = [Value::Nil; 255];
    let mut r = Stack::new(&mut tmp);
    let k = c.constants.as_slice();

    let mut ip = c.code.iter();
    'exec: loop {
        // We assume that there is always at least 1 instruction, and that
        // once we hit some sort of return the execution terminates.
        let pc = *ip.next().unwrap();

        // We assume that this stack slot is always safe to read.
        let a  = pc.A();
        let op = pc.Op();
        match op {
            // Stores
            Op::Move => r.store_reg(a, pc.B()),
            Op::True => {
                r.store_bool(a, true);
                ip.next();
            }
            Op::False => r.store_bool(a, false),
            Op::FalseSkip => {
                r.store_bool(a, false);
                ip.next();
            }
            Op::LoadInt  => r.store_int (a,   pc.sBx() as i64),
            Op::LoadReal => r.store_real(a,   pc.sBx() as f64),
            Op::LoadK    => r.store     (a, k[pc.Bx () as usize]),

            // Integer arithmetic
            Op::Neg => {
                let arg = r.load(pc.B());
                match arg {
                    Value::Int(i) => {
                        let i = i.neg();
                        r.store_int(a, i);
                    }
                    _ => panic!("Can't negate non-integer {:?}", arg),
                }
            }
            Op::Add => arith_int!(r, wrapping_add, a, pc.B()),
            Op::Sub => arith_int!(r, wrapping_sub, a, pc.B()),
            Op::Mul => arith_int!(r, wrapping_mul, a, pc.B()),
            Op::Div => arith_int!(r, wrapping_div, a, pc.B()),
            Op::Mod => arith_int!(r, rem,          a, pc.B()),

            // Floating-point arithmetic
            Op::FNeg => {
                let arg = r.load(pc.B());
                match arg {
                    Value::Real(i) => {
                        let f = i.neg();
                        r.store_real(a, f);
                    }
                    _ => panic!("Can't negate non-real {:?}", arg),
                }
            }
            Op::FAdd => arith_real!(r, add, a, pc.B()),
            Op::FSub => arith_real!(r, sub, a, pc.B()),
            Op::FMul => arith_real!(r, mul, a, pc.B()),
            Op::FDiv => arith_real!(r, div, a, pc.B()),
            Op::FMod => arith_real!(r, rem, a, pc.B()),
            Op::Return0  => break 'exec,
            Op::Return   => break 'exec,
            _ => todo!("Can't execute {op:?} yet"),
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
