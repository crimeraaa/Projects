// standard
#include "json.h"

#include <string.h> // memcmp

// local
#include "parser.h"

extern json_Error
json_parse_lstring(const char *s, size_t n, mem_Allocator alloc, json_Value *v)
{
    json_Parser p;
    json_init_parser(&p, s, n, alloc);
    return json_parse(&p, v);
}

extern const char *
json_type_name(json_Type t)
{
    switch (t) {
    case JSON_NULL:     return "null";
    case JSON_BOOLEAN:  return "boolean";
    case JSON_NUMBER:   return "number";
    case JSON_STRING:   return "string";
    case JSON_ARRAY:    return "array";
    case JSON_OBJECT:   return "object";
    }
    return NULL;
}

extern const char *
json_error_string(json_Error e)
{
    switch (e) {
    // Zero value which indicates no error occurred.
    case JSON_OK: return "no error";

    // Tokenizer errors
    case JSON_ILLEGAL_CHARACTER:    return "illegal character";
    case JSON_INVALID_NUMBER:       return "invalid number";
    case JSON_INVALID_STRING:       return "invalid string";
    case JSON_UNTERMINATED_STRING:  return "unterminated string";

    // Parsing errors
    case JSON_UNEXPECTED_TOKEN:     return "unexpected token";

    // Allocation errors
    case JSON_OUT_OF_MEMORY:        return "out of memory";
    }

    return NULL;
}

static void
json_destroy_string(json_String *string, mem_Allocator alloc)
{
    size_t size = sizeof(*string) + string->len + 1;
    JSON_LOGFLN("FREE", "(json_String *)%p | %zu bytes", string, size);
    mem_free_bytes(alloc, string, size, NULL);
}

static void
json_destroy_array(json_Array a, mem_Allocator alloc)
{
    for (size_t i = 0; i < a.len; i += 1) {
        json_destroy_value(a.data[i], alloc);
    }
    JSON_LOGFLN("FREE", "(json_Value *)%p : %zu bytes", a.data,
              sizeof(*a.data) * a.cap);
    mem_free_array(alloc, a.data, a.cap, NULL);
}

static void
json_destroy_object(json_Object o, mem_Allocator alloc)
{
    // Assumes that we own all of our strings
    for (size_t i = 0; i < o.cap; i += 1) {
        json_String *s = o.data[i].key;
        if (s == NULL) {
            continue;
        }
        json_destroy_string(s, alloc);
        json_destroy_value(o.data[i].value, alloc);
    }

    JSON_LOGFLN("FREE", "(json_Member *)%p : %zu bytes", cast(void *)o.data,
              sizeof(*o.data) * o.cap);
    mem_free_array(alloc, o.data, o.cap, NULL);
}

extern void
json_destroy_value(json_Value v, mem_Allocator alloc)
{
    switch (v.type) {
    case JSON_NULL:
    case JSON_BOOLEAN:
    case JSON_NUMBER: break;
    case JSON_STRING: json_destroy_string(v.string, alloc); break;
    case JSON_ARRAY:  json_destroy_array(v.array, alloc);   break;
    case JSON_OBJECT: json_destroy_object(v.object, alloc); break;
    }
}

static uint32_t
json_string_hash(const char *data, size_t len)
{
    static const uint32_t
    PRIME  = 0x01000193,
    OFFSET = 0x811c9dc5;

    uint32_t hash = OFFSET;
    for (size_t i = 0, n = len; i < n; i += 1) {
        hash ^= cast(uint32_t)data[i];
        hash *= PRIME;
    }
    return hash;
}

extern json_String *
json_string_new(size_t write_len, const char *text, size_t text_len, mem_Allocator alloc)
{
    json_String *s;
    size_t size;
    char *data, *writer;
    char prev_char = 0, curr_char = 0;

    size = sizeof(*s) + write_len + 1;
    s    = cast(json_String *)mem_alloc_bytes(alloc, size, NULL);
    if (s == NULL) {
        return NULL;
    }

    JSON_LOGFLN("NEW ", "(json_String *)%p | len=%zu, size=%zu",
                cast(void *)s, write_len, size);

    s->len = write_len;
    data   = json_string_data(s, NULL);
    writer = data;

    for (const char *it = text, *end = it + text_len; it < end; it += 1) {
        prev_char = curr_char;
        curr_char = *it;

        // We're in an escape sequence, so read it.
        if (prev_char == '\\') {
            size_t byte_count = 0;
            uint16_t hex4 = 0;
            switch (curr_char) {
            case '"':
            case '\\':
            case '/':  *writer++ = curr_char; break;
            case 'b':  *writer++ = '\b';      break;
            case 'f':  *writer++ = '\f';      break;
            case 'n':  *writer++ = '\n';      break;
            case 'r':  *writer++ = '\r';      break;
            case 't':  *writer++ = '\t';      break;
            case 'u':
                // Point to the "\u" in its entirety so we can sanity check...
                it -= 1;
                hex4 = json_decode_hex4(it, cast(size_t)(end - it), &byte_count);
                // JSON_LOGFLN("INFO", "hex4=%#x, hex_count=%zu", hex4, byte_count);

                // We screwed up by not properly verifying the string
                // in the lexer phase...
                if (hex4 == UINT16_MAX) {
                    json_destroy_string(s, alloc);
                    return NULL;
                }

                // Then skip the whole sequence.
                it += 5;
                memcpy(writer, &hex4, byte_count);
                writer += byte_count;
                break;
            }
            prev_char = 0;
        }
        else if (curr_char != '\\') {
            *writer++ = curr_char;
        }
    }

    data[write_len] = 0;
    // Hash the actual represented bytes- including now-escaped characters.
    s->hash = json_string_hash(data, s->len);

    // JSON_LOGFLN("INFO", "s = {hash=%u, len=%zu, data=\"%s\"}",
    //             s->hash, write_len, data);
    return s;
}

extern char *
json_string_data(json_String *s, size_t *n)
{
    if (n != NULL) {
        *n = s->len;
    }
    return cast(char *)(s + 1);
}

static bool
json_string_equals_lstring(json_String *a, const char *s, size_t n,
                           uint32_t hash)
{
    // Quick comparison could be equal?
    size_t a_len;
    const char *a_data = json_string_data(a, &a_len);
    if (a->hash == hash && a_len == n) {
        return memcmp(a_data, s, n) == 0;
    }
    return false;
}

static bool
json_string_equals(json_String *a, json_String *b)
{
    size_t n;
    const char *s = json_string_data(b, &n);
    return json_string_equals_lstring(a, s, n, b->hash);
}

extern json_Error
json_array_append(json_Array *a, json_Value v, mem_Allocator alloc)
{
    if (a->len + 1 > a->cap) {
        json_Value *new_data;
        size_t old_cap, new_cap;

        old_cap = a->cap;
        new_cap = old_cap * 2;
        if (new_cap == 0) {
            new_cap = 8;
        }

        new_data = mem_resize_array(json_Value, alloc, a->data, old_cap, new_cap, NULL);
        if (new_data == NULL) {
            return JSON_OUT_OF_MEMORY;
        }

        a->data = new_data;
        a->cap  = new_cap;
    }
    a->data[a->len++] = v;
    return JSON_OK;
}

static json_Member *
json_members_get_ptr(json_Member *data, size_t cap, const char *k, size_t n,
                     uint32_t hash)
{
    json_Member *prev;
    size_t wrap;

    prev = NULL;
    wrap = cap - 1;
    for (size_t i = cast(size_t)hash & wrap; i < cap; i = (i + 1) & wrap) {
        json_Member *memb = &data[i];
        if (memb->key == NULL) {
            if (prev == NULL) {
                prev = memb;
            } else {
                return prev;
            }
        } else if (json_string_equals_lstring(memb->key, k, n, hash)) {
            return memb;
        }
    }
    // Unreachable
    return NULL;
}

extern json_Value
json_object_get(json_Object *o, const char *k)
{
    json_Member *memb;
    size_t n;
    uint32_t hash;

    n    = strlen(k);
    hash = json_string_hash(k, n);
    memb = json_members_get_ptr(o->data, o->cap, k, n, hash);

    // Since all elements are zero-initialized, unset members will have
    // be null.
    return memb->value;
}

static json_Error
json_object_resize(json_Object *o, size_t new_cap, mem_Allocator alloc)
{
    json_Member *old_data, *new_data;
    size_t old_cap;

    old_data = o->data;
    old_cap  = o->cap;
    new_data = mem_alloc_array(json_Member, alloc, new_cap, NULL);
    if (new_data == NULL) {
        return JSON_OUT_OF_MEMORY;
    }

    // Rehash
    for (size_t i = 0; i < old_cap; i += 1) {
        json_Member *memb;
        json_String *k;
        const char *s;
        size_t n;

        k = old_data[i].key;
        if (k == NULL) {
            continue;
        }

        s     = json_string_data(k, &n);
        memb  = json_members_get_ptr(new_data, new_cap, s, n, k->hash);
        *memb = old_data[i];
    }
    mem_free_array(alloc, old_data, old_cap, NULL);
    o->data = new_data;
    o->cap  = new_cap;
    return JSON_OK;
}


extern json_Error
json_object_insert(json_Object *o, const char *k, json_Value v, mem_Allocator alloc)
{
    size_t n = strlen(k);
    return json_object_insert_lstring(o, k, n, v, alloc);
}

extern json_Error
json_object_insert_lstring(json_Object *o, const char *k, size_t n,
                           json_Value v, mem_Allocator alloc)
{
    json_String *s;
    json_Error err;

    s = json_string_new(n, k, n, alloc);
    if (s == NULL) {
        return JSON_OUT_OF_MEMORY;
    }

    err = json_object_insert_jstring(o, s, v, alloc);
    if (err) {
        json_destroy_string(s, alloc);
    }
    return err;
}

extern json_Error
json_object_insert_jstring(json_Object *o, json_String *k, json_Value v,
                           mem_Allocator alloc)
{
    json_Member *memb;
    const char *s;
    size_t n;
    json_Error err = JSON_OK;

    if (o->len + 1 > (o->cap * 3 / 4)) {
        size_t new_cap = o->cap * 2;
        if (new_cap == 0) {
            new_cap = 8;
        }

        err = json_object_resize(o, new_cap, alloc);
        if (err) {
            return err;
        }
    }

    s    = json_string_data(k, &n);
    memb = json_members_get_ptr(o->data, o->cap, s, n, k->hash);

    // printf("[GET ] | (json_Member *)%p : index=%zu\n", cast(void *)memb,
    //        cast(size_t)(memb - o->data));
    // Account for duplicates: first time inserting this key?
    if (memb->key == NULL || !json_string_equals(memb->key, k)) {
        o->len++;
    }

    // "key": "value"
    memb->key   = k;
    memb->value = v;
    return JSON_OK;
}

static void
print_helper(json_Value v, int depth);

static void
print_indent(int count)
{
    for (int i = 0; i < count; i += 1) {
        printf("\t");
    }
}

static void
print_string(json_String *s)
{
    const char *data;
    size_t start, stop;

    start = 0;
    data  = json_string_data(s, &stop);
    printf("\"");
    for (size_t i = 0; i < stop; i += 1) {
        char c = data[i];
        switch (c) {
        // Character literals
        case '"': case '/': case '\\':
            goto print_escaped;
        // ASCII escape sequences
        case '\b': c = 'b'; goto print_escaped;
        case '\f': c = 'f'; goto print_escaped;
        case '\r': c = 'r'; goto print_escaped;
        case '\n': c = 'n'; goto print_escaped;
        case '\t': c = 't'; print_escaped:
            printf("%.*s\\%c", cast(int)(i - start), &data[start], c);
            start = i + 1;
            break;
        }
    }
    printf("%.*s\"", cast(int)(stop - start), &data[start]);
}

static void
print_array(json_Array a, int depth)
{
    printf("[");
    if (a.len > 0) {
        printf("\n");
        for (size_t i = 0; i < a.len; i += 1) {
            if (i > 0) {
                printf(",\n");
            }
            print_indent(depth + 1);
            print_helper(a.data[i], depth + 1);
        }
        printf("\n");
    }
    printf("]");
}

static void
print_object(json_Object o, int depth)
{
    printf("{");
    if (o.len > 0) {
        size_t written = 0;
        printf("\n");
        for (size_t i = 0; i < o.cap; i += 1) {
            json_Member memb = o.data[i];
            if (memb.key == NULL) {
                continue;
            }

            if (written++ > 0) {
                printf(",\n");
            }
            print_indent(depth + 1);
            print_string(memb.key);
            printf(": ");
            print_helper(memb.value, depth + 1);
        }
        printf("\n");
    }
    printf("}");
}

static void
print_helper(json_Value v, int depth)
{
    switch (v.type) {
    case JSON_NULL:     printf("null"); break;
    case JSON_BOOLEAN:  printf("%s", (v.boolean) ? "true" : "false"); break;
    case JSON_NUMBER:   printf("%.14g", v.number);      break;
    case JSON_STRING:   print_string(v.string);         break;
    case JSON_ARRAY:    print_array(v.array, depth);    break;
    case JSON_OBJECT:   print_object(v.object, depth);  break;
    }
}

extern void
json_print_value(json_Value value)
{
    print_helper(value, /*depth=*/0);
}

