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

// Wrapper function. Call this manually only for multiline strings.
static Token
token_make(TokenKind k, String view, Pos pos)
{
    Token t;
    t.kind = k;
    t.loc  = Loc{view, pos};
    return t;
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

static TokenKind
getkw_or_id(String s)
{
    static constexpr auto
    check = [](String s, TokenKind kind, usize offset) -> TokenKind {
        String kw = slice_from(token_kind_string(kind), offset);
        String s2 = slice_from(s, offset);
        return s2 == kw ? kind : Token_Ident;
    };

    // len("do") <= n <= len("function")
    if (2 <= len(s) && len(s) <= 8) switch (s[0]) {
    case 'a':     return check(s, Token_and,   1);
    case 'b':     return check(s, Token_break, 1);
    case 'c':     return check(s, Token_cast,  1);
    case 'd':     return check(s, Token_do,    1);
    case 'e':
        switch (len(s)) {
        case 3:   return check(s, Token_end,    1);
        case 4:   return check(s, Token_else,   1);
        case 6:   return check(s, Token_elseif, 1);
        }
        break;
    case 'f':
        switch (s[1]) {
        case 'a': return check(s, Token_false,    2);
        case 'o': return check(s, Token_for,      2);
        case 'u': return check(s, Token_function, 2);
        }
        break;
    case 'g':     return check(s, Token_global, 1);
    case 'i':
        switch (s[1]) {
        case 'f': return check(s, Token_if, 2);
        case 'n': return check(s, Token_in, 2);
        }
        break;
    case 'l':     return check(s, Token_local, 1);
    case 'n':
        switch (s[1]) {
        case 'i': return check(s, Token_nil, 2);
        case 'o': return check(s, Token_not, 2);
        }
        break;
    case 'o':     return check(s, Token_or, 1);
    case 'r':
        if (len(s) == 6 && s[1] == 'e') switch (s[2]) {
        case 'p': return check(s, Token_repeat, 2);
        case 't': return check(s, Token_return, 2);
        }
        break;
    case 't':
        switch (s[1]) {
        case 'h': return check(s, Token_then, 2);
        case 'r': return check(s, Token_true, 2);
        }
        break;
    case 'u':     return check(s, Token_until, 1);
    case 'w':     return check(s, Token_while, 1);
    default:
        break;
    }
    return Token_Ident;
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

static bool
char_is_digit(char c, int base) { return char_to_digit(c, base).is_some(); }

Result<intr, LexerError>
Lexer::parse_int(int base)
{
    intr i        = 0;
    bool sep_prev = false;
    bool sep_curr = false;

    // Work from the most significant to least significant digits.
    for (char c : slice_from(this->input, this->curr_offset)) {
        sep_prev = sep_curr;
        sep_curr = c == '_';
        if (!char_is_digit(c, base) && !sep_curr) {
            break;
        }

        this->next();
        if (sep_curr) {
            // Don't allow multiple consecutive underscores.
            if (sep_prev) {
                return Err(this->make_error(LexerErrorKind::Excess_Underscores));
            }
            continue;
        }

        if (!char_to_digit(c, base)
            .is_some_and([&i, base](int digit) {
                i *= cast(intr)base;
                i += cast(intr)digit;
                return true; }))
        {
            return Err(this->make_error(LexerErrorKind::Invalid_Digit));
        }
    }
    return Ok(i);
}

Result<real, LexerError>
Lexer::parse_fraction()
{
    real numerator   = 0.0;
    real denominator = 1.0;
    bool sep_prev    = false;
    bool sep_curr    = false;

    for (char c : slice_from(this->input, this->curr_offset)) {
        if (!char_is_decimal(c) && c != '_') {
            break;
        }

        this->next();
        sep_prev = sep_curr;
        sep_curr = c == '_';
        if (sep_curr) {
            if (sep_prev) {
                return Err(this->make_error(LexerErrorKind::Excess_Underscores));
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
            return Err(this->make_error(LexerErrorKind::Invalid_Digit));
        }
    }

    return Ok(numerator / denominator);
}

LexerResult
Lexer::scan_number(char leader)
{
    static constexpr auto
    char_is_trailing = [](char c) { return char_is_alnum(c) || c == '.'; };

    if (leader == '0') {
        // Skip the zero, we never need it to calculate the resulting value
        // regardless if it's a prefixed integer or not.
        this->next();

        Option<char> p = this->peek();
        if (p.is_none()) {
            return Ok(this->make_token_int(0));
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
            return Err(this->make_error(LexerErrorKind::Invalid_Base));
        }

        if (base.is_some()) {
            // We skipped the zero already, so skip the base prefix.
            this->next();
            return this->parse_int(base.unwrap())
                .and_then<Token>([this](intr i) -> LexerResult {
                    // Check for trailing characters that we didn't consume.
                    int extra = this->take_while(char_is_trailing);
                    if (extra == 0) {
                        return Ok(this->make_token_int(i));
                    }
                    return Err(this->make_error(LexerErrorKind::Invalid_Digit)); });
        }
    }

    Option<intr> int_part = None{};
    if (leader != '.') {
        Result<intr, LexerError> tmp = this->parse_int(/*base=*/10);
        if (tmp.is_err()) {
            return Err(tmp.unwrap_err());
        }
        int_part = tmp.ok();
    }

    Option<real> frac_part = None{};
    if (leader == '.' || this->match('.')) {
        Result<real, LexerError> tmp = this->parse_fraction();
        if (tmp.is_err()) {
            return Err(tmp.unwrap_err());
        }
        frac_part = tmp.ok();
    }

    Option<intr> exp_part = None{};
    if (this->match('e') || this->match('E')) {
        bool have_plus  = this->match('+');
        bool have_minus = this->match('-');
        if (have_plus && have_minus) {
            return Err(this->make_error(LexerErrorKind::Invalid_Exponent));
        }

        Result<intr, LexerError> tmp = this->parse_int(/*base=*/10);
        if (tmp.is_err()) {
            return Err(tmp.unwrap_err());
        }
        exp_part = tmp.ok();
    }

    int extra = this->take_while(char_is_trailing);
    if (extra > 0) {
        return Err(this->make_error(LexerErrorKind::Invalid_Digit));
    }

    if (int_part.is_some() && frac_part.is_none() && exp_part.is_none()) {
        return Ok(this->make_token_int(int_part.unwrap()));
    }

    static constexpr auto
    pow10 = [](intr e) { return std::pow(10.0, cast(real)e); };

    real mantissa = cast(real)int_part.unwrap_or(0) + frac_part.unwrap_or(0.0);
    real exponent = exp_part.map_or_else(pow10, 1.0);
    return Ok(this->make_token_float(mantissa * exponent));
}

LexerResult
Lexer::scan_string(char quote)
{
    bool ok = false;
    for (;;) {
        if (!this->peek().is_some_and([this, quote, &ok](char c) {
            if (c == '\n') {
                return false;
            }

            this->next();
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
    if (!ok) {
        return Err(this->make_error(LexerErrorKind::Unterminated_String));
    }

    Token t = this->make_token(Token_String);

    // Skip the quotes.
    t.loc.view = slice(t.loc.view, 1, len(t.loc.view) - 1);
    return Ok(t);
}

LexerResult
Lexer::scan_token()
{
    Option<char> p = this->skip_whitespace();
    if (p.is_none()) {
        return Ok(this->make_token(Token_Eof));
    }

    this->prev_offset = this->curr_offset;
    this->prev_pos    = this->curr_pos;
    char c = p.unwrap();
    if (char_is_letter(c)) {
        this->next();
        this->take_while(char_is_alnum);
        String    s = this->lexeme();
        TokenKind k = getkw_or_id(s);
        return Ok(this->make_token(k));
    } else if (char_is_decimal(c)) {
        return this->scan_number(/*leader=*/c);
    }

    this->next();
    TokenKind k = Token_None;
    switch (c) {
    case '&': k = Token_Ampersand;  break;
    case '|': k = Token_Pipe;       break;
    case '^': k = Token_Caret;      break;
    case '=': k = this->match('=') ? Token_Equal_Equal : Token_Assign; break;
    case '+': k = Token_Plus;       break;
    case '-': k = this->match('>') ? Token_Arrow : Token_Dash; break;
    case '*': k = Token_Asterisk;   break;
    case '/': k = Token_Slash;      break;
    case '%': k = Token_Percent;    break;
    case '~': k = this->match('=') ? Token_Tilde_Equal   : Token_Tilde;        break;
    case '<': k = this->match('=') ? Token_Less_Equal    : Token_Less_Than;    break;
    case '>': k = this->match('=') ? Token_Greater_Equal : Token_Greater_Than; break;
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
        if (this->match('.')) {
            // Have '..', try to match '...'.
            k = this->match('.') ? Token_Vararg : Token_Concat;
            break;
        }

        // Don't have '..' but it could be a fractional literal.
        if (this->peek_at(1).is_some_and(char_is_decimal)) {
            return this->scan_number(c);
        }
        k = Token_Period;
        break;
    case '\'':
    case '\"': return this->scan_string(/*quote=*/c);
    default:
        break;
    }

    if (k) {
        return Ok(this->make_token(k));
    } else {
        return Err(this->make_error(LexerErrorKind::Unexpected_Character));
    }
}


LULU_INTERNAL_FUNC char const *
lexer_error_string(LexerErrorKind err)
{
    using E = LexerErrorKind;
    switch (err) {
    case E::Unexpected_Character: return "Unexpected character";
    case E::Invalid_Base:         return "Invalid integer base prefix";
    case E::Invalid_Digit:        return "Invalid digit";
    case E::Invalid_Exponent:     return "Invalid exponent sequence";
    case E::Excess_Underscores:   return "Consecutive underscores not supported";
    case E::Unterminated_String:  return "Unterminated string";
    }
    LULU_UNREACHABLE();
    return nullptr;
}

Option<char>
Lexer::peek_at(usize offset) const noexcept
{
    usize  i = this->curr_offset + offset;
    String s = this->input;
    if (i < len(s)) {
        return Some(s[i]);
    } else {
        return None{};
    }
}

// Returns the current character and advances the cursor.
void
Lexer::next()
{
    this->curr_pos.col++;
    this->curr_offset++;
}

bool
Lexer::check(char wanted) const noexcept
{
    return this->peek().is_some_and([=](char c) { return c == wanted; });
}

bool
Lexer::match(char wanted) noexcept
{
    bool found = this->check(wanted);
    if (found) {
        this->next();
    }
    return found;
}

String
Lexer::lexeme() const
{
    return slice(this->input, this->prev_offset, this->curr_offset);
}

// Keep advancing while the character pointed to by the cursor
// matches the predicate.
template<class F>
int
Lexer::take_while(F f)
{
    int n;
    for (n = 0;; n++) {
        bool ok = this->peek().is_some_and(f);
        if (!ok) {
            break;
        }
        this->next();
    }
    return n;
}

Option<char>
Lexer::skip_whitespace()
{
    for (;;) {
        Option<char> c = this->peek();

        // Return `false` to indicate we need to keep skipping whitespaces,
        // else return `true` to indicate we found a non-whitespace and
        // non-comment character.
        if (c.is_none_or([this](char c) -> bool {
            switch (c) {
            case '\n':
                this->curr_pos.line++;
                this->curr_pos.col = 0; // Will be set to 1 on next advance.
                [[fallthrough]];
            case '\r':
            case '\t':
            case ' ':
                this->next();
                return false;
            case '-':
                if (!this->peek_at(1)
                    .is_some_and([](char c) { return c == '-'; }))
                {
                    break;
                }

                // Skip "--".
                this->next();
                this->next();

                // Don't consume LF, we want to handle it in the switch.
                while (this->peek()
                    .is_some_and([](char c) { return c != '\n'; }))
                {
                    this->next();
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

