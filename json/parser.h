#ifndef JSON_PARSER_H
#define JSON_PARSER_H

// local
#include "lexer.h"

typedef struct json_Parser json_Parser;
struct json_Parser {
    json_Lexer lexer;
    json_Token consumed, lookahead;
    mem_Allocator alloc;
};

extern json_Error
json_init_parser(
    json_Parser *p,
    const char *input, size_t len,
    mem_Allocator alloc
);

extern json_Error
json_parse(json_Parser *p, json_Value *value);

extern uint16_t
json_char_to_hex(char c);

extern uint16_t
json_decode_hex4(const char *s, size_t n, size_t *byte_count);

#endif // !JSON_PARSER_H
