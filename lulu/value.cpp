#pragma once

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
struct type2vk {
    static_assert(false, "Unsupported type to get ValueKind of");
    static constexpr auto kind = Value_nil;
};

template<> struct type2vk<bool> { static constexpr auto kind = Value_bool; };
template<> struct type2vk<intr> { static constexpr auto kind = Value_int;  };
template<> struct type2vk<real> { static constexpr auto kind = Value_real; };

// Helper variable template is a C++14 thing.
template<class T>
constexpr auto trait_ValueKind = type2vk<T>::kind;

union Value {
    /* Two-fold (2-fold) job: actual (signed) integers, and booleans. This
       allows us to implement boolean operations in terms of integer ones. */
    intr  i;
    real  r;
    void *p;


    bool get_bool() const { return cast(bool)this->i;  }
    intr get_intr() const { return this->i;  }
    real get_real() const { return this->r; }

    void set_bool(bool arg) { this->i = cast(intr)arg; }
    void set_intr(intr arg) { this->i = arg; }
    void set_real(real arg) { this->r = arg; }

    // Only necesary for other template shenanigans. Otherwise, use the named versions.
    template<class T> T    get() const;
    template<>        bool get() const { return this->get_bool(); }
    template<>        intr get() const { return this->get_intr(); }
    template<>        real get() const { return this->get_real(); }

    // Only necesary for other template shenanigans. Otherwise, use the named versions.
    template<class T> void set(T    arg);
    template<>        void set(bool arg) { this->set_bool(arg); }
    template<>        void set(intr arg) { this->set_intr(arg); }
    template<>        void set(real arg) { this->set_real(arg); }
};

// Tagged value.
struct TValue {
    ValueKind kind  = Value_nil;
    Value     value = {0};

    static TValue make_bool(bool b) { return make(b);  }
    static TValue make_real(real r) { return make(r);  }
    static TValue make_intr(intr i) { return make(i);  }

    // This is just for template shenanigans. Don't use it directly- prefer using
    // the named variants.
    template<class T>
    static TValue
    make(T arg)
    {
        TValue tv = {trait_ValueKind<T>, {0}};
        tv.value.set(arg);
        return tv;
    }

    bool is_nil () const { return this->kind == Value_nil;   }
    bool is_bool() const { return this->kind == Value_bool;  }
    bool is_intr() const { return this->kind == Value_int;   }
    bool is_real() const { return this->kind == Value_real;  }

#define get(T, p)   (LULU_ASSERT(p->is_##T()), (p)->value.get_##T())
    bool get_bool() const { return get(bool, this); }
    intr get_intr() const { return get(intr, this); }
    real get_real() const { return get(real, this); }
#undef get
};

static bool
operator==(TValue a, TValue b)
{
    if (a.kind == b.kind) switch (a.kind) {
    case Value_nil:     return true;
    case Value_bool:    return a.get_bool() == b.get_bool();
    case Value_int:     return a.get_intr() == b.get_intr();
    case Value_real:    return a.get_real() == b.get_real();
    case Value_string:  LULU_UNREACHABLE(); break;
    }
    return false;
}

