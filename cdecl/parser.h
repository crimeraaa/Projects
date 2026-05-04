#ifndef CDECL_PARSER_H
#define CDECL_PARSER_H

#include "lexer.h"
#include "table.h"

typedef struct Parser Parser;
struct Parser {
    Lexer lexer;
    Token consumed, lookahead;
    Table *symbols;
};

Type_Error
parser_init(Parser *p, const char *input, size_t input_len, Table *symbols);

const Type_Info *
parser_parse(Parser *p, Type_Error *out);

#endif // !CDECL_PARSER_H
