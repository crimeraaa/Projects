#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Value {
    Nil,
    Bool(bool),
    Int (i64),
    Float(f64),
}
