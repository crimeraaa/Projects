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
    for (;;) {
        Error err = this->advance();
       if (err) {
            if (err == ERR_EOF) {
                std::fputs("<eof>", stdout);
           } else {
               std::fprintf(stdout, "Error(%i) @ ", cast(int)err);
               print_token(&this->lookahead);
           }
           std::fputc('\n', stdout);
           break;
        }
        print_token(&this->lookahead);
    }
    return nullptr;
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

