#include "lexer.h"
#include "parser.h"

static enum Type_Error
parser_next_token(struct Parser *p)
{
    p->consumed = p->lookahead;
    return lexer_scan_token(&p->lexer, &p->lookahead);
}

enum Type_Error
parser_init(struct Parser *p, const char *input, size_t input_len,
            struct Table *symbols)
{
    memset(p, 0, sizeof(*p));
    lexer_init(&p->lexer, input, input_len);
    p->symbols = symbols;
    return parser_next_token(p);
}

enum Type_Error
parser_parse(struct Parser *p, const struct Type_Info **out)
{
    return TYPE_ERROR_NONE;
}
