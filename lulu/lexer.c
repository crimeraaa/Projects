#include <stdlib.h> // strtod

#define ASCII_SET_DISABLED
#define ASCII_IMPLEMENTATION
#include "../ascii/ascii.h"

#define STRINGS_IMPLEMENTATION
#include "lexer.h"

internal const string_View
TOKEN_STRINGS[] = {
#define TOKEN_STRING(e, s) {s, sizeof(s) - 1}
    TOKEN_KINDS(TOKEN_STRING),
#undef TOKEN_STRING
};

global const char *
token_string(Token_Kind k)
{
    return TOKEN_STRINGS[k].data;
}

global Lexer
lexer_make(string_View input)
{
    Lexer x = {input,
        /*start =*/input.data, /*cursor =*/input.data,
        /*line  =*/1,          /*col    =*/1};
    return x;
}

internal bool
lexer_eof(const Lexer *x)
{
    // C standard says that we can only validly point to 1 past the last
    // element of the array, so technically we should only use `==`.
    // How much do we care about that?
    return x->cursor >= x->input.data + x->input.len;
}

internal char
lexer_peek_char(const Lexer *x)
{
    return *x->cursor;
}

internal char
lexer_peek_next_char(Lexer *x)
{
    if (x->cursor < x->input.data + x->input.len) {
        return x->cursor[1];
    }
    return 0;
}

// Returns the current character and advances the cursor.
internal char
lexer_next_char(Lexer *x)
{
    return *x->cursor++;
}

internal bool
lexer_check_char(const Lexer *x, char c)
{
    return lexer_peek_char(x) == c;
}

internal bool
lexer_match_char(Lexer *x, char c)
{
    bool found = lexer_check_char(x, c);
    if (found) {
        lexer_next_char(x);
    }
    return found;
}

internal bool
lexer_match_either_char(Lexer *x, char c1, char c2)
{
    return lexer_match_char(x, c1) || lexer_match_char(x, c2);
}

internal void
lexer_skip_whitespace(Lexer *x)
{
    for (;;) {
        char c = lexer_peek_char(x);
        switch (c) {
        case '\n':
            x->line++;
            x->col = 0; // Will be set to 1 on next advance.
        case '\r':
        case '\t':
        case ' ':
            lexer_next_char(x);
            break;
        case '-':
            if (lexer_peek_next_char(x) == '-') {
                // Skip "--".
                lexer_next_char(x);
                lexer_next_char(x);
                while (!lexer_eof(x) && lexer_peek_char(x) != '\n') {
                    lexer_next_char(x);
                }
                continue;
            }
            return;
        default:
            return;
        }
    }
}

internal inline string_View
lexer_get_lexeme(const Lexer *x)
{
    size_t n = cast(size_t)(x->cursor - x->start);
    return string_make(x->start, n);
}

// Wrapper function. Call this manually only for multiline strings.
// Does not initalize `.number`.
internal void
lexer_init_token_fields(Token *t,
    Token_Kind k,
    string_View lexeme,
    i32 line, i32 col)
{
    t->kind     = k;
    t->lexeme   = lexeme;
    t->line     = line;
    t->col      = col;
}

// Does not initalize `.number`.
internal void
lexer_init_token(const Lexer *x, Token *t, Token_Kind k)
{
    string_View s;
    i32 line, col;

    s = lexer_get_lexeme(x);
    if (s.len == 0) {
        s = TOKEN_STRINGS[k];
    }
    line = x->line;
    col  = cast(i32)(cast(size_t)x->col - s.len);
    lexer_init_token_fields(t, k, s, line, col);
}

// Keep advancing while the character pointed to by the cursor
// matches the predicate.
internal inline int
lexer_consume_fn(Lexer *x, bool (*fn)(char c))
{
    int n;
    for (n = 0; !lexer_eof(x); n++) {
        char c = lexer_peek_char(x);
        if (!fn(c)) {
            break;
        }
        lexer_next_char(x);
    }
    return n;
}

internal inline int
lexer_consume_decimal(Lexer *x)
{
    return lexer_consume_fn(x, ascii_is_decimal);
}

internal inline int
lexer_consume_alnum(Lexer *x)
{
    return lexer_consume_fn(x, ascii_is_alnum);
}


internal inline bool
lexer_ascii_is_ident(char c)
{
    return c == '_' || ascii_is_alnum(c);
}

internal inline Token_Kind
lexer_get_keyword(string_View s, Token_Kind k)
{
    return string_eq(s, TOKEN_STRINGS[k]) ? k : TOKEN_IDENT;
}

internal lulu_Error
lexer_scan_keyword(Lexer *x, Token *t)
{
    string_View s;
    Token_Kind k = TOKEN_IDENT;
    lexer_consume_fn(x, lexer_ascii_is_ident);
    s = lexer_get_lexeme(x);
    // len("do") <= n <= len("function")
    if (2 <= s.len && s.len <= 8) {
        switch (s.data[0]) {
        case 'a': k = lexer_get_keyword(s, TOKEN_AND);   break;
        case 'b': k = lexer_get_keyword(s, TOKEN_BREAK); break;
        case 'd': k = lexer_get_keyword(s, TOKEN_DO);    break;
        case 'e':
            switch (s.len) {
            case 3: k = lexer_get_keyword(s, TOKEN_END);    break;
            case 4: k = lexer_get_keyword(s, TOKEN_ELSE);   break;
            case 6: k = lexer_get_keyword(s, TOKEN_ELSEIF); break;
            }
            break;
        case 'f':
            switch (s.len) {
            case 3: k = lexer_get_keyword(s, TOKEN_FOR);      break;
            case 5: k = lexer_get_keyword(s, TOKEN_FALSE);    break;
            case 8: k = lexer_get_keyword(s, TOKEN_FUNCTION); break;
            }
            break;
        case 'i':
            switch (s.data[1]) {
            case 'f': k = lexer_get_keyword(s, TOKEN_IF); break;
            case 'n': k = lexer_get_keyword(s, TOKEN_IN); break;
            }
            break;
        case 'l': k = lexer_get_keyword(s, TOKEN_LOCAL); break;
        case 'n':
            switch (s.data[1]) {
            case 'i': k = lexer_get_keyword(s, TOKEN_NIL); break;
            case 'o': k = lexer_get_keyword(s, TOKEN_NOT); break;
            }
            break;
        case 'o': k = lexer_get_keyword(s, TOKEN_OR); break;
        case 'r':
            if (s.len != 6 || s.data[1] != 'e') {
                break;
            }
            switch (s.data[2]) {
            case 'p': k = lexer_get_keyword(s, TOKEN_REPEAT); break;
            case 't': k = lexer_get_keyword(s, TOKEN_RETURN); break;
            }
            break;
        case 't':
            switch (s.data[1]) {
            case 'h': k = lexer_get_keyword(s, TOKEN_THEN); break;
            case 'r': k = lexer_get_keyword(s, TOKEN_TRUE); break;
            }
            break;
        case 'u': k = lexer_get_keyword(s, TOKEN_UNTIL); break;
        case 'w': k = lexer_get_keyword(s, TOKEN_WHILE); break;
        default:
            break;
        }
    }
    lexer_init_token(x, t, k);
    return LULU_OK;
}

internal u64
lexer_ascii_to_digit(char c, u64 base, bool *ok)
{
    u64 digit = 0;
    if (ascii_is_decimal(c)) {
        digit = cast(u64)(c - '0');
    } else if (ascii_is_hexadecimal(c)) {
        char a = ascii_is_upper(c) ? 'A' : 'a';
        digit  = cast(u64)(c - a + 0xA);
    } else {
        // We should never have spaces in this function.
        // Rather, we can only encounter invalid base-`base` digits.
        *ok = false;
        return 0;
    }

    *ok = (digit < base);
    return digit;
}

/**
 * @note We assume that we only ever receive positive integers, because
 *       unary negation on literals only occurs during constant folding
 *       (if we even have it).
 */
internal u64
lexer_scan_u64(const Lexer *x, bool *ok)
{
    string_View s;
    u64 u = 0, base = 0;

    *ok = true;
    s   = lexer_get_lexeme(x);
    if (s.len > 2 && s.data[0] == '0') {
        switch (s.data[1]) {
        case 'b': case 'B': base = 2;  break;
        case 'o': case 'O': base = 8;  break;
        case 'd': case 'D': base = 10; break;
        case 'z': case 'Z': base = 12; break;
        case 'x': case 'X': base = 16; break;
        }
    }

    if (base == 0) {
        base = 10;
    } else {
        // Trim the integer prefix. We know the length is >2, so we
        // have something to parse.
        s = string_slice_from(s, 2);
    }

    // Work from the most significant to least significant digits.
    for (size_t i = 0; i < s.len; i++) {
        char c = s.data[i];
        u64 digit = lexer_ascii_to_digit(c, base, ok);
        if (!*ok) {
            break;
        }
        u *= base;
        u += digit;
    }

    // TODO: Check limits? `f64` can accurately represent all real numbers
    //       in the range [-(1 << #mantissa), 1 << #mantissa]. Beyond that,
    //       precision is lost, i.e. any real numbers in the range
    //       (1 << #mantissa, inf] may skip over values. The higher up we go,
    //       the more values are skipped due to imprecision.
    return u;
}

#define FLAG_FRAC   (1 << 0)
#define FLAG_EXP    (1 << 1)
#define FLAG_SIGN   (1 << 2)
#define FLAG_FLOAT  (FLAG_FRAC | FLAG_EXP)

internal f64
lexer_scan_f64(const Lexer *x, u8 flags, bool *ok)
{
    string_View s;
    char *p;
    f64 d;
    unused(flags);

    // TODO: Implement our own `strtod()`.
    s   = lexer_get_lexeme(x);
    d   = strtod(s.data, &p);
    *ok = (p == s.data + s.len);
    return d;
}

internal lulu_Error
lexer_scan_number(Lexer *x, Token *t)
{
    Token_Kind k;
    u8 flags = 0;
    bool ok;

    lexer_consume_decimal(x);
    if (lexer_match_char(x, '.')) {
        flags |= FLAG_FRAC;
        lexer_consume_decimal(x);
    }

    // This should only work for base-10, but we'll check later.
    if (lexer_match_either_char(x, 'e', 'E')) {
        flags |= FLAG_EXP;
        if (lexer_match_either_char(x, '+', '-')) {
            flags |= FLAG_SIGN;
        }
        lexer_consume_decimal(x);
    }

    lexer_consume_alnum(x);
    if (flags & FLAG_FLOAT) {
        t->number.f = lexer_scan_f64(x, flags, &ok);
        k = TOKEN_FLOAT;
    } else {
        t->number.u = lexer_scan_u64(x, &ok);
        k = TOKEN_INT;
    }
    lexer_init_token(x, t, k);
    return ok ? LULU_OK : LULU_INVALID_NUMBER;
}

#undef FLAG_SIGN
#undef FLAG_EXP
#undef FLAG_FRAC

internal lulu_Error
lexer_scan_string(Lexer *x, Token *t, char quote)
{
    bool ok = false;
    while(!lexer_eof(x)) {
        char c = lexer_next_char(x);
        if (c == quote) {
            ok = true;
            break;
        } else if (c == '\n') {
            break;
        }
    }
    lexer_init_token(x, t, TOKEN_STRING);
    return ok ? LULU_OK : LULU_UNTERMINATED_STRING;
}

global lulu_Error
lexer_scan_token(Lexer *x, Token *t)
{
    char c;
    Token_Kind k;
    lexer_skip_whitespace(x);
    if (lexer_eof(x)) {
        lexer_init_token(x, t, TOKEN_EOF);
        return LULU_EOF;
    }

    x->start = x->cursor;
    c = lexer_next_char(x);
    k = TOKEN_NONE;
    if (c == '_' || ascii_is_letter(c)) {
        return lexer_scan_keyword(x, t);
    } else if (ascii_is_decimal(c)) {
        return lexer_scan_number(x, t);
    }

    switch (c) {
    case '=': k = lexer_match_char(x, '=') ? TOKEN_EQ : TOKEN_ASSIGN; break;
    case '+': k = TOKEN_ADD; break;
    case '-': k = TOKEN_SUB; break;
    case '*': k = TOKEN_MUL; break;
    case '/': k = TOKEN_DIV; break;
    case '%': k = TOKEN_MOD; break;
    case '^': k = TOKEN_POW; break;
    case '~': k = lexer_match_char(x, '=') ? TOKEN_NEQ : TOKEN_NONE; break;
    case '<': k = lexer_match_char(x, '=') ? TOKEN_LEQ : TOKEN_LT;   break;
    case '>': k = lexer_match_char(x, '=') ? TOKEN_GEQ : TOKEN_GT;   break;
    case '(': k = TOKEN_OPEN_PAREN;   break;
    case ')': k = TOKEN_CLOSE_PAREN;  break;
    case '{': k = TOKEN_OPEN_CURLY;   break;
    case '}': k = TOKEN_CLOSE_CURLY;  break;
    // TODO: multiline string
    case '[': k = TOKEN_OPEN_BRACKET; break;
    case ']': k = TOKEN_CLOSE_CURLY;  break;
    case ':': k = TOKEN_COLON;        break;
    case ';': k = TOKEN_SEMICOL;      break;
    case ',': k = TOKEN_COMMA;        break;
    case '.':
        if (lexer_match_char(x, '.')) {
            // Have '..', try to match '...'.
            k = lexer_match_char(x, '.') ? TOKEN_VARARG : TOKEN_CONCAT;
            break;
        }

        // Don't have '..' but it could be a fractional literal.
        c = lexer_peek_next_char(x);
        if (ascii_is_decimal(c)) {
            return lexer_scan_number(x, t);
        }
        k = TOKEN_PERIOD;
        break;
    case '\'':
    case '\"':
        k = TOKEN_STRING;
        break;
    default:
        break;
    }
    lexer_init_token(x, t, k);
    return (k == TOKEN_NONE) ? LULU_UNEXPECTED_CHARACTER : LULU_OK;
}

