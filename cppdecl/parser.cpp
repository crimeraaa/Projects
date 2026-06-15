#include <cstdio>  // printf, snprintf
#include <cstring> // memcpy

#include "parser.hpp"

void
Parser::advance()
{
    Error err;

    m_consumed = m_lookahead;
    err = m_lexer.scan_token(&m_lookahead);
    // `error()` throws an exception and thus never returns.
    switch (err) {
    case ERR_UNEXPECTED_TOKEN:     error("Unexpected token"_s);
    case ERR_UNTERMINATED_COMMENT: error("Unterminated multiline comment"_s);
    case ERR_UNTERMINATED_STRING:  error("Unterminated multiline string"_s);
    case ERR_INVALID_NUMBER:       error("Invalid number"_s);
    default:
        break;
    }
}

bool
Parser::check(Token_Kind kind) const
{
    return m_lookahead.kind == kind;
}

bool
Parser::match(Token_Kind kind)
{
    bool found = check(kind);
    if (found) {
        advance();
    }
    return found;
}

#define X(KW) #KW

// ORDER: Sync with `enum Token_Kind`!
internal const char *
TOKEN_STRINGS[TK_MULTICHAR_COUNT] = {
    KEYWORD_LIST(X),
    "...", "->", "::",
    "<<", ">>", "&&", "||", "==", "!=", "<=", ">=",
    "++", "--",
    "&=", "|=", "^=",
    "<<=", ">>=",
    "+-", "-=", "*=", "/=", "%=",
    "<char>",
    "<int>", "<float>", "<string>",
    "<ident>",
    "<eof>",
};

#undef X

void
Parser::expect(Token_Kind kind)
{
    using std::snprintf;
    if (!match(kind)) {
        // NOTE: Check assumption if we modify the token kinds!
        // Longest keyword (that we support) is char[9]: namespace.
        // The longest ever C++ keyword (as of 2026) is char[16]: reinterpret_cast.
        char buf[32];
        int n;
        if (kind < TK_MULTICHAR_START) {
            n = snprintf(buf, sizeof(buf), "Expected '%c'", kind);
        } else {
            kind = cast(Token_Kind)(cast(int)kind - TK_MULTICHAR_START - 1);
            n = snprintf(buf, sizeof(buf), "Expected '%s'", TOKEN_STRINGS[kind]);
        }
        assert(n > 0);
        error({buf, cast(size_t)n});
    }
}

Ast *
Parser::program(String *out)
{
    String tmp{};
    m_message = (out != nullptr) ? out : &tmp;
    try {
        // Put the first token in the lookahead.
        advance();
        Ast *root = expr();
        // Optional, for now.
        match(TK_SEMICOL);
        expect(TK_EOF);
        return root;
    } catch (Error err) {
        if (err == ERR_OUT_OF_MEMORY) {
            *m_message = "Out of memory"_s;
        }
    }
    return nullptr;
}

Ast *
Parser::def()
{
    switch (m_lookahead.kind) {
    case TK_struct:
    case TK_typedef:
    case TK_using:
        break;
    default:
        // Need a type. May be a func or var-decl.
        break;
    }
    return nullptr;
}

Ast *
Parser::type()
{
    switch (m_lookahead.kind) {
    case TK_bool:
    case TK_double:
    case TK_int:
    case TK_void:
    default:
        break;
    }
    return nullptr;
}

Ast *
Parser::expr(Precedence prec)
{
    Ast *node = nullptr;

    // Prefix
    // Don't advance yet so we can better report errors.
    switch (m_lookahead.kind) {
    case TK_LPAREN:
        // Consume the left parenthesis.
        // TODO: Disambiguate from type casts!
        advance();
        node = expr();
        expect(TK_RPAREN);
        break;
    case TK_BAND: node = unary(AST_LEA);    break;
    case TK_BNOT: node = unary(AST_BNOT);   break;
    case TK_NOT:  node = unary(AST_NOT);    break;
    case TK_SUB:  node = unary(AST_NEG);    break;
    case TK_MUL:  node = unary(AST_DEREF);  break;
    case TK_INCR: node = unary(AST_INCR);   break;
    case TK_DECR: node = unary(AST_DECR);   break;
    case TK_LITERAL_FLOAT:
    case TK_LITERAL_INT:
        // Consume the literal's token.
        // Literals are always terminals in the grammar.
        advance();
        node = Ast::make<Ast_Literal>(m_arena, m_consumed.data);
        break;
    case TK_IDENT:
        // TODO: When we introduce functions, track locals.
        advance();
        node = Ast::make<Ast_Variable>(m_arena, m_consumed.lexeme, /*scope=*/0);
        break;
    default:
        error("Expected an expression"_s);
    }

    // Infix
    for (;;) {
        const Rule r = get_rule(node, m_lookahead.kind);
        // Parent caller is of a higher precedence?
        if (r.left < prec) {
            break;
        }
        node = binary(node, r.kind, r.right);
    }
    return node;
}

Ast *
Parser::unary(Ast_Prefix_Kind kind)
{
    // Consume the unary operator.
    advance();
    Ast *arg = expr(PREC_UNARY);
    return Ast::make<Ast_Prefix>(m_arena, kind, arg);
}

Ast *
Parser::binary(Ast *left, Ast_Infix_Kind kind, Precedence prec)
{
    // Consume the infix operator so that our lookahead is now the
    // first token of the right hand side expression.
    advance();
    Ast *right = expr(prec);
    if (kind == AST_INDEX) {
        expect(TK_RSQUARE);
    }
    // else if (kind == AST_CALL) {
    //     expect(TK_RPAREN);
    // }
    return Ast::make<Ast_Infix>(m_arena, kind, left, right);
}

Parser::Rule
Parser::get_rule(Ast *left, Token_Kind kind)
{
#define LEFT(p, k)   {p, cast(Precedence)(cast(int)p + 1), k}
#define RIGHT(p, k)  {p, p, k}
    switch (kind) {
    case TK_ASSIGN:  return RIGHT(PREC_ASSIGN, AST_ASSIGN);
    case TK_LSQUARE: return LEFT(PREC_ASSIGN,  AST_INDEX);

    // Bitwise
    case TK_BAND: return LEFT(PREC_BAND,   AST_BAND);
    case TK_BOR:  return LEFT(PREC_BOR,    AST_BOR);
    case TK_BXOR: return LEFT(PREC_BXOR,   AST_BXOR);
    case TK_SHL:  return LEFT(PREC_BSHIFT, AST_SHL);
    case TK_SHR:  return LEFT(PREC_BSHIFT, AST_SHR);

    // Logical
    case TK_AND: return LEFT(PREC_AND, AST_AND);
    case TK_OR:  return LEFT(PREC_OR,  AST_OR);

    // Equality
    case TK_EQ:  return LEFT(PREC_EQ, AST_EQ);
    case TK_NEQ: return LEFT(PREC_EQ, AST_NEQ);

    // Relational
    case TK_LT:  return LEFT(PREC_REL, AST_LT);
    case TK_LEQ: return LEFT(PREC_REL, AST_LEQ);
    case TK_GT:  return LEFT(PREC_REL, AST_GT);
    case TK_GEQ: return LEFT(PREC_REL, AST_GEQ);

    // Arithmetic
    case TK_ADD: return LEFT(PREC_TERM,   AST_ADD);
    case TK_SUB: return LEFT(PREC_TERM,   AST_SUB);
    case TK_MUL: return LEFT(PREC_FACTOR, AST_MUL);
    case TK_DIV: return LEFT(PREC_FACTOR, AST_DIV);
    case TK_MOD: return LEFT(PREC_FACTOR, AST_MOD);
    default:
        break;
    }
    return {};
#undef RIGHT
#undef LEFT
}

internal const char *
ast_kind_name(Ast_Kind kind, u8 sub_kind)
{
    switch (kind) {
    case AST_NONE:
        break;
    case AST_LITERAL:
        switch (cast(Ast_Literal_Kind)sub_kind) {
        case UNTYPED_NONE:   break;
        case UNTYPED_INT:    return "Int";
        case UNTYPED_FLOAT:  return "Float";
        case UNTYPED_STRING: return "String";
        }
        break;
    case AST_VARIABLE:
        break;
    case AST_PREFIX:
        switch (cast(Ast_Prefix_Kind)sub_kind) {
        case AST_DEREF: return "DEREF";
        case AST_NEG:   return "NEG";
        case AST_LEA:   return "LEA";
        case AST_NOT:   return "NOT";
        case AST_BNOT:  return "BNOT";
        case AST_INCR:  return "INCR";
        case AST_DECR:  return "DECR";
        }
    case AST_INFIX:
        switch (cast(Ast_Infix_Kind)sub_kind) {
        case AST_ASSIGN: return "ASSIGN";
        case AST_INDEX:  return "INDEX";

        // Bitwise
        case AST_BAND: return "BAND";
        case AST_BOR:  return "BOR";
        case AST_BXOR: return "BXOR";
        case AST_SHL:  return "SHL";
        case AST_SHR:  return "SHR";

        // Logical
        case AST_AND: return "AND";
        case AST_OR:  return "OR";

        // Relational
        case AST_EQ:  return "EQ";
        case AST_NEQ: return "NEQ";
        case AST_LT:  return "LT";
        case AST_LEQ: return "LEQ";
        case AST_GT:  return "GT";
        case AST_GEQ: return "GEQ";

        // Arithmetic
        case AST_ADD: return "ADD";
        case AST_SUB: return "SUB";
        case AST_MUL: return "MUL";
        case AST_DIV: return "DIV";
        case AST_MOD: return "MOD";
        }
    }
    return nullptr;
}

void
Parser::error(String msg)
{
    using std::snprintf;

    String loc = m_lookahead.lexeme;
    if (len(loc) == 0) {
        loc = "<eof>"_s;
    }

    // We won't return the nodes anyway.
    m_arena->free_all();

    // TODO: clamp length of `loc` similar to how Lua does it.
    size_t cap = len(msg) + len(loc) + 16;
    char  *s   = m_arena->alloc<char>(cap);
    if (s == nullptr) {
        throw ERR_OUT_OF_MEMORY;
    }

    int n = snprintf(s, cap, "%.*s at '%.*s'", STRINGX(msg), STRINGX(loc));
    // Must be non-negative and non-zero (i.e. no errors occured).
    assert(n > 0);
    *m_message = {s, cast(size_t)n};

    throw ERR_UNEXPECTED_TOKEN;
}

void
Ast::dump(const int depth) const
{
    using std::printf;
    using std::fputs;
    using std::fputc;

    for (int i = 0; i < depth - 1; i += 1) {
        fputs("|---", stdout);
    }

    if (depth > 0) {
        fputs("|-->", stdout);
    }

    switch (kind()) {
    case AST_NONE:
        break;
    case AST_LITERAL:
    {
        Ast_Literal lit = literal();
        switch (lit.kind()) {
        case UNTYPED_NONE:   break;
        case UNTYPED_INT:    printf("Int(%llu)", lit.i());    break;
        case UNTYPED_FLOAT:  printf("Float(%.14g)", lit.f()); break;
        case UNTYPED_STRING: printf("String('%.*s')", STRINGX(lit.s())); break;
        }
        break;
    }
    case AST_VARIABLE:
        printf("Var('%.*s')", STRINGX(variable()->ident));
        break;
    case AST_PREFIX:
        printf("%s\n", ast_kind_name(kind(), prefix()->kind));
        prefix()->arg->dump(depth + 1);
        break;
    case AST_INFIX:
        printf("%s\n", ast_kind_name(kind(), infix()->kind));
        infix()->left->dump(depth + 1);
        printf("\n");
        infix()->right->dump(depth + 1);
        break;
    }

    if (depth == 0) {
        printf("\n;");
    }
}

