#pragma once

#include "internal.hpp"

/*
 Description:
    Defines the `Result` interface using CRTP. The concrete type must provide
    implementations of the `into_ok` and `into_err` methods so that the rest
    of the interface can be implicitly defined in terms of them.

 Template parameters:
    Self - A concrete instantiation of Result.
    T    - The value type of the Result.
    E    - The error type of the Result.
 */
template<class Self, class T, class E>
struct trait_Result {
private:
    // CRTP BS
    Self const *to_self() const { return cast(Self const *)this; }

    // Requires concrete implementations.
    T Ok()  const { return to_self()->into_ok();  }
    E Err() const { return to_self()->into_err(); }

public:
    T    unwrap() const { LULU_ASSERT(is_ok()); return Ok(); };
    bool is_ok () const { return !is_err(); };
    bool is_err() const { return Err();     };

    template<class F> T
    unwrap_or_else_err(F f) const
    {
        auto p = to_self();
        if (is_ok()) {
            return Ok();
        }
        f(Err());
        return {};
    }

    T
    expect(char const *message) const
    {
        LULU_ASSERTF(is_ok(), "%s", message);
        return Ok();
    }
};

template<class T, class E>
struct Result : public trait_Result<Result<T, E>, T, E> {
private:
    E m_error;
    T m_value;

public:
    Result(E error) : m_error{error}, m_value{}      {}
    Result(T value) : m_error{},      m_value{value} {}
    
    E into_err() const { return m_error; }
    T into_ok () const { return m_value; }
};

/*
 Description:
    Similar to `trait_Result`, this defines the interface for `Option`,
    which simply adds new methods on top of the existing ones for Result.
    The new methods are also defined in terms of the concrete `into_err`
    and `into_ok` implementations.
 */
template<class Self, class T>
struct trait_Option : public trait_Result<Self, T, /*E=*/bool>
{
    bool is_none() const { return !(cast(Self *)this)->into_err(); }
    bool is_some() const { return !is_none(); }
};

template<class T>
struct Option : public trait_Option<Option<T>, T> {
private:
    bool m_present;
    T    m_value;

public:
    Option()        : m_present{false}, m_value{}      {}
    Option(T value) : m_present{true},  m_value{value} {}

    bool into_err() const { return !m_present; }
    T    into_ok () const { return  m_value;   }
};

// Specialization for pointers since we can assume there is at least 1 unused
// bit in the least significant portion. I.e. if addresses are always aligned
// to 8, then the lower 3 bits are always unused.
template<class T>
struct Option<T *> : public trait_Option<Option<T *>, T *> {
private:
    // Get or set the 'ok' bit. The complement can also be used to filter out
    // the 'ok' bit unconditionally to retrieve the actual pointer.
    static constexpr uintptr BIT_OK = 0x1;

    uintptr m_value;

public:
    Option()           : m_value{} {}
    Option(T *pointer) : m_value{cast(uintptr)pointer | BIT_OK} {}

    bool into_err() const { return !(m_value & BIT_OK); }
    T *  into_ok () const { return cast(T *)(m_value & ~BIT_OK); }
};

