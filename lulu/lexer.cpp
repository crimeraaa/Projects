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

static Option<char>
lexer_peek_char(Lexer const *x)
{
    if (x->curr_offset < len(x->input)) {
        return Some(x->input[x->curr_offset]);
    } else {
        return None{};
    }
}

static Option<char>
lexer_peek_next_char(Lexer *x)
{
    if (x->curr_offset + 1 < len(x->input)) {
        return Some(x->input[x->curr_offset + 1]);
    } else {
        return None{};
    }
}

// Returns the current character and advances the cursor.
static void
lexer_next_char(Lexer *x)
{
    x->curr_pos.col++;
    x->curr_offset++;
}

static bool
lexer_check_char(Lexer const *x, char wanted)
{
    return lexer_peek_char(x).is_some_and([=](char c) { return c == wanted; });
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
    return slice(x->input, x->prev_offset, x->curr_offset);
}

// Wrapper function. Call this manually only for multiline strings.
static Token
token_make(TokenKind k, String view, Pos pos)
{
    Token t;
    t.kind = k;
    t.loc  = Loc{view, pos};
    return t;
}

// Initalizes the given token with the current lexeme.
static void
lexer_init_token(Lexer const *x, Token *out, TokenKind k)
{
    String s = lexer_get_lexeme(x);
    if (len(s) == 0) {
        s = token_kind_string(k);
    }
    *out = token_make(k, s, x->prev_pos);
}

// Keep advancing while the character pointed to by the cursor
// matches the predicate.
static int
lexer_consume_fn(Lexer *x, bool (*fn)(char c))
{
    int n;
    for (n = 0;; n++) {
        bool ok = lexer_peek_char(x).is_some_and(fn);
        if (!ok) {
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

static Option<char>
lexer_skip_whitespace(Lexer *x)
{
    for (;;) {
        Option<char> c = lexer_peek_char(x);

        // Return `false` to indicate we need to keep skipping whitespaces,
        // else return `true` to indicate we found a non-whitespace and
        // non-comment character.
        if (c.is_none_or([x](char c) -> bool {
            switch (c) {
            case '\n':
                x->curr_pos.line++;
                x->curr_pos.col = 0; // Will be set to 1 on next advance.
                [[fallthrough]];
            case '\r':
            case '\t':
            case ' ':
                lexer_next_char(x);
                return false;
            case '-':
                if (!lexer_peek_next_char(x)
                    .is_some_and([](char c) { return c == '-'; }))
                {
                    break;
                }

                // Skip "--".
                lexer_next_char(x);
                lexer_next_char(x);

                // Don't consume LF, we want to handle it in the switch.
                while (lexer_peek_char(x)
                    .is_some_and([](char c) { return c != '\n'; }))
                {
                    lexer_next_char(x);
                }
                return false;
            default:
                break;
            }
            return true; }))
        {
            return c;
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
    return LexerError::Ok;
}

static Option<int>
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
    if (0 <= digit && digit < base) {
        return Some(digit);
    } else {
        return None{};
    }
}

static Result<intr, LexerError>
lexer_parse_int(Lexer *x, int base)
{
    intr i        = 0;
    bool sep_prev = false;
    bool sep_curr = false;

    // Work from the most significant to least significant digits.
    for (char c : slice_from(x->input, x->curr_offset)) {
        if (!char_is_alnum(c)) {
            break;
        }

        lexer_next_char(x);
        sep_prev = sep_curr;
        sep_curr = c == '_';
        if (sep_curr) {
            // Don't allow multiple consecutive underscores.
            if (sep_prev) {
                return Err(LexerError::Excess_Underscores);
            }
            continue;
        }

        if (!char_to_digit(c, base)
            .is_some_and([&i, base](int digit) {
                i *= cast(intr)base;
                i += cast(intr)digit;
                return true; }))
        {
            return Err(LexerError::Invalid_Digit);
        }
    }
    return Ok(i);
}

static Result<real, LexerError>
lexer_parse_fraction(Lexer *x)
{
    real numerator   = 0.0;
    real denominator = 1.0;
    bool sep_prev    = false;
    bool sep_curr    = false;

    for (char c : slice_from(x->input, x->curr_offset)) {
        if (!char_is_decimal(c) && c != '_') {
            break;
        }

        lexer_next_char(x);
        sep_prev = sep_curr;
        sep_curr = c == '_';
        if (sep_curr) {
            if (sep_prev) {
                return Err(LexerError::Excess_Underscores);
            }
            continue;
        }

        if (!char_to_digit(c, /*base=*/10)
            .is_some_and([&](int digit) {
                numerator    = numerator * 10.0 + cast(real)digit;
                denominator *= 10.0;
                return true;
            }))
        {
            return Err(LexerError::Invalid_Digit);
        }
    }

    return Ok(numerator / denominator);
}

static real
pow10(intr exponent) { return std::pow(10.0, cast(real)exponent); }

static LexerError
lexer_scan_number(Lexer *x, Token *out, char leader)
{
    if (leader == '0') {
        // Skip the zero, we never need it to calculate the resulting value
        // regardless if it's a prefixed integer or not.
        lexer_next_char(x);
        Option<char> p = lexer_peek_char(x);
        if (p.is_none()) {
            lexer_init_token(x, out, Token_Int);
            out->integer = 0;
            return LexerError::Ok;
        }

        Option<int> base   = None{};
        char        prefix = p.unwrap();
        switch (prefix) {
        case 'b': case 'B': base = Some( 2); break;
        case 'd': case 'D': base = Some(10); break;
        case 'o': case 'O': base = Some( 8); break;
        case 'x': case 'X': base = Some(16); break;
        case 'z': case 'Z': base = Some(12); break;
        default:
            if (char_is_decimal(prefix)) {
                break;
            }
            return LexerError::Invalid_Base;
        }

        if (base.is_some()) {
            // We skipped the zero already, so skip the base prefix.
            lexer_next_char(x);
            auto r = lexer_parse_int(x, base.unwrap());
            if (r.is_err()) {
                return r.unwrap_err();
            }
            lexer_init_token(x, out, Token_Int);
            return LexerError::Ok;
        }
    }

    Option<intr> int_part = None{};
    if (leader != '.') {
        auto tmp = lexer_parse_int(x, /*base=*/10);
        if (tmp.is_err()) {
            return tmp.unwrap_err();
        }
        int_part = tmp.ok();
    }

    Option<real> frac_part = None{};
    if (leader == '.' || lexer_match_char(x, '.')) {
        auto tmp = lexer_parse_fraction(x);
        if (tmp.is_err()) {
            return tmp.unwrap_err();
        }
        frac_part = tmp.ok();
    }

    Option<intr> exp_part = None{};
    if (lexer_match_char(x, 'e') || lexer_match_char(x, 'E')) {
        bool have_plus  = lexer_match_char(x, '+');
        bool have_minus = lexer_match_char(x, '-');
        if (have_plus && have_minus) {
            return LexerError::Invalid_Exponent;
        }

        auto tmp = lexer_parse_int(x, /*base=*/10);
        if (tmp.is_err()) {
            return tmp.unwrap_err();
        }
        exp_part = tmp.ok();
    }

    if (int_part.is_some_and([&](intr i) {
        if (frac_part.is_some() || exp_part.is_some()) {
            return false;
        }

        lexer_init_token(x, out, Token_Int);
        out->integer = i;
        return true; }))
    {
        return LexerError::Ok;
    }

    real mantissa = cast(real)int_part.unwrap_or(0) + frac_part.unwrap_or(0.0);
    real exponent = exp_part.map_or_else(pow10, 1.0);
    lexer_init_token(x, out, Token_Float);
    out->floating = mantissa * exponent;
    return LexerError::Ok;
}

static LexerError
lexer_scan_string(Lexer *x, Token *out, char quote)
{
    bool ok = false;
    for (;;) {
        if (!lexer_peek_char(x).is_some_and([&ok, x, quote](char c) {
            if (c == '\n') {
                return false;
            }

            lexer_next_char(x);
            if (c == quote) {
                ok = true;
                return false;
            }
            return true; }))
        {
            break;
        }
    }

    // We can also reach here if EOF was found, meaning there was no closing quote.
    lexer_init_token(x, out, Token_String);
    if (!ok) {
        return LexerError::Unterminated_String;
    }

    // Skip the quotes.
    out->loc.view = slice(out->loc.view, 1, len(out->loc.view) - 1);
    return LexerError::Ok;
}

LULU_INTERNAL_FUNC LexerError
lexer_scan_token(Lexer *x, Token *out)
{
    Option<char> p = lexer_skip_whitespace(x);
    if (p.is_none()) {
        lexer_init_token(x, out, Token_Eof);
        return LexerError::Ok;
    }

    x->prev_offset = x->curr_offset;
    x->prev_pos    = x->curr_pos;
    char c = p.unwrap();
    if (char_is_letter(c)) {
        lexer_next_char(x);
        lexer_consume_fn(x, char_is_alnum);
        String s = lexer_get_lexeme(x);
        return lexer_scan_keyword_or_ident(x, s, out);
    } else if (char_is_decimal(c)) {
        return lexer_scan_number(x, out, /*leader=*/c);
    }

    lexer_next_char(x);
    TokenKind k = Token_None;
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
        if (lexer_peek_next_char(x).is_some_and(char_is_decimal)) {
            return lexer_scan_number(x, out, c);
        }
        k = Token_Period;
        break;
    case '\'':
    case '\"': return lexer_scan_string(x, out, c);
    default:
        break;
    }
    lexer_init_token(x, out, k);
    return k ? LexerError::Ok : LexerError::Unexpected_Character;
}

LULU_INTERNAL_FUNC char const *
lexer_error_string(LexerError err)
{
    using E = LexerError;
    switch (err) {
    case E::Ok:                   return "No error";
    case E::Unexpected_Character: return "Unexpected character";
    case E::Invalid_Base:         return "Invalid base";
    case E::Invalid_Digit:        return "Invalid digit";
    case E::Invalid_Exponent:     return "Invalid exponent";
    case E::Excess_Underscores:   return "Consecutive underscores not supported";
    case E::Unterminated_String:  return "Unterminated string";
    }
    LULU_UNREACHABLE();
    return nullptr;
}
