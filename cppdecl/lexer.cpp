#include "lexer.hpp"

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
    if (!this->skip_whitespace()) {

    }
    this->start = this->current;

    if (this->is_joever()) {
        this->set_token(t, TK_EOF);
        return ERR_EOF;
    }

    char c = this->advance();
    if (char_is_ident_start(c)) {
        this->match_func(char_is_ident);
        return this->try_keyword(t);
    } else if (char_is_decimal(c)) {
        this->match_func(char_is_decimal);
        return this->set_token(t, TK_NUMBER_LITERAL);
    }

    switch (c) {
    case '(': return this->set_token(t, TK_PAREN_OPEN);
    case ')': return this->set_token(t, TK_PAREN_CLOSE);
    case '[': return this->set_token(t, TK_SQUARE_OPEN);
    case ']': return this->set_token(t, TK_SQUARE_CLOSE);
    case '{': return this->set_token(t, TK_CURLY_OPEN);
    case '}': return this->set_token(t, TK_CURLY_CLOSE);
    case ':': return this->set_token2(t, c, TK_SCOPE, TK_COLON);
    case '~': return this->set_token(t, TK_BNOT);
    case '&': return this->set_token3(t, c, TK_AND, TK_BAND_ASSIGN,  TK_AMPERSAND);
    case '|': return this->set_token3(t, c, TK_OR,  TK_BOR_ASSIGN,   TK_PIPE);
    case '<': return this->set_token4(t, c, TK_SHL_ASSIGN, TK_SHL, TK_LEQ, TK_ANGLE_OPEN);
    case '>': return this->set_token4(t, c, TK_SHR_ASSIGN, TK_SHR, TK_GEQ, TK_ANGLE_CLOSE);
    case '!': return this->set_token2(t, '=', TK_NEQ, TK_NOT);
    case '.': 
        // No 2nd?
        if (!this->match(c)) {
            return this->set_token(t, TK_PERIOD);
        }

        // Had a 2nd, now have a 3rd?
        if (this->match(c)) {
            return this->set_token(t, TK_ELLIPSIS);
        }
        break;

    case '+': return this->set_token3(t, c, TK_INCR,  TK_ADD_ASSIGN,   TK_PLUS);
    case '-': return this->set_token3(t, c, TK_DECR,  TK_SUB_ASSIGN,   TK_MINUS);
    case '=': return this->set_token2(t, c, TK_EQ,         TK_ASSIGN);
    case '*': return this->set_token2(t, '=', TK_MUL_ASSIGN, TK_ASTERISK);
    case ',': return this->set_token(t, TK_COMMA);
    case ';': return this->set_token(t, TK_SEMICOL);

    default:
        break;
    }
    this->set_token(t, TK_NONE);
    return ERR_UNEXPECTED_TOKEN;
}

Error
Lexer::set_token2(Token *t, char c, Token_Kind k1c, Token_Kind k1)
{
    return this->set_token(t, this->match(c) ? k1c : k1);
}

Error
Lexer::set_token3(Token *t, char c, Token_Kind k1c, Token_Kind k1eq,
                  Token_Kind k1)
{
    if (this->match(c)) {
        return this->set_token(t, k1c);
    }
    return this->set_token2(t, '=', k1eq, k1);
}

Error
Lexer::set_token4(Token *t, char c, Token_Kind k1ceq, Token_Kind k1c,
                  Token_Kind k1eq, Token_Kind k1)
{
    if (this->match(c)) {
        return this->set_token2(t, '=', k1ceq, k1c);
    }
    return this->set_token2(t, '=', k1eq, k1);
}

bool
Lexer::skip_whitespace()
{
    for (;;) {
        char c = this->peek();
        switch (c) {
        case '\n':
            this->line += 1;
            this->col   = 0;
            // fall through
        case '\r':
        case '\t':
        case ' ':
            this->advance();
            break;

        case '/':
            c = this->peek_next();
            switch (c) {
            case '/':
                this->skip_line();
                break;
            case '*':
                this->advance();
                this->advance();
                // Otherwise, keep going.
                if (!this->skip_multiline()) {
                    return false;
                }
                break;
            default:
                return true;
            }
            break;

        case '#':
            // Skip preprocessing directives.
            this->skip_line();
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
    while (!this->is_joever()) {
        char curr, next;

        curr = this->advance();
        next = this->peek();
        if (curr == '/' && next == '*') {
            this->advance();
            nesting++;
        } else if (curr == '*' && next == '/') {
            this->advance();
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
    while (!this->is_joever() && !this->match('\n')) {
        this->advance();
    }
}

static Error
token_set_if(Token *t, Token_Kind kind, String keyword, size_t offset)
{
    String s1 = slice_from(keyword, offset);
    String s2 = slice_from(t->text, offset);

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
Lexer::try_keyword(Token *t)
{
    this->set_token(t, TK_IDENT);
    String s = t->text;
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
    return *this->current;
}

char
Lexer::peek_next() const
{
    if (this->is_joever()) {
        return 0;
    }
    return *(this->current + 1);
}

bool
Lexer::is_joever() const
{
    return this->current >= this->input.end();
}


bool
Lexer::check(char c) const
{
    return this->peek() == c;
}

char
Lexer::advance()
{
    this->col++;
    return *this->current++;
}

bool
Lexer::match(char c)
{
    bool found = this->check(c);
    if (found) {
        this->advance();
    }
    return found;
}

int
Lexer::match_func(bool (*predicate)(char c))
{
    int count = 0;
    while (!this->is_joever()) {
        char c = this->peek();
        if (!predicate(c)) {
            break;
        }
        this->advance();
        count += 1;
    }
    return count;
}

Error
Lexer::expect(char c)
{
    return this->match(c) ? ERR_NONE : ERR_UNEXPECTED_TOKEN;
}

String
Lexer::get_text() const
{
    return {this->start, cast(size_t)(this->current - this->start)};
}

Error
Lexer::set_token(Token *t, Token_Kind k) const
{
    t->kind = k;
    t->text = this->get_text();
    t->line = this->line;
    t->col  = this->col - cast(int)len(t->text);
    return ERR_NONE;
}

