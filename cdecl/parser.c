// standard
#include <string.h> // memset

// local
#include "cdecl.h"
#include "lexer.h"
#include "parser.h"

typedef struct Data Data;
struct Data {
    // General type class for broad comparisons.
    Type_Info_Variant_Kind variant_kind;

    // Concrete primitive type, if there is one.
    Type_Kind_Basic basic_kind;

    // Bit set of which token kinds are present or not.
    uint32_t tokens;

    // Bit set of which qualifier flags are present or not.
    uint8_t quals;
};

static bool
data_contains_token(Data d, Token_Kind k)
{
    return (d.tokens & (1U << cast(uint32_t)k)) != 0;
}

// Returns `true` if `k` was added to the bit set for the first time,
// else `false` if it was present beforehand.
static bool
data_added_token(Data *d, Token_Kind k)
{
    bool found = data_contains_token(*d, k);
    if (!found) {
        d->tokens |= (1U << cast(uint32_t)k);
    }
    return !found;
}

static bool
data_set_variant_basic(Data *d, Type_Info_Variant_Kind vk, Type_Kind_Basic bk)
{
    bool unset = d->variant_kind == TYPE_VKIND_VOID;
    if (unset) {
        d->variant_kind = vk;
        d->basic_kind   = bk;
    }
    return unset;
}

// static void
// token_set_remove(Token_Set *s, Token_Kind k)
// {
//     *s = *s & ~(1U << cast(uint32_t)k);
// }


// Returns `true` if `f` was added to the bit set for the first time,
// else `false` if it was present beforehand.
static bool
data_contains_qual(Data d, Type_Qual_Flag f)
{
    return (d.quals & cast(uint8_t)f) != 0;
}

static bool
data_added_qual(Data *d, Type_Qual_Flag f)
{
    bool found = data_contains_qual(*d, f);
    if (!found) {
        d->quals |= cast(uint8_t)f;
    }
    return !found;
}

static Type_Error
parser_next_token(Parser *p)
{
    p->consumed = p->lookahead;
    return lexer_scan_token(&p->lexer, &p->lookahead);
}

Type_Error
parser_init(Parser *p, const char *input, size_t input_len, Table *symbols)
{
    memset(p, 0, sizeof(*p));
    lexer_init(&p->lexer, input, input_len);
    p->symbols = symbols;
    return parser_next_token(p);
}

static bool
parse_data_type(Parser *p, Data *d, Type_Error *out)
{
    Token_Kind token_kind;

    token_kind = p->lookahead.kind;
    *out       = parser_next_token(p);
    if (*out) {
        return false;
    }

    // Duplicate token?
    if (!data_added_token(d, token_kind)) {
        // Duplicate tokens are always an error except for `long long`.
        // We only allow `long` to repeat twice; any more is an error.
        if (token_kind != TOKEN_LONG || d->basic_kind == TYPE_LLONG) {
fucked_up:
            *out = TYPE_ERROR_UNEXPECTED_TOKEN;
            return false;
        }
    }

    // Simplest case: variable declaration of a simple type.
    switch (token_kind) {
    case TOKEN_BOOL:
        if (!data_set_variant_basic(d, TYPE_VKIND_BOOL, TYPE_BOOL)) {
            goto fucked_up;
        }
        break;
    // Integer
    case TOKEN_CHAR:
        if (!data_set_variant_basic(d, TYPE_VKIND_INTEGER, TYPE_CHAR)) {
            goto fucked_up;
        }
        break;
    case TOKEN_SHORT:
        if (!data_set_variant_basic(d, TYPE_VKIND_INTEGER, TYPE_SHORT)) {
            goto fucked_up;
        }
        break;
    case TOKEN_INT:
        if (!data_set_variant_basic(d, TYPE_VKIND_INTEGER, TYPE_INT)) {
            goto fucked_up;
        }
        break;

    case TOKEN_LONG:
        switch (d->basic_kind) {
        case TYPE_VOID:
        case TYPE_INT:    d->basic_kind = TYPE_LONG;    break;
        case TYPE_LONG:   d->basic_kind = TYPE_LLONG;   break;
        case TYPE_UINT:   d->basic_kind = TYPE_ULONG;   break;
        case TYPE_ULONG:  d->basic_kind = TYPE_ULLONG;  break;
        case TYPE_DOUBLE: d->basic_kind = TYPE_LDOUBLE; break;
        default:
            goto fucked_up;
        }
        break;

    // Float
    case TOKEN_FLOAT:
    case TOKEN_DOUBLE:
        d->variant_kind = TYPE_VKIND_FLOATING;
        break;

    // Qualifiers
    case TOKEN_CONST:
        if (!data_added_qual(d, TYPE_QUAL_CONST)) {
            goto fucked_up;
        }
        break;
    case TOKEN_VOLATILE:
        if (!data_added_qual(d, TYPE_QUAL_VOLATILE)) {
            goto fucked_up;
        }
        break;
    case TOKEN_SIGNED:
        if (!data_added_qual(d, TYPE_QUAL_SIGNED)) {
            goto fucked_up;
        }
        break;
    case TOKEN_UNSIGNED:
        if (!data_added_qual(d, TYPE_QUAL_UNSIGNED)) {
            goto fucked_up;
        }
        break;

    case TOKEN_IDENTIFIER:
        // Don't want to deal with this just yet.
        *out = TYPE_ERROR_UNEXPECTED_TOKEN;
        break;

    case TOKEN_EOF:
        *out = TYPE_ERROR_EOF;
        break;

    default:
        *out = TYPE_ERROR_UNEXPECTED_TOKEN;
        break;
    }
    return true;
}

const Type_Info *
parser_parse(Parser *p, Type_Error *out)
{
    Data data = {TYPE_VKIND_VOID, TYPE_VOID, 0, 0};
    parse_data_type(p, &data, out);

    // Fix data type
    if (data.basic_kind == TYPE_VOID) {
        // lone `signed` or `unsigned` resolve to their int counterparts.
        if (data_contains_qual(data, TYPE_QUAL_SIGNED)) {
            data.basic_kind = TYPE_INT;
        } else if (data_contains_qual(data, TYPE_QUAL_UNSIGNED)) {
            data.basic_kind = TYPE_UINT;
        }
    }
    return NULL;
}
