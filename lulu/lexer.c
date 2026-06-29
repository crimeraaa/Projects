#include <stdlib.h> // strtod

#include "lexer.h"

static const char *
token_string(Token_Kind k, size_t *n)
{
    static const struct {
        const char *data;
        size_t len;
    } TOKEN_STRINGS[] = {
#define TOKEN_STRING(e, s) {s, sizeof(s) - 1}
        TOKEN_KINDS(TOKEN_STRING),
#undef TOKEN_STRING
    };

    *n = TOKEN_STRINGS[k].len;
    return TOKEN_STRINGS[k].data;
}

LULU_INTERNAL_FUNC const char *
token_cstring(Token_Kind k)
{
    // Avoid NULL dereference.
    size_t n;
    return token_string(k, &n);
}

LULU_INTERNAL_FUNC Lexer
lexer_make(const char *name, const char *input, size_t len)
{
    Lexer x = {name, input, len,
        /*start =*/0, /*cursor =*/0,
        /*line  =*/1, /*col    =*/1};
    return x;
}

static bool
lexer_eof(const Lexer *x)
{
    return x->cursor >= x->len;
}

static char
lexer_peek_char(const Lexer *x)
{
    return x->input[x->cursor];
}

static char
lexer_peek_next_char(Lexer *x)
{
    if (x->cursor < x->len) {
        return x->input[x->cursor + 1];
    }
    return 0;
}

// Returns the current character and advances the cursor.
static char
lexer_next_char(Lexer *x)
{
    return x->input[x->cursor++];
}

static bool
lexer_check_char(const Lexer *x, char c)
{
    return lexer_peek_char(x) == c;
}

static bool
lexer_match_char(Lexer *x, char c)
{
    bool found = lexer_check_char(x, c);
    if (found) {
        lexer_next_char(x);
    }
    return found;
}

static bool
lexer_match_either_char(Lexer *x, char c1, char c2)
{
    return lexer_match_char(x, c1) || lexer_match_char(x, c2);
}

static const char *
lexer_get_lexeme(const Lexer *x, size_t *len)
{
    *len = cast(size_t)(x->cursor - x->start);
    return &x->input[x->start];
}

// Wrapper function. Call this manually only for multiline strings.
// Does not initalize the value literal.
static Token
token_make(Token_Kind k, const char *lexeme, size_t len, i32 line, i32 col)
{
    Token t;
    t.kind   = k;
    t.lexeme = lexeme;
    t.len    = len;
    t.line   = line;
    t.col    = col;
    return t;
}

// Initalizes the given token with the current lexeme.
// Does not initalize the value literal.
static void
lexer_init_token(const Lexer *x, Token *t, Token_Kind k)
{
    const char *s;
    size_t n;
    i32 line, col;

    s = lexer_get_lexeme(x, &n);
    if (n == 0) {
        s = token_string(k, &n);
    }

    line = x->line;
    col  = cast(i32)(cast(size_t)x->col - n);
    *t   = token_make(k, s, n, line, col);
}

// Keep advancing while the character pointed to by the cursor
// matches the predicate.
static int
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

static bool
ascii_is_decimal(char c)
{
    // ASCII ordering is not guaranteed by the C standard,
    // but <ctypes.h> is locale-dependent so we don't want
    // to use it.
    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return true;
    }
    return false;
}

static bool
ascii_is_lower(char c)
{
    switch (c) {
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z':
        return true;
    }
    return false;
}

static bool
ascii_is_upper(char c)
{
    switch (c) {
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
	case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
	case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
	case 'Y': case 'Z':
        return true;
    }
    return false;
}

static bool
ascii_is_letter(char c)
{
    return c == '_' || ascii_is_lower(c) || ascii_is_upper(c);
}

static bool
ascii_is_alnum(char c)
{
    return ascii_is_decimal(c) || ascii_is_letter(c);
}

static void
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

                // Don't consume LF, we want to handle it in the switch.
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

// Since MSVC is absolutely braindead, they pass 16-byte structs on the stack
// rather than in registers. So we have to manually manage the string views...
static Token_Kind
lexer_get_keyword_kind(const char *lexeme, size_t lexeme_len,
    Token_Kind keyword_kind,
    size_t offset)
{
    const char *keyword;
    size_t keyword_len;

    keyword = token_string(keyword_kind, &keyword_len);
    if (lexeme_len != keyword_len) {
        return TOKEN_IDENT;
    }

    for (size_t i = offset; i < lexeme_len; i++) {
        if (lexeme[i] != keyword[i]) {
            return TOKEN_IDENT;
        }
    }
    return keyword_kind;
}

static Lexer_Error
lexer_scan_keyword_or_ident(Lexer *x, Token *t)
{
    const char *s;
    size_t n;
    Token_Kind k = TOKEN_IDENT;

    lexer_consume_fn(x, ascii_is_alnum);
    s = lexer_get_lexeme(x, &n);
    // len("do") <= n <= len("function")
    if (2 <= n && n <= 8) {
        switch (s[0]) {
        case 'a': k = lexer_get_keyword_kind(s, n, TOKEN_AND,   1); break;
        case 'b': k = lexer_get_keyword_kind(s, n, TOKEN_BREAK, 1); break;
        case 'd': k = lexer_get_keyword_kind(s, n, TOKEN_DO,    1); break;
        case 'e':
            switch (n) {
            case 3: k = lexer_get_keyword_kind(s, n, TOKEN_END,    1); break;
            case 4: k = lexer_get_keyword_kind(s, n, TOKEN_ELSE,   1); break;
            case 6: k = lexer_get_keyword_kind(s, n, TOKEN_ELSEIF, 1); break;
            }
            break;
        case 'f':
            switch (s[1]) {
            case 'a': k = lexer_get_keyword_kind(s, n, TOKEN_FALSE,    2); break;
            case 'o': k = lexer_get_keyword_kind(s, n, TOKEN_FOR,      2); break;
            case 'u': k = lexer_get_keyword_kind(s, n, TOKEN_FUNCTION, 2); break;
            }
            break;
        case 'i':
            switch (s[1]) {
            case 'f': k = lexer_get_keyword_kind(s, n, TOKEN_IF, 2); break;
            case 'n': k = lexer_get_keyword_kind(s, n, TOKEN_IN, 2); break;
            }
            break;
        case 'l': k = lexer_get_keyword_kind(s, n, TOKEN_LOCAL, 1); break;
        case 'n':
            switch (s[1]) {
            case 'i': k = lexer_get_keyword_kind(s, n, TOKEN_NIL, 2); break;
            case 'o': k = lexer_get_keyword_kind(s, n, TOKEN_NOT, 2); break;
            }
            break;
        case 'o': k = lexer_get_keyword_kind(s, n, TOKEN_OR, 1); break;
        case 'r':
            if (n != 6 || s[1] != 'e') {
                break;
            }
            switch (s[2]) {
            case 'p': k = lexer_get_keyword_kind(s, n, TOKEN_REPEAT, 2); break;
            case 't': k = lexer_get_keyword_kind(s, n, TOKEN_RETURN, 2); break;
            }
            break;
        case 't':
            switch (s[1]) {
            case 'h': k = lexer_get_keyword_kind(s, n, TOKEN_THEN, 2); break;
            case 'r': k = lexer_get_keyword_kind(s, n, TOKEN_TRUE, 2); break;
            }
            break;
        case 'u': k = lexer_get_keyword_kind(s, n, TOKEN_UNTIL, 1); break;
        case 'w': k = lexer_get_keyword_kind(s, n, TOKEN_WHILE, 1); break;
        default:
            break;
        }
    }
    lexer_init_token(x, t, k);
    return LEXER_OK;
}

static bool
ascii_to_digit(char c, int base, int *digit)
{
    if (ascii_is_decimal(c)) {
        *digit = c - '0';
    } else if (ascii_is_lower(c)) {
        *digit = c - 'a' + 0xa;
    } else if (ascii_is_upper(c)) {
        *digit = c - 'A' + 0xA;
    } else {
        // We should never have spaces in this function.
        // Rather, we can only encounter invalid base-`base` digits.
        return false;
    }
    return *digit < base;
}

/**
 * @note We assume that we only ever receive positive integers, because
 *       unary negation on literals only occurs during constant folding
 *       (if we even have it).
 */
static bool
lexer_scan_u64(const Lexer *x, u64 *u)
{
    const char *s;
    size_t n;
    int base = 0;

    *u = 0;
    s  = lexer_get_lexeme(x, &n);
    if (n > 2 && s[0] == '0') {
        switch (s[1]) {
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
        s += 2;
        n -= 2;
    }

    // Work from the most significant to least significant digits.
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        int digit;
        if (!ascii_to_digit(c, base, &digit)) {
            return false;
        }
        *u *= cast(u64)base;
        *u += cast(u64)digit;
    }

    // TODO: Check limits? `f64` can accurately represent all real numbers
    //       in the range [-(1 << #mantissa), 1 << #mantissa]. Beyond that,
    //       precision is lost, i.e. any real numbers in the range
    //       (1 << #mantissa, inf] may skip over values. The higher up we go,
    //       the more values are skipped due to imprecision.
    return true;
}

#define FLAG_FRAC   (1 << 0)
#define FLAG_EXP    (1 << 1)
#define FLAG_SIGN   (1 << 2)
#define FLAG_FLOAT  (FLAG_FRAC | FLAG_EXP)

static bool
lexer_scan_f64(const Lexer *x, u8 flags, f64 *f)
{
    const char *s;
    size_t n;
    char *p;
    unused(flags);

    // TODO: Implement our own `strtod()`.
    *f = 0.0;
    s  = lexer_get_lexeme(x, &n);
    *f = strtod(s, &p);
    return (p == s + n);
}

static Lexer_Error
lexer_scan_number(Lexer *x, Token *t)
{
    u8 flags = 0;
    bool ok;

    lexer_consume_fn(x, ascii_is_decimal);
    if (lexer_match_char(x, '.')) {
        flags |= FLAG_FRAC;
        lexer_consume_fn(x, ascii_is_decimal);
    }

    // This should only work for base-10, but we'll check later.
    if (lexer_match_either_char(x, 'e', 'E')) {
        flags |= FLAG_EXP;
        if (lexer_match_either_char(x, '+', '-')) {
            flags |= FLAG_SIGN;
        }
        lexer_consume_fn(x, ascii_is_decimal);
    }

    lexer_consume_fn(x, ascii_is_alnum);
    lexer_init_token(x, t, TOKEN_NONE);
    if (flags & FLAG_FLOAT) {
        ok      = lexer_scan_f64(x, flags, &t->value.f);
        t->kind = TOKEN_FLOAT;
    } else {
        ok      = lexer_scan_u64(x, &t->value.u);
        t->kind = TOKEN_INT;
    }
    return ok ? LEXER_OK : LEXER_INVALID_NUMBER;
}

#undef FLAG_SIGN
#undef FLAG_EXP
#undef FLAG_FRAC

static Lexer_Error
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
    if (!ok) {
        return LEXER_UNTERMINATED_STRING;
    }

    // Skip the quotes.
    t->lexeme++;
    t->len -= 2;
    return LEXER_OK;
}

LULU_INTERNAL_FUNC Lexer_Error
lexer_scan_token(Lexer *x, Token *t)
{
    char c;
    Token_Kind k;
    lexer_skip_whitespace(x);
    if (lexer_eof(x)) {
        lexer_init_token(x, t, TOKEN_EOF);
        return LEXER_OK;
    }

    x->start = x->cursor;
    c = lexer_next_char(x);
    k = TOKEN_NONE;

    /* TODO(2025-06-29)
        If we're assuming ASCII only (which is a dangerous assumptiuon!) then
        can we just make use of the bitsets? Is that more efficient, or is it
        a meaningless optimization?
     */
    if (ascii_is_letter(c)) {
        return lexer_scan_keyword_or_ident(x, t);
    } else if (ascii_is_decimal(c)) {
        return lexer_scan_number(x, t);
    }

    switch (c) {
    case '=': k = lexer_match_char(x, '=') ? TOKEN_EQ : TOKEN_ASSIGN; break;
    case '+': k = TOKEN_ADD; break;
    case '-': k = lexer_match_char(x, '>') ? TOKEN_ARROW : TOKEN_SUB; break;
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
    case '\"': return lexer_scan_string(x, t, c);
    default:
        break;
    }
    lexer_init_token(x, t, k);
    return (k == TOKEN_NONE) ? LEXER_UNEXPECTED_CHARACTER : LEXER_OK;
}

