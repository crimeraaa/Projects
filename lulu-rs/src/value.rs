#[repr(u8)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Value {
    #[default]
    Nil,
    Bool(bool),
    Int (i64),
    Real(f64),
}

pub struct Stack<'a> {
    values: &'a mut [Value],
}

impl<'a> Stack<'a> {
    pub fn new(values: &'a mut [Value]) -> Self {
        Self {values}
    }

    pub fn store     (&mut self, a: u8, v: Value) { self.values[a as usize] = v;  }
    pub fn store_reg (&mut self, a: u8, b: u8)    { self.store(a, self.values[b as usize]); }
    pub fn store_bool(&mut self, a: u8, b: bool)  { self.store(a, Value::Bool(b)); }
    pub fn store_int (&mut self, a: u8, i: i64)   { self.store(a, Value::Int (i)); }
    pub fn store_real(&mut self, a: u8, r: f64)   { self.store(a, Value::Real(r)); }

    pub fn load(&mut self, a: u8) -> &mut Value {
        &mut self.values[a as usize]
    }

    /// We don't know the lifetimes of the unchecked registers, so we need
    /// to explicitly annotate them.
    ///
    /// See: https://stackoverflow.com/questions/30073684/how-to-get-mutable-references-to-two-array-elements-at-the-same-time
    pub fn load2(&mut self, a: u8, b: u8) -> (&'a mut Value, &'a mut Value) {
        unsafe {
            let ra = &mut *(self.values.get_unchecked_mut(a as usize) as *mut _);
            let rb = &mut *(self.values.get_unchecked_mut(b as usize) as *mut _);
            (ra, rb)
        }
    }
}

