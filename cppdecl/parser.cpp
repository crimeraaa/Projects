#include <cstdio>  // printf
#include <cstring> // memcpy

#include "parser.hpp"

void
Parser::advance()
{
    Error err;

    m_consumed = m_lookahead;
    err = m_lexer.scan_token(&m_lookahead);
    switch (err) {
    case ERR_NONE:
    case ERR_EOF:
        break;
    case ERR_UNEXPECTED_TOKEN:
        error("Unexpected token"_s);
        break;
    case ERR_UNTERMINATED_COMMENT:
        error("Unterminated multiline comment"_s);
        break;
    case ERR_UNTERMINATED_STRING:
        error("Unterminated multiline string"_s);
        break;
    case ERR_INVALID_NUMBER:
        error("Invalid number"_s);
        break;
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

void
Parser::expect(Token_Kind kind)
{
    if (!match(kind)) {
        // Making the token enum to string table is VERY annoying
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "Expected Token_Kind(%i)", kind);
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
        return expr();
    } catch (Error err) {
        if (err == ERR_OUT_OF_MEMORY) {
            *m_message = "Out of memory"_s;
        }
        return nullptr;
    }
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
    Token t = m_lookahead;

    // Prefix
    // Don't advance yet so we can better report errors.
    switch (t.kind) {
    case TK_LPAREN:
        // Consume the left parenthesis.
        advance();
        node = expr();
        expect(TK_RPAREN);
        break;
    case TK_BAND: node = unary(AST_LEA);    break;
    case TK_BNOT: node = unary(AST_BNOT);   break;
    case TK_NOT:  node = unary(AST_NOT);    break;
    case TK_SUB:  node = unary(AST_NEG);    break;
    case TK_MUL:  node = unary(AST_DEREF);  break;
    case TK_LITERAL_FLOAT:
    case TK_LITERAL_INT:
        // Consume the literal's token.
        advance();

        // New terminal
        node = Ast::make<Ast_Literal>(m_arena, t.data);
        break;
    default:
        error("Expected an expression"_s);
        return nullptr;
    }

    // Infix
    for (;;) {
        const Rule r = get_rule(m_lookahead.kind);
        // Parent caller is of a higher precedence?
        if (r.left < prec) {
            break;
        }
        // Consume the infix operator so that our lookahead is now the
        // first token of the right hand side expression.
        advance();
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
    Ast *right = expr(prec);
    return Ast::make<Ast_Infix>(m_arena, kind, left, right);
}

Parser::Rule
Parser::get_rule(Token_Kind kind)
{
#define LEFT(p, k)   {p, cast(Precedence)(cast(int)p + 1), k}
#define RIGHT(p, k)  {p, p, k}
    switch (kind) {
    case TK_ASSIGN: return RIGHT(PREC_ASSIGN, AST_ASSIGN);
    case TK_ADD:    return LEFT(PREC_TERM,    AST_ADD);
    case TK_SUB:    return LEFT(PREC_TERM,    AST_SUB);
    case TK_MUL:    return LEFT(PREC_FACTOR,  AST_MUL);
    case TK_DIV:    return LEFT(PREC_FACTOR,  AST_DIV);
    case TK_MOD:    return LEFT(PREC_FACTOR,  AST_MOD);
    default:
        break;
    }
    return {};
#undef RIGHT
#undef LEFT
}

static const char *
ast_kind_name(Ast_Kind kind, u8 sub_kind)
{
    switch (kind) {
    case AST_NONE:
        break;
    case AST_LITERAL:
        switch (cast(Ast_Literal_Kind)sub_kind) {
        case UNTYPED_NONE:  break;
        case UNTYPED_INT:   return "Int";
        case UNTYPED_FLOAT: return "Float";
        }
        break;
    case AST_VARIABLE:
        break;
    case AST_PREFIX:
        switch (cast(Ast_Prefix_Kind)sub_kind) {
        case AST_DEREF:     return "deref";
        case AST_NEG:       return "neg";
        case AST_LEA:       return "lea";
        case AST_NOT:       return "not";
        case AST_BNOT:      return "bnot";
        }
    case AST_INFIX:
        switch (cast(Ast_Infix_Kind)sub_kind) {
        case AST_ASSIGN:    return "assign";
        case AST_INDEX:     return "index";
        case AST_ADD:       return "add";
        case AST_SUB:       return "sub";
        case AST_MUL:       return "mul";
        case AST_DIV:       return "div";
        case AST_MOD:       return "mod";
        }
    }
    return nullptr;
}

void
Parser::error(String msg)
{
#define X(s)    cast(int)len(s), raw_data(s)

    String loc = m_lookahead.lexeme;
    if (len(loc) == 0) {
        loc = "<eof>"_s;
    }

    // We won't return the nodes anyway.
    m_arena->free_all();

    size_t cap = len(msg) + len(loc) + 16;
    char  *s   = m_arena->alloc<char>(cap);
    if (s == nullptr) {
        throw ERR_OUT_OF_MEMORY;
    }

    int n = std::snprintf(s, cap, "%.*s at \"%.*s\"\n", X(msg), X(loc));
    // Must be non-negative and non-zero (i.e. no errors occured).
    assert(n > 0);
    *m_message = {s, cast(size_t)n};

    throw ERR_UNEXPECTED_TOKEN;

#undef X
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
        case UNTYPED_NONE:  break;
        case UNTYPED_INT:   printf("Int(%llu)", lit.i());    break;
        case UNTYPED_FLOAT: printf("Float(%.14g)", lit.f()); break;
        }
        break;
    }
    case AST_VARIABLE:
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

