#ifndef LULU_VALUE_H
#define LULU_VALUE_H

#include "internal.h"

typedef enum ValueKind {
    Value_none = -1,
    Value_nil,
    Value_bool,
    Value_uint,
    Value_int,
    Value_real,
    Value_string,
} ValueKind;

typedef union Value Value;
union Value {
    lulu_uint u;
    lulu_int  i;
    lulu_real f;
    lulu_bool b;
};

#define value_bool(v)   (v).b
#define value_uint(v)   (v).u
#define value_int(v)    (v).i
#define value_real(v)   (v).f

#define value_set_bool(v, bv)   (value_bool(*v) = (bv))
#define value_set_uint(v, uv)   (value_uint(*v) = (uv))
#define value_set_int( v, iv)   (value_int (*v) = (iv))
#define value_set_real(v, rv)   (value_real(*v) = (rv))

typedef struct TValue TValue;
struct TValue {
    ValueKind kind;
    Value     value;
};

static inline ValueKind
tvalue_kind(TValue tv)
{
    return tv.kind;
}

static inline bool
tvalue_is_kind(TValue tv, ValueKind kind)
{
    return tvalue_kind(tv) == kind;
}

#define tvalue_is_none(tv)      tvalue_is_kind(tv, Value_none)
#define tvalue_is_nil(tv)       tvalue_is_kind(tv, Value_nil)
#define tvalue_is_bool(tv)      tvalue_is_kind(tv, Value_bool)
#define tvalue_is_uint(tv)      tvalue_is_kind(tv, Value_uint)
#define tvalue_is_int(tv)       tvalue_is_kind(tv, Value_int)
#define tvalue_is_real(tv)      tvalue_is_kind(tv, Value_real)

// If assertions are disabled then they will expand to a no-op.
#define tvalue_get(tv, is, get) (LULU_ASSERT(is(tv)), get((tv).value))
#define tvalue_bool(tv)         tvalue_get(tv, tvalue_is_bool, value_bool)
#define tvalue_uint(tv)         tvalue_get(tv, tvalue_is_uint, value_uint)
#define tvalue_int(tv)          tvalue_get(tv, tvalue_is_int,  value_int)
#define tvalue_real(tv)         tvalue_get(tv, tvalue_is_real, value_real)

static inline TValue
tvalue_make_nil(void)
{
    TValue tv = {Value_nil, {0}};
    return tv;
}

static inline TValue
tvalue_make_bool(lulu_bool b)
{
    TValue tv = {Value_bool, {0}};
    value_set_bool(&tv.value, b); 
    return tv;
}

static inline TValue
tvalue_make_uint(lulu_uint u)
{
    TValue tv = {Value_uint, {0}};
    value_set_uint(&tv.value, u);
    return tv;
}

static inline TValue
tvalue_make_int(lulu_int i)
{
    TValue tv = {Value_int, {0}};
    value_set_int(&tv.value, i);
    return tv;
}

static inline TValue
tvalue_make_real(lulu_real r)
{
    TValue tv = {Value_real, {0}};
    value_set_real(&tv.value, r);
    return tv;
}

LULU_INTERNAL_FUNC bool
tvalue_eq(TValue a, TValue b);

#endif // !LULU_VALUE_H
