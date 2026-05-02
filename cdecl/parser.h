#ifndef CDECL_PARSER_H
#define CDECL_PARSER_H

#include "lexer.h"
#include "table.h"

struct Parser {
    struct Lexer lexer;
    struct Token consumed, lookahead;
    struct Table *symbols;
};

enum Type_Error
parser_init(struct Parser *p, const char *input, size_t input_len,
            struct Table *symbols);

enum Type_Error
parser_parse(struct Parser *p, const struct Type_Info **out);

#endif // !CDECL_PARSER_H
