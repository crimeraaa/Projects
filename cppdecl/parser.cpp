// f iostream
#include <cstdio>

#include "parser.hpp"

Error
Parser::advance()
{
    this->consumed = this->lookahead;
    return this->lexer.scan_token(&this->lookahead);
}

static void
print_token(Token *t)
{
    std::fprintf(stdout, "%i:%i: \"%.*s\"\n",
                 t->line, t->col, cast(int)len(t->text), raw_data(t->text));
}

Ast *
Parser::program()
{
    // for (;;) {
    //    Error err = this->advance();
    //    if (err) {
    //         if (err == ERR_EOF) {
    //             std::fputs("<eof>", stdout);
    //        } else {
    //            std::fprintf(stdout, "Error(%i) @ ", cast(int)err);
    //            print_token(&this->lookahead);
    //        }
    //        std::fputc('\n', stdout);
    //        break;
    //     }
    //     print_token(&this->lookahead);
    // }

    // Put the first token in the lookahead.
    this->advance();
    return this->expr();
}

Ast *
Parser::def()
{
    switch (this->lookahead.kind) {
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
    switch (this->lookahead.kind) {
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
    Token t = this->lookahead;

    // Prefix
    // Consume the prefix operator (e.g. unary operator or literal).
    // This ensures the correct state for parsing nested expressions.
    this->advance();
    switch (t.kind) {
    case TK_MINUS:     node = this->unary(AST_NEG);    break;
    case TK_ASTERISK:  node = this->unary(AST_DEREF);  break;
    case TK_AMPERSAND: node = this->unary(AST_ADDR);   break;
    case TK_LITERAL_FLOAT:
    case TK_LITERAL_INT:
        node  = this->arena->alloc<Ast>();
        *node = t.data;
        break;
    default:
        // Expected an expression!
        break;
    }
    

    // Infix
    for (;;) {
        const Rule r = this->get_rule(this->lookahead.kind);
        // Parent caller is of a higher precedence OR nothing to do?
        if (r.left < prec || r.infix == nullptr) {
            break;
        }
        node = (this->*r.infix)(node, r.kind, r.right);
    }
    return node;
}

Ast *
Parser::unary(Ast_Prefix_Kind kind)
{
    Ast *node, *arg;
    node  = this->arena->alloc<Ast>();
    arg   = this->expr(PREC_UNARY);
    *node = Ast_Prefix{kind, arg};
    return node;
}

Ast *
Parser::arith(Ast *left, Ast_Infix_Kind kind, Precedence prec)
{
    Ast *right = this->expr(prec);
    Ast *node  = this->arena->alloc<Ast>();
    *node = Ast_Infix{kind, left, right};
    printf("left=%p, right=%p\n", left, right);
    return node;
}

Parser::Rule
Parser::get_rule(Token_Kind kind)
{
    printf("Token_Kind(%i)\n", kind);
#define LEFT(p, k, f)   {p, cast(Precedence)(cast(int)p + 1), k, f}
#define RIGHT(p, k, f)  {p, p, k, f}
    switch (kind) {
    case TK_PLUS:     return LEFT(PREC_TERM,   AST_ADD, &Parser::arith);
    case TK_MINUS:    return LEFT(PREC_TERM,   AST_SUB, &Parser::arith);
    case TK_ASTERISK: return LEFT(PREC_FACTOR, AST_MUL, &Parser::arith);
    case TK_SLASH:    return LEFT(PREC_FACTOR, AST_DIV, &Parser::arith);
    default:
        break;
    }
    return {};
#undef RIGHT
#undef LEFT
}

void
Ast::dump(const int depth) const
{
    for (int i = 0; i < depth; i += 1) {
        std::printf("\t");
    }

    if (depth > 0) {
        printf("| ");
    }

    switch (this->kind()) {
    case AST_NONE:
    case AST_DECL:
        break;
    case AST_LITERAL:
    {
        Untyped ut = this->literal();
        switch (ut.kind()) {
        case UNTYPED_NONE:  break;
        case UNTYPED_INT:   std::printf("Int(%llu)", ut.i());    break;
        case UNTYPED_FLOAT: std::printf("Float(%.14g)", ut.f()); break;
        }
        break;
    }
    case AST_VARIABLE:
        break;
    case AST_PREFIX:
        switch (prefix()->kind) {
        case AST_DEREF: std::printf("DEREF");     break;
        case AST_NEG:   std::printf("NEGATE");    break;
        case AST_ADDR:  std::printf("ADDRESSOF"); break;
        }
        std::printf("\n");
        this->prefix()->arg->dump(depth + 1);
        break;
    case AST_INFIX:
        switch (this->infix()->kind) {
        case AST_ASSIGN:  std::printf("ASSIGN"); break;
        case AST_INDEX:   std::printf("INDEX");  break;
        case AST_ADD:     std::printf("ADD");    break;
        case AST_SUB:     std::printf("SUB");    break;
        case AST_MUL:     std::printf("MUL");    break;
        case AST_DIV:     std::printf("DIV");    break;
        }
        std::printf("\n");
        this->infix()->left->dump(depth + 1);
        printf("\n");
        this->infix()->right->dump(depth + 1);
        break;
    }

    if (depth == 0) {
        std::printf("\n;");
    }
}

