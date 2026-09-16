use std::{
    fmt::{self, Display},
    mem::transmute,
};

use crate::value::Value;

pub struct Chunk {
    pub code:       Vec<Code>,
    pub constants:  Vec<Value>,

    /// Track how many stack slots are needed at most to accomodate the
    /// entire function call.
    pub stack_used: u32,
}

impl Chunk {
    pub fn new() -> Self {
        Self {code: Vec::new(), constants: Vec::new(), stack_used: 0}
    }

    /// Appends the given instruction, returning its index. Said index can be
    /// used by parser expressions or runtime bytecode instructions, and thus
    /// must fit in 25 bits.
    pub fn add_code(&mut self, i: Code) -> i32 {
        self.code.push(i);
        self.code.len() as i32 - 1
    }

    #[allow(unused)]
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
}

/// https://stackoverflow.com/questions/32710187/how-do-i-get-an-enum-as-a-string
macro_rules! enum_str {
    (enum $name:ident {
        $($variant:ident),+, // Can add `= $val:expr` if you need specific values
    }) => {
        #[repr(u8)]
        #[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
        #[allow(unused)]
        pub enum $name {
            $($variant),+
        }

        impl $name {
            pub fn to_str(&self) -> &'static str {
                match self {
                    $($name::$variant => stringify!($variant)),+
                }
            }
        }
    };
}

enum_str! {
    enum Op {
        Move,      // R(A) := R(B)
        True,      // R(A) := true  ; ip++
        False,     // R(A) := false
        FalseSkip, // R(A) := false ; ip++
        LoadInt,   // R(A) := sBx as i64
        LoadReal,  // R(A) := sBx as f64
        LoadK,     // R(A) := K(Bx)

        // R(A) := R(B) op R(C)
        Neg,  Add,  Sub,  Mul,  Div,  Mod,
        FNeg, FAdd, FSub, FMul, FDiv, FMod,
        Eq,   Lt,   Leq,
        FEq,  FLt,  FLeq,

        Return0,
        Return,
    }
}

impl Display for Op {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.to_str())
    }
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
pub struct Code(u32);

pub struct CodeInfo {
    width:  u8,
    offset: u8,
    min:    i32,
    max:    u32,
}

// https://doc.rust-lang.org/book/ch20-05-macros.html#declarative-macros-for-general-metaprogramming
macro_rules! mask_in {
    // Base case is a simple identifier.
    ($arg:ident) => {
        ($arg as u32) << CodeInfo::$arg.offset
    };

    // Variadic case is 2 or more identifiers in a comma-separated list.
    ($head:ident, $($tail:ident),+) => {
        mask_in!($head) | mask_in!($($tail),+)
    };
}

macro_rules! get_bitf {
    ($i:expr, $arg:ident) => {
        ($i >> CodeInfo::$arg.offset) & CodeInfo::$arg.max
    };
}

#[allow(nonstandard_style, unused)]
impl Code {
    pub fn make_ABC(Op: Op, A: u16, B: u16, C: u16) -> Self {
        Self(mask_in!(Op, A, B, C))
    }

    pub fn make_vABC(Op: Op, A: u16, B: u16, vC: u16, k: bool) -> Self {
        Self(mask_in!(Op, A, B, k, vC))
    }

    pub fn make_ABx(Op: Op, A: u16, Bx: u32) -> Self {
        Self(mask_in!(Op, A, Bx))
    }

    pub fn make_AsBx(Op: Op, A: u16, sBx: i32) -> Self {
        // sBx is represented in terms of unsigned Bx with excess K = (max(Bx) / 2).
        let Bx = (sBx + (CodeInfo::sBx.max as i32)) as u32;

        // println!("sBx = {sBx: >6} ({sBx:017b})");
        // println!(" Bx = {Bx: >6} ({Bx:017b})");
        Self::make_ABx(Op, A, Bx)
    }

    // I know what I'm doing
    pub fn Op (self) -> Op   { unsafe { transmute(get_bitf!(self.0, Op) as u8) } }
    pub fn A  (self) -> u8   { get_bitf!(self.0, A) as u8  }
    pub fn B  (self) -> u8   { get_bitf!(self.0, B) as u8  }
    pub fn C  (self) -> u8   { get_bitf!(self.0, C) as u8  }
    pub fn Bx (self) -> u32  { get_bitf!(self.0, Bx)       }
    pub fn sBx(self) -> i32  { (self.Bx() as i32) + CodeInfo::sBx.min }
    pub fn k  (self) -> bool { get_bitf!(self.0, k)  != 0  }
    pub fn vC (self) -> u8   { get_bitf!(self.0, vC) as u8 }
}

#[allow(nonstandard_style)]
impl CodeInfo {
    // ABC
    const Op: Self = Self::new(7, 0);
    const A:  Self = Self::Op.next(8);
    const B:  Self = Self::A.next(8);
    const C:  Self = Self::B.next(9);

    /// ABx: unsigned Bx
    const Bx: Self = Self::A.next(Self::B.width + Self::C.width);

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

    /// Creates the info for the next field, of the given width, with an offset
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

/// Not my proudest function, but it works
fn count_digits(n: usize, radix: usize) -> usize {
    let mut power = radix;
    let mut count = 1;

    while n >= power {
        count += 1;
        if let Some(new_power) = power.checked_mul(radix) {
            power = new_power;
        } else {
            break;
        }
    }
    count
}

impl Chunk {
    pub fn disassemble_all(&self) {
        println!(".constants:");
        let pad = count_digits(self.constants.len(), 10);
        for (i, v) in self.constants.iter().enumerate() {
            let v = *v;
            println!("[{i:0>0$}] {v:?}", pad);
        }

        println!(".code:");
        let pad = self.get_pad();
        for (i, pc) in self.code.iter().enumerate() {
            self.disassemble_at(i, *pc, pad);
        }
    }

    pub fn get_pad(&self) -> usize {
        count_digits(self.code.len(), 10)
    }

    pub fn disassemble_at(&self, i: usize, pc: Code, pad: usize) {
        print!("[{i:0>0$}] ", pad);

        let op = pc.Op();
        // After ':' is: fill align width
        print!("{: <8} {: >3} ", op.to_str(), pc.A());
        match op {
            Op::LoadInt | Op::LoadReal => println!("{: >7}", pc.sBx()),
            Op::LoadK => println!("{: >7}", pc.Bx()),
            _ => println!("{: >3} {: >3}", pc.B(), pc.C()),
        }
    }
}
