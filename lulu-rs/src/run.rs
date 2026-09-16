use std::{
    // Needed so we can use the named arithmetic functions.
    ops::{Neg, Add, Div, Mul, Rem, Sub},
};

use crate::{
    code::{Op, Chunk},
    value::{Value, Stack},
};

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

pub fn execute(c: &Chunk) {
    let mut tmp = [Value::Nil; 255];

    // Borrow exactly as many values as need, no more and no less.
    let mut r = Stack::new(&mut tmp[0..(c.stack_used as usize)]);
    let k = c.constants.as_slice();

    let mut ip = c.code.iter();

    // The `while let` idiom lets us mutate the iterator within the loop,
    // as a normal `for` loop would otherwise take ownership of it.
    while let Some(pc) = ip.next() {
        let pc = *pc;

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
            Op::Return0 | Op::Return => {
                dump_stack(&r, a, pc.B());
                break;
            }
            _ => todo!("Can't execute {op:?} yet"),
        }
    }
}

fn dump_stack(r: &Stack, a: u8, b: u8) {
    let r = r.as_slice();
    let n = match (a, b) {
        // Return0 or Variadic?
        (0, 0) => r.len(),
        _      => (b - a) as usize,
    };

    println!("(stack dump, length {})", n);
    for (reg, val) in r
        .iter()
        .take(n)
        .enumerate()
    {
        let val = *val;
        println!("[{reg}] {val:?}");
    }
}

