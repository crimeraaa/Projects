use std::mem::transmute;
use crate::value::Value;

pub struct Chunk {
    pub code:      Vec<Instruction>,
    pub constants: Vec<Value>,
}

impl Chunk {
    pub fn new() -> Self {
        Self {code: Vec::new(), constants: Vec::new()}
    }

    pub fn add_instruction(&mut self, i: Instruction) -> i32 {
        self.code.push(i);
        self.code.len() as i32 - 1
    }

    pub fn add_constant(&mut self, v: Value) -> u32 {
        match self.constants
            .iter()
            .enumerate()
            .find(|(_, ppv)| **ppv == v)
        {
            None => {
                self.constants.push(v);
                self.constants.len() as u32 - 1
            },
            Some(p) => {
                let (i, _) = p;
                i as u32
            },
        }
    }

    pub fn disassemble_all(&self) {
        for (i, v) in self.constants.iter().enumerate() {
            println!("K({}) = {:?}", i, *v);
        }

        for (i, pc) in self.code.iter().enumerate() {
            self.disassemble_at(i, *pc);
        }
    }

    pub fn disassemble_at(&self, i: usize, pc: Instruction) {
        print!("[{i}] ");

        let op = pc.Op();
        println!("{:-16?} {:-3} ", op, pc.A());

        use OpCode::*;
        match op {
            IntI   | IntK |
            FloatI | FloatK => {
                println!("{:-7}", pc.Bx())
            }
            _ => println!("{:-3} {:-3}", pc.B(), pc.C()),
        }
    }
}

#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
pub enum OpCode {
    Move,   // R(A) := R(B)
    IntI,   // R(A) := sBx as i64
    IntK,   // R(A) := K(Bx).i64
    FloatI, // R(A) := sBx as f64
    FloatK, // R(A) := K(Bx).f64

    /// R(A) := R(B) op R(C)
    Neg,  Add,  Sub,  Mul,  Div,  Mod,
    FNeg, FAdd, FSub, FMul, FDiv, FMod,
    Eq,   Lt,   Leq,
    FEq,  FLt,  FLeq,

    Return0,
    Return,
}

/// A bitfield. The format, in big endian format, is as follows:
///
/// tens | 3...2.................1...................0.....................|
/// ones | 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 |
/// ABC  |        C(9)      |     B(8)      |     A(8)      |    Op(7)     |
/// ABx  |         unsigned Bx(17)          |     A(8)      |    Op(7)     |
/// AsBx |         signed  sBx(17)          |     A(8)      |    Op(7)     |
/// vABC |        C(8)    |k|     B(8)      |     A(8)      |    Op(7)     |
///
#[derive(Clone, Copy)]
pub struct Instruction(u32);

pub struct InstructionInfo {
    width:  u8,
    offset: u8,
    min:    i32,
    max:    u32,
}

// https://doc.rust-lang.org/book/ch20-05-macros.html#declarative-macros-for-general-metaprogramming
macro_rules! mask_in {
    // Base case is a simple identifier.
    ( $arg: ident) => {
        ($arg as u32) << InstructionInfo::$arg.offset & InstructionInfo::$arg.max
    };

    // Variadic case is 2 or more identifiers in a comma-separated list.
    ( $arg1: ident, $($argn: ident), +) => {
        mask_in!($arg1) | mask_in!($($argn), +)
    };
}

macro_rules! get_bitf {
    ( $i: expr, $arg: ident ) => {
        $i >> InstructionInfo::$arg.offset | InstructionInfo::$arg.max
    };
}

#[allow(nonstandard_style)]
impl Instruction {
    pub fn make_ABC(Op: OpCode, A: u16, B: u16, C: u16) -> Self {
        Self(mask_in!(Op, A, B, C))
    }

    pub fn make_vABC(Op: OpCode, A: u16, B: u16, vC: u16, k: bool) -> Self {
        Self(mask_in!(Op, A, B, k, vC))
    }

    pub fn make_ABx(Op: OpCode, A: u16, Bx: u32) -> Self {
        Self(mask_in!(Op, A, Bx))
    }

    pub fn make_AsBx(Op: OpCode, A: u16, sBx: i32) -> Self {
        // sBx is represented in terms of unsigned Bx with excess K = (max(Bx) / 2).
        let Bx = (sBx + InstructionInfo::sBx.max as i32) as u32;
        Self::make_ABx(Op, A, Bx)
    }

    // I know what I'm doing
    pub fn Op (self) -> OpCode { unsafe { transmute(get_bitf!(self.0, Op) as u8) } }
    pub fn A  (self) -> u8     { get_bitf!(self.0, A) as u8  }
    pub fn B  (self) -> u8     { get_bitf!(self.0, B) as u8  }
    pub fn C  (self) -> u8     { get_bitf!(self.0, C) as u8  }
    pub fn Bx (self) -> u32    { get_bitf!(self.0, Bx)       }
    pub fn sBx(self) -> i32    { (self.Bx() as i32) + InstructionInfo::sBx.min }
    pub fn k  (self) -> bool   { get_bitf!(self.0, k)  != 0  }
    pub fn vC (self) -> u8     { get_bitf!(self.0, vC) as u8 }
}

#[allow(nonstandard_style)]
impl InstructionInfo {
    // ABC
    const Op: Self = Self::new(7, 0);
    const A:  Self = Self::Op.next(8);
    const B:  Self = Self::A.next(8);
    const C:  Self = Self::B.next(9);

    /// ABx: unsigned Bx
    const Bx: Self = Self::B.next(Self::B.width + Self::C.width);

    /// AsBx: signed Bx
    const sBx: Self = Self::Bx.signed();
    
    /// vABC: vC and k
    const k:  Self = Self::B.next(1);
    const vC: Self = Self::k.next(8);

    /// Creates info for a particular field, calculating other parts
    /// in terms of the given arguments.
    const fn new(width: u8, offset: u8) -> Self {
        Self {width, offset, min: 0i32, max: (1u32 << (width as u32)) - 1u32}
    }

    /// Creates the info for the next field, of the given size, with an offset
    /// calculated by the info of the current field.
    const fn next(self, width: u8) -> Self {
        Self::new(width, self.width + self.offset)
    }

    /// Creates a signed version of the given field, where the minimum and
    /// maximum values reflect the intended actual argument range.
    const fn signed(self) -> Self {
        let max = self.max / 2;
        let min = -(max as i32);
        Self {min, max, ..self}
    }
}

