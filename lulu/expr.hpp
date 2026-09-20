#pragma once

#include "internal.hpp"
#include "lexer.hpp"
#include "type.hpp"
#include "list.hpp"
#include "value.hpp"

enum ExprKind : u8 {
    Expr_None,
    Expr_nil,
    Expr_Literal,
    Expr_Type,       // Type name. No data.
    Expr_Constant,   // Constant index is in `constant`.
    Expr_Local,      // Variable index is in `reg`.
    Expr_Call,       // Call instruction index in `pc`.
    Expr_Compare,    // Comparison instruction index in `pc`.
    Expr_Pending,    // Instruction index in `pc`- need destination register.
    Expr_Discharged, // Set to register `reg`.
};

struct Expr {
    ExprKind    kind         = Expr_None;
    ValueKind   literal_kind = Value_nil; // Helps reduce pointer dereferencing.
    Loc         loc;
    Type const *type         = nullptr;
    union {
        Value literal = {0};
        u32   constant; // Index of value in chunk constants array.
        i32   pc;       // Index of instruction in chunk bytecode array.
        u16   reg;      // Index of stack slot and/or active variable info.
    };

    // Expr::make* ========================================================= {{{

    static Expr
    make(ExprKind kind, Type const *type, Token const &token)
    {
        Expr e;
        e.kind = kind;
        e.type = type;
        e.loc  = token.loc;
        return e;
    }

    static Expr
    make_nil(Token const &token)
    {
        return make(Expr_nil, basic_type_get(Value_nil), token);
    }

    static Expr make_bool(Token const &t, bool b) { return make_literal(t, b); }
    static Expr make_int (Token const &t, intr i) { return make_literal(t, i); }
    static Expr make_real(Token const &t, real r) { return make_literal(t, r); }

    // Literals are typed but can be coerced as long as data loss does not occur.
    // Coercion only occurs for numeric types.
    template<class T>
    static inline Expr
    make_literal(Token const &token, T arg)
    {
        auto constexpr    kind = trait_ValueKind<T>;
        Type const *const type = basic_type_get(kind);
        Expr              expr = make(Expr_Literal, type, token);

        // Literal value stuff
        expr.literal_kind = kind;
        expr.literal.set(arg);
        return expr;
    }

    // Expressions that represent type names (and nothing else!) should be
    // treated VERY differently from identifiers.
    static Expr
    make_type(Token const &token, Type const *type)
    {
        return make(Expr_Type, type, token);
    }

    static Expr
    make_local(Token const &token, Type const *type, u16 reg)
    {
        Expr e = make(Expr_Local, type, token);
        e.reg = reg;
        return e;
    }

    // ===================================================================== }}}
    // Expr::*is* =========================================================== {{{

    bool is_literal() const { return this->kind == Expr_Literal;    }
    bool is_reg    () const { return this->kind == Expr_Discharged; }
    bool is_local  () const { return this->kind == Expr_Local;      }
    bool is_compare() const { return this->kind == Expr_Compare;    }
    bool is_pc     () const { return this->kind == Expr_Pending;    }

    // Helpers for various literal types.
    bool is_literal_bool() const { return this->is_literal() && this->literal_kind == Value_bool; }
    bool is_literal_intr() const { return this->is_literal() && this->literal_kind == Value_int;  }
    bool is_literal_real() const { return this->is_literal() && this->literal_kind == Value_real; }

    
    // Tests if the expression's underlying type is a numeric of some kind,
    // i.e. an integer or a real.
    bool
    type_is_numeric() const
    {
        switch (this->type->kind) {
        case TypeKind_Basic:
            switch (this->type->basic.kind) {
            case Value_int:
            case Value_real: return true;
            default:
                break;
            }
            break;
        default:
            LULU_UNIMPLEMENTED();
            break;
        }
        return false;
    }

    bool
    type_is_basic() const
    {
        return this->type != nullptr && this->type->kind == TypeKind_Basic;
    }

    bool
    type_is_basic_kind(ValueKind kind) const
    {
        return this->type_is_basic() && this->type->basic.kind == kind;
    }

    // ===================================================================== }}}
    // Expr::*get* ========================================================= {{{

    ValueKind
    get_literal_kind() const
    {
        LULU_ASSERT(this->is_literal());
        return this->literal_kind;
    }

    ValueKind
    type_get_basic_kind() const
    {
        LULU_ASSERT(this->type_is_basic());
        return this->type->basic.kind;
    }

    // Variant data getters that assert we are of the correct kind first.
    bool get_bool() const { LULU_ASSERT(this->is_literal_bool()); return this->literal.get_bool(); }
    intr get_intr() const { LULU_ASSERT(this->is_literal_intr()); return this->literal.get_intr(); }
    real get_real() const { LULU_ASSERT(this->is_literal_real()); return this->literal.get_real(); }
    u16  get_reg () const { LULU_ASSERT(this->is_reg()); return this->reg; }
    i32  get_pc  () const { LULU_ASSERT(this->is_pc());  return this->pc;  }

    // Necessary for template shenanigans.
    template<class T> T    get_literal() const;
    template<>        bool get_literal() const { return this->get_bool(); }
    template<>        intr get_literal() const { return this->get_intr(); }
    template<>        real get_literal() const { return this->get_real(); }


    // ===================================================================== }}}
    // Expr::set* ========================================================== {{{
    // More template BS. Are you having fun yet?
    template<class T>
    void
    set_literal(T arg)
    {
        this->literal_kind = trait_ValueKind<T>;
        this->type         = basic_type_get(this->literal_kind);
        this->literal.set(arg);
    }

    // Various setters. They set the kind and payload, but nothing else.
    void set_bool      (bool b)    { this->set_literal(b); }
    void set_intr      (intr i)    { this->set_literal(i); }
    void set_real      (real r)    { this->set_literal(r); }
    void set_discharged(u16 reg)   { this->kind = Expr_Discharged; this->reg      = reg;   }
    void set_pending   (i32 pc)    { this->kind = Expr_Pending;    this->pc       = pc;    }
    void set_compare   (i32 pc)    { this->kind = Expr_Compare;    this->pc       = pc;    }
    void set_constant  (u32 index) { this->kind = Expr_Constant;   this->constant = index; }

    // ===================================================================== }}}
};

#define EXPR_EXPAND(e)  STRING_EXPAND((e).token.lexeme)

using ExprList = List<Expr>;

