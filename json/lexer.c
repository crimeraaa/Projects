// standard
#include <string.h> // memcmp

// local
#define ASCII_SET_DISABLED
#define ASCII_IMPLEMENTATION
#include "../strings/ascii.h"
#include "lexer.h"
#include "json.h"

global void
json_init_lexer(json_Lexer *x, const char *input, size_t len)
{
    x->start   = input;
    x->current = input;
    x->input   = input;
    x->len     = len;
    x->line    = 1;
    x->col     = 1;
}

internal bool
json_is_at_end(const json_Lexer *x)
{
    return x->current >= x->input + x->len;
}

internal char
json_peek_char(const json_Lexer *x)
{
    return x->current[0];
}

// internal char
// json_peek_next_char(const Lexer *x)
// {
//     return json_is_at_end(x) ? '\0' : x->current[1];
// }

internal char
json_advance_char(json_Lexer *x)
{
    char c = json_peek_char(x);
    x->current += 1;
    x->col     += 1;
    // JSON_LOGFLN("INFO ", "json:%i:%i: '%c'", x->line, x->col, c);
    return c;
}

internal bool
json_check_char(const json_Lexer *x, char wanted)
{
    char c = json_peek_char(x);
    return c == wanted;
}

internal bool
json_match_char(json_Lexer *x, char wanted)
{
    bool ok = json_check_char(x, wanted);
    if (ok) {
        json_advance_char(x);
    }
    return ok;
}

internal bool
json_match_either_char(json_Lexer *x, char a, char b)
{
    return json_match_char(x, a) || json_match_char(x, b);
}

internal void
skip_whitespace(json_Lexer *x)
{
    for (;;) {
        char c = json_peek_char(x);
        switch (c) {
        case '\n':
            x->line += 1;
            x->col = 0; // Will be immediately set to 1 on advance.
        case '\r':
        case '\t':
        case '\v':
        case ' ':
            json_advance_char(x);
            break;
        default:
            return;
        }
    }
}

const char *
json_get_lexeme(const json_Lexer *x, size_t *len)
{
    *len = cast(size_t)(x->current - x->start);
    return x->start;
}

internal json_Error
json_init_token(const json_Lexer *x, json_Token *t, json_Token_Type type)
{
    t->type = type;
    t->text = json_get_lexeme(x, &t->len);
    t->line = x->line;
    t->col  = x->col - cast(int)t->len;
    // JSON_LOGFLN("INFO ", ".json:%i:%i Token(%02i) : \"%.*s\"",
    //             t->line, t->col, t->type, cast(int)t->len, t->text);
    return JSON_OK;
}

internal int
json_consume_sequence(json_Lexer *x, bool (*predicate)(char c))
{
    int n = 0;
    while (!json_is_at_end(x)) {
        char c = json_peek_char(x);
        if (!predicate(c)) {
            break;
        }
        n += 1;
        json_advance_char(x);
    }
    return n;
}

internal json_Error
json_try_keyword(json_Lexer *x, json_Token *t)
{
    size_t len;
    const char *text = json_get_lexeme(x, &len);

    // Length is in the range of all keywords?
    if (4 <= len && len <= 5) {
        switch (text[0]) {
        case 'f':
            if (len == 5 && memcmp(&text[1], "alse", 4) == 0) {
                return json_init_token(x, t, TOKEN_FALSE);
            }
        case 'n':
            if (len == 4 && memcmp(&text[1], "ull", 3) == 0) {
                return json_init_token(x, t, TOKEN_NULL);
            }
        case 't':
            if (len == 4 && memcmp(&text[1], "rue", 3) == 0) {
                return json_init_token(x, t, TOKEN_TRUE);
            }
        default:
            break;
        }
    }
    // Identifiers aren't supported in strict JSON.
    json_init_token(x, t, TOKEN_INVALID);
    return JSON_UNEXPECTED_TOKEN;
}

internal json_Error
json_consume_digits(json_Lexer *x)
{
    bool leading_zero;
    int digit_count;

    leading_zero = json_match_char(x, '0');
    digit_count  = json_consume_sequence(x, ascii_is_decimal);

    // Leading zero is followed by some number of other digits?
    // This is to disallow octal numbers as in JavaScript.
    if (leading_zero && digit_count > 0) {
        return JSON_INVALID_NUMBER;
    }
    return JSON_OK;
}

internal json_Error
json_try_number(json_Lexer *x, json_Token *t)
{
    json_Error err;

    // Optional sign, negative only.
    json_match_char(x, '-');

    // Integer portion
    err = json_consume_digits(x);
    if (err) {
        return err;
    }

    // Fractional portion
    if (json_match_char(x, '.')) {
        err = json_consume_digits(x);
        if (err) {
            return err;
        }
    }

    // Exponent portion
    if (json_match_either_char(x, 'e', 'E')) {
        // Optional sign after exponent character
        json_match_either_char(x, '+', '-');
        err = json_consume_digits(x);
        if (err) {
            return err;
        }
    }

    // Trailing characters were found?
    if (json_consume_sequence(x, ascii_is_alnum) > 0) {
        return JSON_INVALID_NUMBER;
    }
    return json_init_token(x, t, TOKEN_NUMBER);
}

internal json_Error
json_validate_string(json_Lexer *x)
{
    // JSON_LOGLN("INFO ", "Trying a string...");
    while (!json_is_at_end(x) && !json_check_char(x, '"')) {
        char c = json_advance_char(x);
        if (c != '\\') {
            continue;
        }

        // Is an escape sequence, so check the character that's being escaped.
        c = json_peek_char(x);
        switch (c) {
        case '"': case '\\': case '/':
        case 'b': case 'f': case 'n': case 'r': case 't':
            // '\e' nor \v' are in the JSON spec.
            json_advance_char(x);
            break;

        // unicode ::= "\u" hex hex hex hex
        // hex     ::= [0-9a-fA-F]
        case 'u':
            // JSON_LOGLN("INFO ", "Trying a hex4...");
            // Skip 'u'
            json_advance_char(x);
            if (json_consume_sequence(x, ascii_is_hexadecimal) != 4) {
                return JSON_INVALID_STRING;
            }
            break;
        default:
            break;
        }
    }

    if (!json_match_char(x, '"')) {
        return JSON_UNTERMINATED_STRING;
    }

    return JSON_OK;
}

internal json_Error
json_try_string(json_Lexer *x, json_Token *t)
{
    json_Error err = json_validate_string(x);
    if (!err) {
        return json_init_token(x, t, TOKEN_STRING);
    }
    return err;
}

global json_Error
json_scan_token(json_Lexer *x, json_Token *t)
{
    char curr;
    skip_whitespace(x);
    if (json_is_at_end(x)) {
        json_init_token(x, t, TOKEN_EOF);
        return JSON_OK;
    }

    x->start = x->current;
    curr  = json_advance_char(x);
    // Maybe a keyword?
    if (ascii_is_letter(curr)) {
        json_consume_sequence(x, ascii_is_letter);
        return json_try_keyword(x, t);
    } else if (ascii_is_decimal(curr) || curr == '.') {
        return json_try_number(x, t);
    }

    switch (curr) {
    case '{': return json_init_token(x, t, TOKEN_CURLY_OPEN);
    case '}': return json_init_token(x, t, TOKEN_CURLY_CLOSE);
    case '[': return json_init_token(x, t, TOKEN_BRACKET_OPEN);
    case ']': return json_init_token(x, t, TOKEN_BRACKET_CLOSE);
    case ',': return json_init_token(x, t, TOKEN_COMMA);
    case ':': return json_init_token(x, t, TOKEN_COLON);
    case '"': return json_try_string(x, t);
    default:
          break;
    }
    json_init_token(x, t, TOKEN_INVALID);
    return JSON_UNEXPECTED_TOKEN;
}
