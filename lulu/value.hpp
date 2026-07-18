#pragma once

#include "lulu.h"
#include "internal.hpp"

#define VALUE_KINDS(X)      \
    X(Value_nil,    "nil")  \
    X(Value_bool,   "bool") \
    X(Value_int,    "int")  \
    X(Value_real,   "real") \
    X(Value_string, "string")

enum ValueKind : u8 {
#define X(e, s) e,
    VALUE_KINDS(X)
#undef X
};

// Stupid template shenanigans because we can't token paste
template<class T>
struct trait_ValueKind {
    static_assert(false, "Unsupported type to get ValueKind of");
    static ValueKind constexpr kind = Value_nil;
};

template<> struct trait_ValueKind<bool>      { static ValueKind constexpr kind = Value_bool; };
template<> struct trait_ValueKind<lulu_int>  { static ValueKind constexpr kind = Value_int;  };
template<> struct trait_ValueKind<lulu_real> { static ValueKind constexpr kind = Value_real; };

union Value {
    lulu_int  integer;
    lulu_real floating;
    bool      boolean;
};

static inline bool      value_bool(Value v) { return v.boolean;  }
static inline lulu_int  value_int (Value v) { return v.integer;  }
static inline lulu_real value_real(Value v) { return v.floating; }

static inline void value_set_bool(Value *v, bool      arg) { v->boolean  = arg; }
static inline void value_set_int (Value *v, lulu_int  arg) { v->integer  = arg; }
static inline void value_set_real(Value *v, lulu_real arg) { v->floating = arg; }

// Only necesary for other template shenanigans. Otherwise, use the named versions.
template<class T> inline T
value_get(Value v);

template<> inline bool      value_get(Value v)  { return value_bool(v); }
template<> inline lulu_int  value_get(Value v)  { return value_int (v); }
template<> inline lulu_real value_get(Value v)  { return value_real(v); }

// Only necesary for other template shenanigans. Otherwise, use the named versions.
template<class T>
inline void
value_set(Value *v, T arg);

template<> inline void value_set(Value *v, bool      arg) { value_set_bool(v, arg); }
template<> inline void value_set(Value *v, lulu_int  arg) { value_set_int (v, arg); }
template<> inline void value_set(Value *v, lulu_real arg) { value_set_real(v, arg); }

// Tagged value.
struct TValue {
    ValueKind kind;
    Value     value;
};

static inline bool tvalue_is_kind(TValue tv, ValueKind kind) { return tv.kind == kind; }

#define tvalue_is_nil(tv)   tvalue_is_kind(tv, Value_nil)
#define tvalue_is_bool(tv)  tvalue_is_kind(tv, Value_bool)
#define tvalue_is_int(tv)   tvalue_is_kind(tv, Value_int)
#define tvalue_is_real(tv)  tvalue_is_kind(tv, Value_real)

// If assertions are disabled then they will expand to a no-op.
#define tvalue_get(tv, T)   (LULU_ASSERT(tvalue_is_##T(tv)), value_##T((tv).value))
#define tvalue_bool(tv)     tvalue_get(tv, bool)
#define tvalue_int(tv)      tvalue_get(tv, int)
#define tvalue_real(tv)     tvalue_get(tv, real)

// This is just for template shenanigans. Don't use it directly- prefer using
// the named variants.
template<class T>
inline TValue
tvalue_make(T arg)
{
    TValue tv = {trait_ValueKind<T>::kind, {0}};
    value_set<T>(&tv.value, arg);
    return tv;
}

static inline TValue tvalue_make_nil (void)        { return {Value_nil, {0}}; }
static inline TValue tvalue_make_bool(bool b)      { return tvalue_make(b); }
static inline TValue tvalue_make_int (lulu_int  i) { return tvalue_make(i); }
static inline TValue tvalue_make_real(lulu_real r) { return tvalue_make(r); }

LULU_INTERNAL_FUNC bool
tvalue_eq(TValue a, TValue b);

