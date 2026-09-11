#include <cstdlib> // strtod

#include "lexer.hpp"
#include "slice.hpp"

static String const
TOKEN_KIND_STRINGS[] = {
#define X(e, s) s##_s,
    TOKEN_KINDS(X)
#undef X
};

static String
token_kind_string(TokenKind k)
{
    return TOKEN_KIND_STRINGS[k];
}

LULU_INTERNAL_FUNC char const *
token_kind_cstring(TokenKind k)
{
    return raw_data(TOKEN_KIND_STRINGS[k]);
}

static char const *
lexer_get_ptr(Lexer const *x, usize i)
{
    return raw_data(x->input) + i;
}

// Must be of the same type as the cursor.
static usize
lexer_end(Lexer const *x)
{
    return len(x->input);
}

static bool
lexer_eof(Lexer const *x)
{
    return x->cursor >= lexer_end(x);
}

static char
lexer_get(Lexer const *x, usize i)
{
    return *lexer_get_ptr(x, i);
}

static char
lexer_peek_char(Lexer const *x)
{
    return lexer_get(x, x->cursor);
}

static char
lexer_peek_next_char(Lexer *x)
{
    if (x->cursor < lexer_end(x)) {
        return lexer_get(x, x->cursor + 1);
    }
    return 0;
}

// Returns the current character and advances the cursor.
static char
lexer_next_char(Lexer *x)
{
    x->col++;
    return lexer_get(x, x->cursor++);
}

static bool
lexer_check_char(Lexer const *x, char c)
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

static String
lexer_get_lexeme(Lexer const *x)
{
    return {lexer_get_ptr(x, x->start), cast(usize)(x->cursor - x->start)};
}

// Wrapper function. Call this manually only for multiline strings.
static Token
token_make(TokenKind k, String s, i32 line, i32 col)
{
    Token t;
    t.kind    = k;
    t.lexeme  = s;
    t.line    = line;
    t.col     = col;
    return t;
}

// Initalizes the given token with the current lexeme.
static void
lexer_init_token(Lexer const *x, Token *out, TokenKind k)
{
    String s;
    i32 line, col;

    s    = lexer_get_lexeme(x);
    line = x->line;
    col  = cast(i32)(cast(usize)x->col - len(s));
    if (len(s) == 0) {
        s = token_kind_string(k);
    }
    *out = token_make(k, s, line, col);
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
char_is_decimal(char c)
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
char_is_lower(char c)
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
char_is_upper(char c)
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
char_is_letter(char c)
{
    return c == '_' || char_is_lower(c) || char_is_upper(c);
}

static bool
char_is_alnum(char c)
{
    return char_is_decimal(c) || char_is_letter(c);
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
            [[fallthrough]];
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
// rather than in registers.
static TokenKind
lexer_get_keyword(String s, TokenKind kind, usize offset)
{
    String kw = token_kind_string(kind);
    if (len(s) != len(kw)) {
        return Token_Ident;
    }
    return (s == kw) ? kind : Token_Ident;
}

static LexerError
lexer_scan_keyword_or_ident(Lexer *x, String s, Token *out)
{
    TokenKind k = Token_Ident;
    // len("do") <= n <= len("function")
    if (2 <= len(s) && len(s) <= 8) switch (s[0]) {
    case 'a': k = lexer_get_keyword(s, Token_and,   1); break;
    case 'b': k = lexer_get_keyword(s, Token_break, 1); break;
    case 'c': k = lexer_get_keyword(s, Token_cast,  1); break;
    case 'd': k = lexer_get_keyword(s, Token_do,    1); break;
    case 'e':
        switch (len(s)) {
        case 3: k = lexer_get_keyword(s, Token_end,    1); break;
        case 4: k = lexer_get_keyword(s, Token_else,   1); break;
        case 6: k = lexer_get_keyword(s, Token_elseif, 1); break;
        }
        break;
    case 'f':
        switch (s[1]) {
        case 'a': k = lexer_get_keyword(s, Token_false,    2); break;
        case 'o': k = lexer_get_keyword(s, Token_for,      2); break;
        case 'u': k = lexer_get_keyword(s, Token_function, 2); break;
        }
        break;
    case 'g': k = lexer_get_keyword(s, Token_global, 1); break;
    case 'i':
        switch (s[1]) {
        case 'f': k = lexer_get_keyword(s, Token_if, 2); break;
        case 'n': k = lexer_get_keyword(s, Token_in, 2); break;
        }
        break;
    case 'l': k = lexer_get_keyword(s, Token_local, 1); break;
    case 'n':
        switch (s[1]) {
        case 'i': k = lexer_get_keyword(s, Token_nil, 2); break;
        case 'o': k = lexer_get_keyword(s, Token_not, 2); break;
        }
        break;
    case 'o': k = lexer_get_keyword(s, Token_or, 1); break;
    case 'r':
        if (len(s) == 6 && s[1] == 'e') switch (s[2]) {
        case 'p': k = lexer_get_keyword(s, Token_repeat, 2); break;
        case 't': k = lexer_get_keyword(s, Token_return, 2); break;
        }
        break;
    case 't':
        switch (s[1]) {
        case 'h': k = lexer_get_keyword(s, Token_then, 2); break;
        case 'r': k = lexer_get_keyword(s, Token_true, 2); break;
        }
        break;
    case 'u': k = lexer_get_keyword(s, Token_until, 1); break;
    case 'w': k = lexer_get_keyword(s, Token_while, 1); break;
    default:
        break;
    }
    lexer_init_token(x, out, k);
    return Lexer_Ok;
}

/*
 Returns:
    The character converted to an integer in the given base,
    or -1 if it was invalid.
 */
static int
char_to_digit(char c, int base)
{
    // We should never have spaces in this function.
    // Rather, we can only encounter invalid base-`base` digits.
    int digit = -1;
    switch (c) {
    case '0':           digit = 0;  break;
    case '1':           digit = 1;  break;
    case '2':           digit = 2;  break;
    case '3':           digit = 3;  break;
    case '4':           digit = 4;  break;
    case '5':           digit = 5;  break;
    case '6':           digit = 6;  break;
    case '7':           digit = 7;  break;
    case '8':           digit = 8;  break;
    case '9':           digit = 9;  break;
    case 'a': case 'A': digit = 10; break;
    case 'b': case 'B': digit = 11; break;
    case 'c': case 'C': digit = 12; break;
    case 'd': case 'D': digit = 13; break;
    case 'e': case 'E': digit = 14; break;
    case 'f': case 'F': digit = 15; break;
    }
    return (0 <= digit && digit < base) ? digit : -1;
}

/*
 NOTE(2026-07-02):
    We assume that we only ever receive positive integers, because
    unary negation on literals only occurs during constant folding
    (if we even have that!).
 */
LULU_INTERNAL_FUNC LexerError
lexer_parse_int(String s, lulu_int *out)
{
    int  base   = 0;
    bool sep_prev = false;
    if (len(s) > 2 && s[0] == '0') switch (s[1]) {
        case 'b': case 'B': base = 2;  break;
        case 'o': case 'O': base = 8;  break;
        case 'd': case 'D': base = 10; break;
        case 'z': case 'Z': base = 12; break;
        case 'x': case 'X': base = 16; break;
        default:
            if (char_is_letter(s[1])) {
                return Lexer_Invalid_Base_Prefix;
            }
    }

    if (base == 0) {
        base = 10;
    } else {
        // Trim the integer prefix. We know the length is >2, so we
        // have something to parse.
        s = slice_from(s, 2);
    }

    // Avoid reading from and writing to garbage values.
    *out = 0;

    // Work from the most significant to least significant digits.
    for (auto c : s) {
        // Implicitly, numbers like _1234 get treated as identifiers,
        // so we don't need to worry about checking which index we're
        // at- i.e. we can assume we always started at a digit.
        if (c == '_') {
            // Don't allow multiple consecutive underscores.
            if (sep_prev) {
                return Lexer_Excess_Underscores;
            }
            sep_prev = true;
            continue;
        }

        // TODO(2026-07-23): Is there a better way of doing this?
        // Does it even matter if we're constantly assigning it?
        sep_prev = false;
        int digit = char_to_digit(c, base);
        if (digit < 0) {
            return Lexer_Invalid_Base_Digit;
        }
        *out *= cast(lulu_int)base;
        *out += cast(lulu_int)digit;
    }

    /*
     TODO(2026-06-30): Check limits?
        `f64` can accurately represent all real numbers in the range
        [-(1 << #mantissa), 1 << #mantissa]. Beyond that, precision is lost,
        i.e. any real numbers in the range (1 << #mantissa, inf] may skip
        over values. The higher up we go, the more values are skipped due
        to imprecision.
     */
    return Lexer_Ok;
}

#define FLAG_FRAC   (1 << 0)
#define FLAG_EXP    (1 << 1)
#define FLAG_SIGN   (1 << 2)
#define FLAG_FLOAT  (FLAG_FRAC | FLAG_EXP)

LULU_INTERNAL_FUNC LexerError
lexer_parse_real(String s, lulu_real *out)
{
    char *pend;

    /*
     TODO(2026-06-30):
        Implement our own `strtod` that doesn't assume nul-termination!
     */
    *out = std::strtod(s.data, &pend);

    // Could point to the nul terminator, so don't use the index operator.
    return pend == end(s) ? Lexer_Ok : Lexer_Invalid_Number;
}

static LexerError
lexer_scan_number(Lexer *x, Token *out)
{
    bool ok    = true;
    u8   flags = 0;
    int  extra = 0;
    lexer_consume_fn(x, char_is_decimal);
    if (lexer_match_char(x, '.')) {
        flags |= FLAG_FRAC;
        lexer_consume_fn(x, char_is_decimal);
    }

    // This should only work for base-10, but we'll check later.
    if (lexer_match_either_char(x, 'e', 'E')) {
        flags |= FLAG_EXP;
        if (lexer_match_either_char(x, '+', '-')) {
            flags |= FLAG_SIGN;
        }
        lexer_consume_fn(x, char_is_decimal);
    }

    // We can allow alphanumerics for prefixed integers, but not floats.
    extra = lexer_consume_fn(x, char_is_alnum);
    lexer_init_token(x, out, (flags & FLAG_FLOAT) ? Token_Float : Token_Int);
    if (flags & FLAG_FLOAT) {
        ok = (extra == 0);
    }
    return ok ? Lexer_Ok : Lexer_Invalid_Number;
}

#undef FLAG_SIGN
#undef FLAG_EXP
#undef FLAG_FRAC

static LexerError
lexer_scan_string(Lexer *x, Token *out, char quote)
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

    // We can also reach here if EOF was found, meaning there was no closing quote.
    lexer_init_token(x, out, Token_String);
    if (!ok) {
        return Lexer_Unterminated_String;
    }

    // Skip the quotes.
    out->lexeme = slice(out->lexeme, 1, len(out->lexeme) - 1);
    return Lexer_Ok;
}

LULU_INTERNAL_FUNC LexerError
lexer_scan_token(Lexer *x, Token *out)
{
    TokenKind k = Token_None;
    char      c = 0;
    lexer_skip_whitespace(x);
    if (lexer_eof(x)) {
        lexer_init_token(x, out, Token_Eof);
        return Lexer_Ok;
    }

    x->start = x->cursor;
    c        = lexer_next_char(x);

    /* TODO(2025-06-29)
        If we're assuming ASCII only (which is a dangerous assumptiuon!) then
        can we just make use of the bitsets? Is that more efficient, or is it
        a meaningless optimization?
     */
    if (char_is_letter(c)) {
        lexer_consume_fn(x, char_is_alnum);
        String s = lexer_get_lexeme(x);
        return lexer_scan_keyword_or_ident(x, s, out);
    } else if (char_is_decimal(c)) {
        return lexer_scan_number(x, out);
    }

    switch (c) {
    case '&': k = Token_Ampersand;  break;
    case '|': k = Token_Pipe;       break;
    case '^': k = Token_Caret;      break;
    case '=': k = lexer_match_char(x, '=') ? Token_Equal_Equal : Token_Assign; break;
    case '+': k = Token_Plus;       break;
    case '-': k = lexer_match_char(x, '>') ? Token_Arrow : Token_Dash; break;
    case '*': k = Token_Asterisk;   break;
    case '/': k = Token_Slash;      break;
    case '%': k = Token_Percent;    break;
    case '~': k = lexer_match_char(x, '=') ? Token_Tilde_Equal   : Token_Tilde;        break;
    case '<': k = lexer_match_char(x, '=') ? Token_Less_Equal    : Token_Less_Than;    break;
    case '>': k = lexer_match_char(x, '=') ? Token_Greater_Equal : Token_Greater_Than; break;
    case '(': k = Token_Open_Paren;   break;
    case ')': k = Token_Close_Paren;  break;
    case '{': k = Token_Open_Curly;   break;
    case '}': k = Token_Close_Curly;  break;
    // TODO: multiline string
    case '[': k = Token_Open_Bracket; break;
    case ']': k = Token_Close_Curly;  break;
    case ':': k = Token_Colon;        break;
    case ';': k = Token_Semicol;      break;
    case ',': k = Token_Comma;        break;
    case '.':
        if (lexer_match_char(x, '.')) {
            // Have '..', try to match '...'.
            k = lexer_match_char(x, '.') ? Token_Vararg : Token_Concat;
            break;
        }

        // Don't have '..' but it could be a fractional literal.
        c = lexer_peek_next_char(x);
        if (char_is_decimal(c)) {
            return lexer_scan_number(x, out);
        }
        k = Token_Period;
        break;
    case '\'':
    case '\"': return lexer_scan_string(x, out, c);
    default:
        break;
    }
    lexer_init_token(x, out, k);
    return k ? Lexer_Ok : Lexer_Unexpected_Character;
}

LULU_INTERNAL_FUNC char const *
lexer_error_string(LexerError err)
{
    switch (err) {
    case Lexer_Ok:                   return "No error";
    case Lexer_Unexpected_Character: return "Unexpected character";
    case Lexer_Invalid_Number:       return "Invalid number";
    case Lexer_Invalid_Base_Prefix:  return "Invalid base prefix";
    case Lexer_Invalid_Base_Digit:   return "invalid base digit";
    case Lexer_Excess_Underscores:   return "Consecutive underscores not supported";
    case Lexer_Unterminated_String:  return "Unterminated string";
    }
    LULU_UNREACHABLE();
    return nullptr;
}
