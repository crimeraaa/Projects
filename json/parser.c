// standard
#include <stdlib.h> // strtod
#include <string.h> // memcpy

// local
#include "json.h"
#include "parser.h"
#include "lexer.h"

// Parses the value represented by the input of `p`'s lexer and stores
// it in `*v`.
static json_Error
json_parse_value(json_Parser *p, json_Value *v);

static json_Token
token_none(void)
{
    static const json_Token token = {0};
    return token;
}

static json_Error
json_advance_token(json_Parser *p)
{
    p->consumed = p->lookahead;
    return json_scan_token(&p->lexer, &p->lookahead);
}

void
json_init_parser(json_Parser *p, const char *input, size_t len, mem_Allocator allocator)
{
    json_init_lexer(&p->lexer, input, len);
    p->consumed  = token_none();
    p->lookahead = token_none();
    p->alloc     = allocator;
    json_advance_token(p);
}

static bool
json_allow_token(json_Parser *p, json_Token_Type type)
{
    if (p->lookahead.type == type) {
        json_advance_token(p);
        return true;
    }
    return false;
}

static json_Error
json_expect_token(json_Parser *p, json_Token_Type type)
{
    json_Token consumed = p->lookahead;
    json_advance_token(p);
    return (consumed.type == type) ? JSON_OK : JSON_UNEXPECTED_TOKEN;
}

json_Error
json_parse(json_Parser *p, json_Value *v)
{
    json_Error err = json_parse_value(p, v);
    if (!err) {
        err = json_expect_token(p, TOKEN_EOF);
    }
    return err;
}

static json_Error
json_parse_number(json_Parser *p, json_Value *v)
{
    json_Error err = json_advance_token(p);
    if (!err) {
        json_Token consumed;
        double number;
        char *end;

        consumed = p->consumed;
        number   = strtod(consumed.text, &end);
        // Parsed the entire lexeme successfully, no more and no less?
        if (end == consumed.text + consumed.len) {
            json_init_number(v, number);
        } else {
            err = JSON_INVALID_NUMBER;
        }
    }
    return err;
}

extern uint16_t
json_decode_hex4(const char *s, size_t n, size_t *byte_count)
{
    size_t hex_count = 0;
    uint16_t curr_hex = 0, prev_hex = 0, byte2 = 0;
    if (n < 6 || s[0] != '\\' || s[1] != 'u') {
        return UINT16_MAX;
    }

    // JSON_LOG("INFO", "Decoding \"\\u");
    for (size_t i = 2; i < 6; i += 1) {
        // Try to track the first most significant non-zero hex digit.
        if (prev_hex == 0) {
            prev_hex = curr_hex;
        }

        curr_hex = json_char_to_hex(s[i]);

        // JSON_EPRINTF("%x", c);
        // If currently on a zero BUT some previous hex digit was nonzero,
        // then this zero is also significant.
        if (curr_hex > 0 || prev_hex > 0) {
            hex_count += 1;
        }

        byte2 <<= 4;
        byte2  |= curr_hex;
    }

    // JSON_EPRINTF("\"\n");
    *byte_count = hex_count / 2;
    return byte2;
}

extern uint16_t
json_char_to_hex(char c)
{
    if ('0' <= c && c <= '9') {
        return c - '0';
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    }
    return UINT16_MAX;
}

// Count actual number of bytes needed- i.e. with escaped sequences
static size_t
json_escape_lstring_len(const char *text, size_t text_len)
{
    size_t write_len = 0;
    char curr_char = 0, prev_char = 0;

    // JSON_LOGFLN("INFO", "Counting \"%.*s\"...", cast(int)text_len, text);
    for (const char *it = text, *end = it + text_len; it < end; it += 1) {
        prev_char = curr_char;
        curr_char = *it;

        // Currently in an escape sequence?
        if (prev_char == '\\') {
            // Don't count the 'u' of "\u".
            if (curr_char != 'u') {
                write_len += 1;
            } else {
                size_t byte_count;
                uint32_t hex4;

                // Point to the "\u" part so we can sanity check it.
                it -= 1;
                hex4 = json_decode_hex4(it, cast(size_t)(end - it), &byte_count);

                // We screwed up by not properly verifying the string
                // in the lexer phase.
                if (hex4 == UINT16_MAX) {
                    return 0;
                }

                // JSON_LOGFLN("INFO", "hex4=%#x, hex_count=%zu",
                //             hex4, byte_count);
                it += 5;

                write_len += byte_count;
            }
            prev_char = 0;
        }

        // Not the start of a new escape sequence?
        else if (curr_char != '\\') {
            write_len += 1;
        }
    }
    // JSON_LOGFLN("INFO", "write_len=%zu", write_len);
    return write_len;
}

static json_Error
json_parse_string_literal(json_Parser *p, json_Value *v)
{
    json_Error err;
    json_Token consumed;

    consumed = p->lookahead;
    err      = json_expect_token(p, TOKEN_STRING);
    if (!err) {
        json_String *s;
        const char *text;
        size_t text_len = 0, write_len = 0;

        // Skip both quotes
        text      = consumed.text + 1;
        text_len  = consumed.len - 2;
        write_len = json_escape_lstring_len(text, text_len);

        err = json_string_new(write_len, text, text_len, p->alloc, &s);
        if (!err) {
            json_init_string(v, s);
        }
    }
    return err;
}

static json_Error
json_parse_array(json_Parser *p, json_Value *v)
{
    static const json_Array EMPTY_ARRAY = {NULL, 0, 0};
    mem_Allocator alloc;
    json_Error err;
    json_Array *a;

    err = json_expect_token(p, TOKEN_BRACKET_OPEN);
    if (err) {
        return err;
    }

    json_init_array(v, EMPTY_ARRAY);
    a     = &v->array;
    alloc = p->alloc;
    if (p->lookahead.type != TOKEN_BRACKET_CLOSE) {
        do {
            json_Value elem;
            err = json_parse_value(p, &elem);
            if (err) {
                goto cleanup;
            }

            err = json_array_append(a, elem, alloc);
            if (err) {
                json_destroy_value(elem, alloc);
                goto cleanup;
            }
        } while (json_allow_token(p, TOKEN_COMMA));
    }

    err = json_expect_token(p, TOKEN_BRACKET_CLOSE);
    if (err) {
cleanup: json_destroy_value(*v, alloc);
    }
    return err;
}

static json_Error
json_parse_object(json_Parser *p, json_Value *v)
{
    static const json_Object EMPTY_OBJECT = {NULL, 0, 0};
    mem_Allocator alloc;
    json_Error err;
    json_Object *o;

    err = json_expect_token(p, TOKEN_CURLY_OPEN);
    if (err) {
        return err;
    }

    json_init_object(v, EMPTY_OBJECT);
    o     = &v->object;
    alloc = p->alloc;
    if (p->lookahead.type != TOKEN_CURLY_CLOSE) {
        do {
            // It's easy m'kay?
            json_Value mkey, mvalue;
            err = json_parse_string_literal(p, &mkey);
            if (err) {
                goto cleanup_object;
            }

            err = json_expect_token(p, TOKEN_COLON);
            if (err) {
                goto cleanup_key;
            }

            err = json_parse_value(p, &mvalue);
            if (err) {
                goto cleanup_value;
            }

            err = json_object_insert_jstring(o, mkey.string, mvalue, alloc);
            if (err) {
cleanup_value:  json_destroy_value(mvalue, alloc);
cleanup_key:    json_destroy_value(mkey, alloc);
                goto cleanup_object;
            }
        } while (json_allow_token(p, TOKEN_COMMA));
    }

    err = json_expect_token(p, TOKEN_CURLY_CLOSE);
    if (err) {
cleanup_object: json_destroy_value(*v, alloc);
    }
    return err;
}

static json_Error
json_parse_value(json_Parser *p, json_Value *v)
{
    switch (p->lookahead.type) {
    case TOKEN_NULL:
        *v = json_make_null();
        return json_advance_token(p);
    case TOKEN_FALSE:
        *v = json_make_boolean(false);
        return json_advance_token(p);
    case TOKEN_TRUE:
        *v = json_make_boolean(true);
        return json_advance_token(p);

    case TOKEN_NUMBER:       return json_parse_number(p, v);
    case TOKEN_STRING:       return json_parse_string_literal(p, v);
    case TOKEN_BRACKET_OPEN: return json_parse_array(p, v);
    case TOKEN_CURLY_OPEN:   return json_parse_object(p, v);
    default:
        break;
    }
    return JSON_UNEXPECTED_TOKEN;
}

