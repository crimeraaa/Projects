// standard
#include <string.h> // memcmp

// local
#define ASCII_SET_DISABLED
#include "../strings/ascii.c"
#include "lexer.h"

void
lexer_init(struct Lexer *x, const char *s, size_t n)
{
    x->text     = s;
    x->len      = n;
    x->start    = s;
    x->current  = s;
    x->line     = 1;
    x->col      = 1;
}

static const char *
lexer_get_lexeme(const struct Lexer *x, size_t *n)
{
    *n = cast(size_t)(x->current - x->start);
    return x->start;
}

static char
lexer_peek_char(const struct Lexer *x)
{
    return x->current[0];
}

static char
lexer_next_char(struct Lexer *x)
{
    x->col++;
    return *x->current++;
}

static bool
lexer_check_char(const struct Lexer *x, char c)
{
    return lexer_peek_char(x) == c;
}

static bool
lexer_allow_char(struct Lexer *x, char c)
{
    if (lexer_check_char(x, c)) {
        lexer_next_char(x);
        return true;
    }
    return false;
}

static bool
lexer_is_at_end(const struct Lexer *x)
{
    return x->current >= x->text + x->len;
}

static void
lexer_skip_whitespace(struct Lexer *x)
{
    for (;;) {
        char c = lexer_peek_char(x);
        switch (c) {
        case '\n':
            x->line++;
            x->col = 0; // Will be set to 1 on next advance
        case '\r':
        case '\t':
        case ' ':
            lexer_next_char(x);
            continue;
        default:
            return;
        }
    }
}

static enum Type_Error
lexer_init_token(struct Lexer *x, struct Token *t, enum Token_Kind k)
{
    t->kind = k;
    t->text = lexer_get_lexeme(x, &t->len);
    t->line = x->line;
    t->col  = cast(int)(cast(size_t)x->col - t->len);
    return TYPE_ERROR_NONE;
}

static int
lexer_consume_sequence(struct Lexer *x, bool (*predicate)(char c))
{
    int count = 0;
    while (!lexer_is_at_end(x)) {
        char c = lexer_peek_char(x);
        if (!predicate(c)) {
            break;
        }
        lexer_next_char(x);
        count++;
    }
    return count;
}

struct Keyword {
    // least: `int`
    // most:  `unsigned` | `volatile`
    char text[8];
    uint8_t len, offset;
};

static enum Type_Error
lexer_check_keyword(struct Token *t, struct Keyword kw, enum Token_Kind kind)
{
    if (t->len == cast(size_t)kw.len) {
        // Actual number of bytes to compare
        size_t n = cast(size_t)(kw.len - kw.offset);
        if (memcmp(&t->text[kw.offset], &kw.text[kw.offset], n) == 0) {
            t->kind = kind;
        }
    }
    return TYPE_ERROR_NONE;
}

#define K(s, offset)   (struct Keyword){s, cast(uint8_t)(sizeof(s) - 1), offset}

static enum Type_Error
lexer_try_keyword(struct Lexer *x, struct Token *t)
{
    lexer_init_token(x, t, TOKEN_IDENTIFIER);
    // length of `int` up to and including `unsigned` | `volatile`
    if (3 <= t->len && t->len <= 8) {
        switch (t->text[0]) {
        case 'b': return lexer_check_keyword(t, K("bool", 1), TOKEN_BOOL);
        case 'c':
            switch (t->text[1]) {
            case 'h': lexer_check_keyword(t, K("char",  2), TOKEN_CHAR);
            case 'o': lexer_check_keyword(t, K("const", 2), TOKEN_CONST);
            }
            break;
        case 'd': return lexer_check_keyword(t, K("double", 1), TOKEN_DOUBLE);
        case 'e': return lexer_check_keyword(t, K("enum",   1), TOKEN_ENUM);
        case 'f':
            switch (t->text[1]) {
            case 'a': return lexer_check_keyword(t, K("false", 2), TOKEN_FALSE);
            case 'l': return lexer_check_keyword(t, K("float", 2), TOKEN_FLOAT);
            }
            break;
        case 'i': return lexer_check_keyword(t, K("int",  1), TOKEN_INT);
        case 'l': return lexer_check_keyword(t, K("long", 1), TOKEN_LONG);
        case 's':
            switch (t->text[1]) {
            case 'h': return lexer_check_keyword(t, K("short",  2), TOKEN_SHORT);
            case 'i': return lexer_check_keyword(t, K("signed", 2), TOKEN_SIGNED);
            case 't': return lexer_check_keyword(t, K("struct", 2), TOKEN_STRUCT);
            }
            break;
        case 't': return lexer_check_keyword(t, K("true", 1), TOKEN_TRUE);
        case 'v':
            if (t->text[1] != 'o') {
                break;
            }
            switch (t->text[2]) {
            case 'i': return lexer_check_keyword(t, K("void",     2), TOKEN_VOID);
            case 'l': return lexer_check_keyword(t, K("volatile", 2), TOKEN_VOLATILE);
            }
            break;
        case 'u':
            if (t->text[1] != 'n') {
                break;
            }
            switch (t->text[2]) {
            case 'i': return lexer_check_keyword(t, K("union",    3), TOKEN_UNION);
            case 's': return lexer_check_keyword(t, K("unsigned", 3), TOKEN_UNSIGNED);
            }
            break;
        default:
            break;
        }
    }
    return TYPE_ERROR_NONE;
}

#undef K

static bool
char_is_ident(char c)
{
    return c == '_' || ascii_is_alnum(c);
}

enum Type_Error
lexer_scan_token(struct Lexer *x, struct Token *t)
{
    char curr;

    lexer_skip_whitespace(x);
    x->start = x->current;
    if (lexer_is_at_end(x)) {
        lexer_init_token(x, t, TOKEN_EOF);
        return TYPE_ERROR_EOF;
    }

    curr = lexer_next_char(x);
    if (curr == '_' || ascii_is_letter(curr)) {
        lexer_consume_sequence(x, char_is_ident);
        return lexer_try_keyword(x, t);
    }

    switch (curr) {
    case '(': return lexer_init_token(x, t, TOKEN_PAREN_OPEN);
    case ')': return lexer_init_token(x, t, TOKEN_PAREN_OPEN);
    case '[': return lexer_init_token(x, t, TOKEN_BRACE_OPEN);
    case ']': return lexer_init_token(x, t, TOKEN_BRACE_OPEN);
    case '{': return lexer_init_token(x, t, TOKEN_CURLY_OPEN);
    case '}': return lexer_init_token(x, t, TOKEN_CURLY_OPEN);
    case '*': return lexer_init_token(x, t, TOKEN_ASTERISK);
    case '&': return lexer_init_token(x, t, TOKEN_AMPERSAND);
    case ',': return lexer_init_token(x, t, TOKEN_COMMA);
    case ';': return lexer_init_token(x, t, TOKEN_SEMICOLON);
    default:
        break;
    }
    return TYPE_ERROR_UNEXPECTED_TOKEN;
}

