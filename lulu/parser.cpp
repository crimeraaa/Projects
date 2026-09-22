#pragma once

#include <stdio.h> // [f]printf

#include "internal.hpp"
#include "slice.cpp"
#include "strings.cpp"
#include "value.cpp"
#include "chunk.cpp"
#include "lexer.cpp"
#include "compiler.cpp"
#include "expr.cpp"
#include "type.cpp"

// If you exceed this, you should probably rethink what you did!
#define PARSER_MAX_RECURSIONS   250

struct ParserData {
    String  path, input;
    Chunk   chunk;
    Scratch scratch;
};

class Parser {
    // Shared state.
    lulu_State *L;
    Compiler *  compiler;
    String      path;

    // Parser state.
    Lexer lexer;
    Token token;

    // Just in case we want to allocate a message.
    Scratch *scratch = nullptr;

    // Tracked to prevent stack overflow.
    int recursions = 0;

public:
    [[nodiscard]] static Chunk *
    parse(lulu_State *L, ParserData &data);

    [[noreturn]] void
    error_at(char const *info, Loc const &loc);

    [[noreturn]] void
    error_at(char const *info, Token const &token)
    {
        this->error_at(info, token.loc);
    }

    [[noreturn]] void
    error_at(char const *info, Expr const &expr)
    {
        this->error_at(info, expr.loc);
    }

    [[noreturn]] void
    error(char const *info)
    {
        this->error_at(info, this->token.loc);
    }

private:
    void
    simple_stmt();

    void
    ident_stmt();
    
    void
    return_stmt();

    void
    decl(ExprList lhs_list);

    void
    assign(ExprList lhs_list);

    [[nodiscard]] ExprList
    primary_expr_list(bool is_lhs);

    [[nodiscard]] ExprList
    expr_list(bool is_lhs = false);

    [[nodiscard]] Expr
    expr(bool is_lhs = false, int prec_in = 1);

    [[nodiscard]] Expr
    unary_expr(bool is_lhs);

    [[nodiscard]] Expr
    primary_expr(bool is_lhs);

    void
    call(Expr &func);

    [[nodiscard]] Expr
    operand(bool is_lhs);

    [[nodiscard]] Expr
    type();

    [[nodiscard]] VarInfo *
    find_variable(String name, u16 *out);

    void
    infer_types(ExprList lhs_list, ExprList rhs_list);

    [[nodiscard]] ExprList
    make_zero_values(Type const *type, int count);

    bool
    check(TokenKind wanted) const noexcept;

    bool
    match(TokenKind wanted) noexcept;

    void
    expect(TokenKind expected);

    void
    advance();

    void
    recurse_push();

    void
    recurse_pop();
}; // struct Parser

[[noreturn]] void
Compiler::error(char const *info, Expr const &e)
{
    this->parser.error_at(info, e);
}

Chunk *
Parser::parse(lulu_State *L, ParserData &data)
{
    Parser   p;
    Compiler c = Compiler(L, p, data.chunk);

    // Parser init.
    p.L          = L;
    p.compiler   = &c;
    p.path       = data.path;
    p.lexer      = Lexer::make(data.input);
    p.scratch    = &data.scratch;
    p.recursions = 0;
    
    p.advance();
    while (!p.check(Token_Eof)) {
        p.simple_stmt();
    }
    p.expect(Token_Eof);
    c.finish();
    return &data.chunk;
}

void
Parser::simple_stmt()
{
    switch (this->token.kind) {
    case Token_Ident:  this->ident_stmt();  break;
    case Token_return: this->return_stmt(); break;
    default:
        this->error("Expected a statement");
        break;
    }

    // Optional.
    this->match(Token_Semicol);
}

/*
 TODO(2026-09-05):
    Allow multiple, comma-separated identifiers in a list?
    E.g. `x, y: T` or `x, y, z := expr1, expr2, expr3`
    or even `x, y, z := f()`.
 */
void
Parser::ident_stmt()
{
    ExprList lhs_list = this->primary_expr_list(/*is_lhs=*/true);
    switch (this->token.kind) {
    case Token_Colon:
        // Consume ':'
        this->advance();
        this->decl(lhs_list);
        break;
    case Token_Assign:
        // Consume '='
        this->advance();
        this->assign(lhs_list);
        break;
    default:
        if (lhs_list.count != 1 || lhs_list->kind != Expr_Call) {
            this->error_at("Expected a declaration, assignment, or function call", *lhs_list);
        }
        break;
    }
    this->scratch->destroy();
}

void
Parser::return_stmt()
{
    // Consume 'return'.
    this->advance();

    ExprList rets = this->expr_list();
    this->compiler->explicit_return(rets);
}


void
Parser::decl(ExprList lhs_list)
{
    Compiler *c = this->compiler;
    c->declare_local(lhs_list);

    // If we have tokens in between ':' and '=', it must be a type.
    // Note that only one (1) type declaration is allowed, e.g. `x, y: int`
    // and not `x, y: int, real`.
    if (!this->check(Token_Assign)) {
        // This should also catch invalid 'types', like `x: 1`.
        Expr tmp = this->type();
        for (Expr &lhs : lhs_list) {
            lhs.type = tmp.type;
        }
    }

    // `x := expr` or `x: T = expr`, but not `x: T`.
    ExprList rhs_list;
    if (this->match(Token_Assign)) {
        rhs_list = this->expr_list();
    }

    // We don't have a type, so we need to infer it from the assigning
    // expressions.
    if (!lhs_list->type) {
        this->infer_types(lhs_list, rhs_list);
    } else if (rhs_list.count == 0) {
        /*
         We do have a type, but we don't have assigning expressions, e.g.
         `x, y: int`. So we need to cough up an equal-length list of
         zero-valued expressions of the appropriate type.

         Note that, grammar-wise, we only allow one (1) type declaration. This
         means that `x, y: int, int` is invalid and would error out before we
         get to this point. So we can assume that all zero values will be of the
         same type.
         */
        rhs_list = this->make_zero_values(lhs_list->type, lhs_list.count);
    }

    // Temporary until we can figure out how to handle function calls.
    LULU_ASSERT(lhs_list.count == rhs_list.count);
    c->define_local(lhs_list, rhs_list);
}

void
Parser::assign(ExprList lhs_list)
{
    // Before anything else, ensure our assignment targets actually
    // exist.
    for (Expr &lhs : lhs_list) {
        if (!lhs.type) {
            this->error_at("Undeclared variable", lhs);
        }
    }

    // Note that, unlike declaration-assignments, the number of assignment
    // targets and assigning expressions MUST match.
    ExprList rhs_list = this->expr_list();
    if (lhs_list.count != rhs_list.count) {
        Expr *tail = rhs_list.last_elem();
        this->error_at("Mismatched number of expressions", *tail);
    }
    this->compiler->assign(lhs_list, rhs_list);
}

/*
 Description:
    Parses a list of comma-separated primary expressions.
 */
ExprList
Parser::primary_expr_list(bool is_lhs)
{
    lulu_State *L = this->L;
    Scratch    *x = this->scratch;
    ExprList    list;
    do {
        Expr expr = this->primary_expr(is_lhs);
        list.append(L, x, expr);
    } while (this->match(Token_Comma));
    return list;
}

/*
 Description:
    Parses a list of comma-separated expressions.
 */
ExprList
Parser::expr_list(bool is_lhs)
{
    lulu_State *L = this->L;
    // Compiler   *c = p->compiler;
    Scratch    *x = this->scratch;
    ExprList    list;
    do {
        Expr expr = this->expr(is_lhs);
        // compiler_expr_next_reg(c, &e);
        list.append(L, x, expr);
    } while (this->match(Token_Comma));
    return list;

}

// Must be higher than all other precedences in `parser_prec()`.
#define PREC_UNARY 8

/*
 Relevant links:
 1) https://odin-lang.org/docs/overview/#operator-precedence
 */
static int
parser_prec(TokenKind k)
{
    switch (k) {
    case Token_Asterisk:
    case Token_Slash:
    case Token_Percent:
    case Token_Ampersand:     return 7;
    case Token_Plus:
    case Token_Dash:
    case Token_Pipe:
    case Token_Caret:         return 6;
    case Token_Greater_Equal:
    case Token_Less_Than:
    case Token_Greater_Than:
    case Token_Less_Equal:    return 5;
    case Token_Equal_Equal:
    case Token_Tilde_Equal:   return 4;
    case Token_and:           return 3;
    case Token_or:            return 2;
    // Precedence 1 is reserved for the base expression parsing.
    // Precedence 0 indicates we cannot parse an expression with this.
    default:
        return 0;
    }
}

/*
 Parses a single expression of the given precedence. By default, we parse
 the basic precedence which is a good starting point.
 */
Expr
Parser::expr(bool is_lhs, int prec_in)
{
    Compiler *c = this->compiler;

    this->recurse_push();
    Expr lhs = this->unary_expr(is_lhs);
    for (;;) {
        Token op       = this->token;
        int   prec_out = parser_prec(op.kind);
        // This also catches tokens that are not binary operators.
        if (prec_out < prec_in) {
            break;
        }

        this->advance();
        if (!lhs.is_literal()) {
            c->expr_any_reg(lhs);
        }

        /*
         Assumptions:
         1) All binary operators are left-associative. We don't have
            exponentiation.
         */
        Expr rhs = this->expr(/*is_lhs=*/false, prec_out + 1);
        c->binary(op, lhs, rhs);
    }
    this->recurse_pop();
    return lhs;
}

/*
 Assumptions:
 1) All unary operators are right-associative. I.e. `cast(bool)cast(int)x` is
    parsed as `cast(bool)(cast(int)x)` which is the same as `bool(int(x))`.
 */
Expr
Parser::unary_expr(bool is_lhs)
{
    Token op = this->token;
    Expr expr;
    switch (op.kind) {
    case Token_cast: {
        this->advance();
        this->expect(Token_Open_Paren);

        Expr type = this->type();
        this->expect(Token_Close_Paren);
        expr = this->expr(/*is_lhs=*/false, PREC_UNARY);
        this->compiler->explicit_cast(type, expr);
        break;
    }
    case Token_Tilde:
    case Token_Dash:
    case Token_Len:
    case Token_not:
        // Skip the unary operand so the first token of the argument
        // is our current.
        this->advance();
        expr = this->expr(is_lhs, PREC_UNARY);
        this->compiler->unary(op, expr);
        break;
    default:
        expr = this->primary_expr(is_lhs);
        break;
    }
    return expr;
}

#undef PREC_UNARY

Expr
Parser::primary_expr(bool is_lhs)
{
    bool loop = true;
    Expr expr = this->operand(is_lhs);
    while (loop) {
        switch (this->token.kind) {
        case Token_Open_Paren:
            // Consume '('.
            this->advance();
            this->call(expr);
            break;
        default:
            loop = false;
            break;
        }
        // After the first atom, we are no longer assignable.
        is_lhs = false;
    }
    return expr;
}

void
Parser::call(Expr &func)
{
    Expr arg;
    if (!this->check(Token_Close_Paren)) {
        arg = this->expr();
    }
    this->expect(Token_Close_Paren);
    this->compiler->call(func, arg);
}


/*
 Description:
    'operand' refers to our most basic expression. These are literals,
    identifiers, unary operations, and table constructors.

 Note (2026-07-03):
    This is about the only place `lhs` is useful. It allows us to avoid
    needlessly parsing a compound literal (in our case, a table) when in
    declarations/assignments. See Odin's parser:

    https://github.com/odin-lang/Odin/blob/1007ea278534e037fa586564c91113f5c925c286/src/parser.cpp#L2379
 */
Expr
Parser::operand(bool is_lhs)
{
    Token token = this->token;
    this->advance();
    switch (token.kind) {
    case Token_nil:     return Expr::make_nil (token);
    case Token_false:   return Expr::make_bool(token, false);
    case Token_true:    return Expr::make_bool(token, true);
    case Token_Int:     return Expr::make_int (token, token.integer);
    case Token_Float:   return Expr::make_real(token, token.floating);
    case Token_Open_Paren:
    {
        Expr res = this->expr(is_lhs);
        this->expect(Token_Close_Paren);
        return res;
    }
    case Token_Open_Curly:
        if (is_lhs) {
            this->error_at("Cannot assign to a compound literal", token);
        } else {
            this->error_at("Table constructors not yet supported", token);
        }
        break;
    case Token_Ident:
    {
        String ident = token.loc.view;
        Option<Type const *> type = type_get(this->L, ident);
        if (type.is_some()) {
            return Expr::make_type(token, type.unwrap());
        } else {
            u16      i;
            VarInfo *v = this->find_variable(ident, &i);

            /*
             Ensures that, for rvalue expressions, we absolutely have a
             variable to work with.

             For lvalue expressions, however, we can be more lenient.
             Especially for declarations since the variable referred to
             the identifier may not yet exist.
             */
            if (!v && !is_lhs) {
                this->error_at("Unknown identifier", token);
            }
            return Expr::make_local(token, (v) ? v->type : nullptr, i);
        }
        break;
    }
    default:
        this->error_at("Expected an operand", token);
        break;
    }
}

/*
 Assumptions:
 1) We are about to consume an identifier.
 */
Expr
Parser::type()
{
    Token const token = this->token;
    this->expect(Token_Ident);

    Option<Type const *> o = type_get(this->L, token.loc.view);
    if (o.is_none()) {
        this->error_at("Unknown type name", token);
    }
    return Expr::make_type(token, o.unwrap());
}

/*
 TODO(2026-07-20): Add global lookup instead of worrying only about locals.
 */
VarInfo *
Parser::find_variable(String name, u16 *out)
{
    // Loop invariants.
    Compiler *     c      = this->compiler;
    Slice<VarInfo> locals = c->slice_active_locals();
    for (VarInfo &v : reverse(locals)) {
        if (name == v.loc.view) {
            if (out) {
                *out = cast(u16)locals.index_ptr_unsafe(&v);
            }
            return &v;
        }
    }

    if (out) {
        *out = cast(u16)-1;
    }
    return nullptr;
}

void
Parser::infer_types(ExprList lhs_list, ExprList rhs_list)
{
    // We want to infer the type but we literally don't have anything
    // to infer *from*, e.g. `x:`.
    if (rhs_list.count == 0) {
        // Report the error at the *last* local variable name.
        Expr *last = lhs_list.last_elem();
        this->error_at("Expected a type after ':'", *last);
    }

    if (lhs_list.count != rhs_list.count) {
        Expr *last = rhs_list.last_elem();
        this->error_at("Mismatched number of expressions", *last);
    }

    // Copy over all assigning expression types to the targets so the
    // compiler can see them.
    ExprList tmp = lhs_list;
    for (Expr rhs : rhs_list) {
        Expr &lhs = *tmp++;
        lhs.type = rhs.type;
    }
}

ExprList
Parser::make_zero_values(Type const *type, int count)
{
    Expr zero;
    switch (type->kind) {
    case TypeKind_Basic:
        zero.kind         = Expr_Literal;
        zero.literal_kind = type->basic.kind;
        zero.type         = type;
        switch (zero.literal_kind) {
        case Value_bool:
            zero.set_bool(false);
            zero.loc.view = "false"_s;
            break;
        case Value_int:
            zero.set_intr(0);
            zero.loc.view = "0"_s;
            break;
        case Value_real:
            zero.set_real(0.0);
            zero.loc.view = "0.0"_s;
            break;
        default:
            LULU_PANICF("Unsupported zero type for ValueType(%i)", zero.literal_kind);
            break;
        }
        break;
    default:
        LULU_PANICF("Unsupported zero type for TypeKind(%i)", type->kind);
        break;
    }

    ExprList rhs_list;
    for (int i = 0; i < count; i++) {
        rhs_list.append(this->L, this->scratch, zero);
    }
    return rhs_list;
}

// Scan a new current token.
void
Parser::advance()
{
    LexerResult result = this->lexer.scan_token();
    if (result.is_err()) {
        LexerError err = result.unwrap_err();
        this->error_at(lexer_error_string(err.kind), err.loc);
    }
    this->token = result.unwrap();
}


bool
Parser::check(TokenKind wanted) const noexcept
{
    return this->token.kind == wanted;
}

bool
Parser::match(TokenKind wanted) noexcept
{
    bool found = this->check(wanted);
    if (found) {
        this->advance();
    }
    return found;
}

void
Parser::expect(TokenKind expected)
{
    if (!this->match(expected)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Expected '%s'", token_kind_cstring(expected));
        this->error(buf);
    }
}

static char const *
parser_clamp_string(Slice<char> buf, String s)
{
    usize it = 0;

    // The iteration range is exclusive, so we save the last index for the
    // nul character.
    usize stop = min(s.len(), buf.len() - 1);

    // Prefix "..." to indicate that the full string was truncated and that
    // you're seeing only the tail portion that fits.
    if (s.len() > stop) {
        buf[it++] = '.';
        buf[it++] = '.';
        buf[it++] = '.';
    }

    for (; it < stop; it++) {
        buf[it] = s[it];
    }
    buf[it] = 0;
    return buf.raw_data();
}

[[noreturn]] void
Parser::error_at(char const *info, Loc const &loc)
{
    char name[80];
    char loc_str[80];
    fprintf(stderr, "%s:%i:%i: %s at '%s'\n",
        parser_clamp_string({name, sizeof(name)}, this->path),
        loc.pos.line, loc.pos.col, info,
        parser_clamp_string({loc_str, sizeof(loc_str)}, loc.view));

    state_throw(this->L, LULU_SYNTAX_ERROR);
}

void
Parser::recurse_push()
{
    LULU_ASSERT(this->recursions + 1 < PARSER_MAX_RECURSIONS);
    this->recursions++;
}

void
Parser::recurse_pop()
{
    LULU_ASSERT(this->recursions - 1 >= 0);
    this->recursions--;
}

