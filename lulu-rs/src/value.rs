use std::{
    fmt::{self, Debug, Display},
};

#[repr(u8)]
#[derive(Clone, Copy, Default, PartialEq)]
pub enum Value {
    #[default]
    Nil,
    Bool(bool),
    Int (i64),
    Real(f64),
    IString(IString),
}

impl Value {
    pub fn type_name(&self) -> &'static str {
        use Value::*;
        match self {
            Nil        => "Nil",
            Bool(_)    => "Bool",
            Int(_)     => "Int",
            Real(_)    => "Real",
            IString(_) => "IString",
        }
    }
}

impl Debug for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}({})", self.type_name(), *self)
    }
}

impl Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use Value::*;
        match self {
            Nil        => write!(f, "nil"),
            Bool(b)    => write!(f, "{b}"),
            Int(i)     => write!(f, "{i}"),
            Real(r)    => write!(f, "{r:.14e}"),
            IString(s) => write!(f, "{s}"),
        }
    }
}

/// Inline or immediate string. Since we know that the tagged value needs
/// to take up 2 machine words (`u8` tag, padding, `i64|f64|* T` payload),
/// then we can store a Pascal-style string in the space unoccupied by the
/// tag, which is 15 bytes.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct IString {
    len: u8,
    chars: [u8; 14],
}

impl IString {
    pub fn new(s: &str) -> Self {
        let n = s.len();
        assert!(n <= 14, "string length {n} is too long");

        let len = n as u8;
        let mut chars = [0 as u8; 14];
        for (i, c) in s.as_bytes().iter().enumerate() {
            chars[i] = *c;
        }
        Self {len, chars}
    }
}

impl Display for IString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = self.chars.as_slice();
        match str::from_utf8(s) {
            Ok(s)  => write!(f, "\"{}\"", s),
            Err(_) => panic!("Invalid UTF-8 sequence in {s:?}"),
        }
    }
}

pub struct Stack<'a> {
    /// Our register stack is a borrowed, mutable slice of tagged values.
    r: &'a mut [Value],
}

impl<'a> Stack<'a> {
    pub fn new(r: &'a mut [Value]) -> Self {
        Self {r}
    }

    pub fn as_slice(&self) -> &[Value] {
        self.r
    }

    /// Store the given tagged value in the given register.
    pub fn store     (&mut self, a: u8, v: Value) { self.r[a as usize] = v;  }
    pub fn store_reg (&mut self, a: u8, b: u8)    { self.store(a, self.r[b as usize]); }
    pub fn store_bool(&mut self, a: u8, b: bool)  { self.store(a, Value::Bool(b)); }
    pub fn store_int (&mut self, a: u8, i: i64)   { self.store(a, Value::Int (i)); }
    pub fn store_real(&mut self, a: u8, r: f64)   { self.store(a, Value::Real(r)); }

    /// Reads the given register, returning the tagged value.
    pub fn load(&mut self, a: u8) -> &mut Value {
        &mut self.r[a as usize]
    }

    /// We don't know the lifetimes of the unchecked registers, so we need
    /// to explicitly annotate them.
    ///
    /// See: https://stackoverflow.com/questions/30073684/how-to-get-mutable-references-to-two-array-elements-at-the-same-time
    pub fn load2(&mut self, a: u8, b: u8) -> (&'a mut Value, &'a mut Value) {
        unsafe {
            let ra = &mut *(self.r.get_unchecked_mut(a as usize) as *mut _);
            let rb = &mut *(self.r.get_unchecked_mut(b as usize) as *mut _);
            (ra, rb)
        }
    }
}


