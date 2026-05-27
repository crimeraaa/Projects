#include <cstdlib> // strtod, strtoul

#include "lexer.hpp"
#include "slice.hpp"

static bool
char_is_ident_start(char c)
{
    return c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

static bool
char_is_decimal(char c)
{
    return '0' <= c && c <= '9';
}

static bool
char_is_ident(char c)
{
    return char_is_decimal(c) || char_is_ident_start(c);
}

Error
Lexer::scan_token(Token *t)
{
    if (!skip_whitespace()) {
        return ERR_UNTERMINATED_COMMENT;
    }

    m_start = m_current;
    if (is_joever()) {
        set_token(t, TK_EOF);
        return ERR_EOF;
    }

    char c = advance();
    if (char_is_ident_start(c)) {
        match_func(char_is_ident);
        return try_keyword(t);
    } else if (char_is_decimal(c)) {
        match_func(char_is_decimal);

        // Only valid for fractional decimal values.
        // Fractionals in other bases are unsupported.
        bool is_frac = match('.');
        if (is_frac) {
            match_func(char_is_decimal);
        }

        // E.g. 1e9
        if (match_either('e', 'E')) {
            match_either('+', '-');
            match_func(char_is_ident);
            is_frac = true;
        }

        // Consume everything else just in case. Works for hex.
        match_func(char_is_ident);
        return try_number(t, is_frac);
    }

    Token_Kind k = cast(Token_Kind)c;
    switch (c) {
    case TK_LPAREN: case TK_RPAREN: case TK_LSQUARE: case TK_RSQUARE:
    case TK_LCURLY: case TK_RCURLY: case TK_COMMA:   case TK_SEMICOL:
    case TK_BNOT:
        break;
    case TK_COLON: k = select_pair(c, TK_SCOPE); break;
    case TK_BAND:  k = select_pair_assign(c, TK_AND, TK_BAND_ASSIGN); break;
    case TK_BOR:   k = select_pair_assign(c, TK_OR,  TK_BOR_ASSIGN);  break;
    case TK_BXOR:  k = select_assign(c, TK_BXOR_ASSIGN); break;
    case TK_LT:    k = select_compound(c, TK_SHL, TK_LEQ, TK_SHL_ASSIGN); break;
    case TK_GT:    k = select_compound(c, TK_SHR, TK_GEQ, TK_SHR_ASSIGN); break;
    case TK_NOT:   k = select_assign(c, TK_NEQ); break;
    case TK_PERIOD: 
        // Have a 2nd period?
        if (match(c)) {
            // Have a 3rd period?
            // Otherwise, got a ".."
            k = match(c) ? TK_ELLIPSIS : TK_NONE;
            break;
        }

        // Possibly a fractional decimal literal,
        // e.g. .1234
        if (match_func(char_is_decimal) > 0) {
            return try_number(t, /*is_frac=*/true);
        }
        k = TK_NONE;
        break;

    case TK_ASSIGN: k = select_pair(c, TK_EQ); break;
    case TK_ADD:    k = select_pair_assign(c, TK_INCR, TK_ADD_ASSIGN); break;
    case TK_SUB:    k = select_pair_assign(c, TK_DECR, TK_SUB_ASSIGN); break;
    case TK_MUL:    k = select_assign(c, TK_MUL_ASSIGN); break;
    case TK_DIV:    k = select_assign(c, TK_DIV_ASSIGN); break;
    default:
        k = TK_NONE;
        break;
    }

    if (k == TK_NONE) {
        set_token(t, k);
        return ERR_UNEXPECTED_TOKEN;
    }
    return set_token(t, k);
}

Token_Kind
Lexer::select_pair(char single, Token_Kind pair)
{
    return match(single) ? pair : cast(Token_Kind)single;
}

Token_Kind
Lexer::select_assign(char single, Token_Kind assign)
{
    return match(TK_ASSIGN) ? assign : cast(Token_Kind)single;
}

Token_Kind
Lexer::select_pair_assign(char single, Token_Kind pair, Token_Kind assign)
{
    if (match(single)) {
        return pair;
    }
    return select_assign(cast(Token_Kind)single, assign);
}

Token_Kind
Lexer::select_compound(char single, Token_Kind pair, Token_Kind assign,
                       Token_Kind pair_assign)
{
    if (match(single)) {
        return select_assign(pair, pair_assign);
    }
    return select_assign(single, assign);
}

bool
Lexer::skip_whitespace()
{
    for (;;) {
        char c = peek();
        switch (c) {
        case '\n':
            m_line += 1;
            m_col   = 0;
            // fall through
        case '\r':
        case '\t':
        case ' ':
            advance();
            break;

        case '/':
            c = peek_next();
            switch (c) {
            case '/':
                skip_line();
                break;
            case '*':
                advance();
                advance();
                // Otherwise, keep going.
                if (!skip_multiline()) {
                    return false;
                }
                break;
            default:
                return true;
            }
            break;

        case '#':
            // Skip preprocessing directives.
            skip_line();
            break;

        default:
            return true;
        }
    }
}

bool
Lexer::skip_multiline()
{
    // Not standard but I really don't care
    int nesting = 1;
    while (!is_joever()) {
        char curr, next;

        curr = advance();
        next = peek();
        if (curr == '/' && next == '*') {
            advance();
            nesting++;
        } else if (curr == '*' && next == '/') {
            advance();
            if (--nesting == 0) {
                return true;
            }
        }
    }
    return false;
}

void
Lexer::skip_line()
{
    while (!is_joever() && !match('\n')) {
        advance();
    }
}

static Error
token_set_if(Token *t, Token_Kind kind, String keyword, size_t offset)
{
    String s1 = slice_from(keyword, offset);
    String s2 = slice_from(t->lexeme, offset);

    // If `len(s2)` overflows, nothing bad happens because `len(s1)` will
    // always be valid and both will compare unequal.
    if (s1 == s2) {
        t->kind = kind;
    }
    return ERR_NONE;
}

// lol
#define X(kw, i) /*kind=*/TK_##kw, /*keyword=*/#kw##_s, /*offset=*/i

Error
Lexer::try_number(Token *t, bool is_frac)
{
    char *endp;
    String s;
    int base = 10;

    set_token(t, TK_NONE);
    s = t->lexeme;

    // Maybe a prefixed integer?
    if (len(s) > 2 && s[0] == '0') {
        base = -1;
        switch (s[1]) {
        case 'b': case 'B': base = 2;  break;
        case 'd': case 'D': base = 10; break;
        case 'o': case 'O': base = 8;  break;
        case 'x': case 'X': base = 16; break;
        default:
            break;
        }

        // Definitely have a prefix, so we need to skip it?
        if (base != -1) {
            s = slice_from(s, 2);
        }
    }

    if (is_frac) {
        t->kind = TK_LITERAL_FLOAT;
        t->data = std::strtod(raw_data(s), &endp);
    } else {
        t->kind = TK_LITERAL_INT;
        t->data = cast(u64)std::strtoul(raw_data(s), &endp, base);
    }
    return (endp == s.end()) ? ERR_NONE : ERR_INVALID_NUMBER;
}

Error
Lexer::try_keyword(Token *t)
{
    set_token(t, TK_IDENT);
    String s = t->lexeme;
    if (!(2 <= len(s) && len(s) <= 8)) {
        return ERR_NONE;
    }

    // C++ keyword lookup is a bitch
    switch (s[0]) {
    case 'b':
        switch (s[1]) {
        case 'o': return token_set_if(t, X(bool,  2));
        case 'r': return token_set_if(t, X(break, 2));
        }
        break;

    case 'c':
        switch (s[1]) {
        case 'a': return token_set_if(t, X(case, 2));
        case 'h': return token_set_if(t, X(char, 2));
        case 'o':
            switch (len(s)) {
            case 5: return token_set_if(t, X(const,    2));
            case 8: return token_set_if(t, X(continue, 2));
            }
        }
        break;

    case 'd':
        switch (s[1]) {
        case 'e': return token_set_if(t, X(default, 2));
        case 'o':
            switch (len(s)) {
            case 2: return token_set_if(t, X(do, 2));
            case 6: return token_set_if(t, X(double, 2));
            }
        }
        break;

    case 'e':
        switch (s[1]) {
        case 'l': return token_set_if(t, X(else, 2));
        case 'n': return token_set_if(t, X(enum, 2));
        }
        break;

    case 'f':
        switch (s[1]) {
        case 'a': return token_set_if(t, X(false, 2));
        case 'l': return token_set_if(t, X(float, 2));
        case 'o': return token_set_if(t, X(for,   2));
        }
        break;

    case 'i':
        switch (len(s)) {
        case 2: return token_set_if(t, X(if, 1));
        case 3: return token_set_if(t, X(int, 1));
        case 6: return token_set_if(t, X(inline, 1));
        }
        break;

    case 'l': return token_set_if(t, X(long, 1));
    case 'n': return token_set_if(t, X(namespace, 1));
    case 'r': return token_set_if(t, X(return, 1));
    case 's':
        switch (s[1]) {
        case 'h': return token_set_if(t, X(short,  2));
        case 'i': return token_set_if(t, X(signed, 2));
        case 't': return token_set_if(t, X(struct, 2));
        }
        break;

    case 't':
        switch (s[1]) {
        case 'e': return token_set_if(t, X(template, 2));
        case 'h': return token_set_if(t, X(this,     2));
        case 'r': return token_set_if(t, X(true,     2));
        case 'y': return token_set_if(t, X(typedef,  2));
        }
        break;

    case 'u':
        switch (s[1]) {
        case 'n':
            switch (len(s)) {
            case 5: return token_set_if(t, X(union, 2));
            case 8: return token_set_if(t, X(unsigned, 2));
            }
            break;
        case 's': return token_set_if(t, X(using, 2));
        }
        break;

    case 'v':
        if (s[1] == 'o') {
            switch (len(s)) {
            case 4: return token_set_if(t, X(void, 2));
            case 8: return token_set_if(t, X(volatile, 2));
            }
        }
        break;
    case 'w': return token_set_if(t, X(while, 1));
    default:
        break;
    }
    return ERR_NONE;
}

#undef X

char
Lexer::peek() const
{
    return *m_current;
}

char
Lexer::peek_next() const
{
    if (is_joever()) {
        return 0;
    }
    return *(m_current + 1);
}

bool
Lexer::is_joever() const
{
    return m_current >= m_input.end();
}


bool
Lexer::check(char c) const
{
    return peek() == c;
}

char
Lexer::advance()
{
    m_col++;
    return *m_current++;
}

bool
Lexer::match(char c)
{
    bool found = check(c);
    if (found) {
        advance();
    }
    return found;
}

bool
Lexer::match_either(char c1, char c2)
{
    return match(c1) || match(c2);
}

int
Lexer::match_func(bool (*predicate)(char c))
{
    int count = 0;
    while (!is_joever()) {
        char c = peek();
        if (!predicate(c)) {
            break;
        }
        advance();
        count += 1;
    }
    return count;
}

Error
Lexer::expect(char c)
{
    return match(c) ? ERR_NONE : ERR_UNEXPECTED_TOKEN;
}

String
Lexer::get_text() const
{
    return {m_start, cast(size_t)(m_current - m_start)};
}

Error
Lexer::set_token(Token *t, Token_Kind k) const
{
    t->kind   = k;
    t->lexeme = get_text();
    t->line   = m_line;
    t->col    = m_col - cast(int)len(t->lexeme);
    return ERR_NONE;
}

