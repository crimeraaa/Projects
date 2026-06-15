// For test main; see below
#if 1

// shut the fuck up microsoft
#define _CRT_SECURE_NO_WARNINGS 1
#define MEM_IMPLEMENTATION

#endif


// standard
#include <string.h> // memcmp

// local
#include "lexer.c"
#include "parser.c"

global json_Value
json_make_null(void)
{
    json_Value v;
    memset(&v, 0, sizeof(v));
    return v;
}

global json_Value
json_make_boolean(bool b)
{
    json_Value v;
    v.type    = JSON_BOOLEAN;
    v.boolean = b;
    return v;
}

global json_Value
json_make_number(double n)
{
    json_Value v;
    v.type   = JSON_NUMBER;
    v.number = n;
    return v;
}

global json_Value
json_make_string(json_String *s)
{
    json_Value v;
    v.type   = JSON_STRING;
    v.string = s;
    return v;
}

global json_Value
json_make_array(json_Array a)
{
    json_Value v;
    v.type  = JSON_ARRAY;
    v.array = a;
    return v;
}

global json_Value
json_make_object(json_Object o)
{
    json_Value v;
    v.type   = JSON_OBJECT;
    v.object = o;
    return v;
}


global json_Error
json_parse_lstring(const char *s, size_t n, mem_Allocator alloc, json_Value *v)
{
    json_Parser p;
    json_Error err;

    // Very first token may have already screwed up?
    err = json_init_parser(&p, s, n, alloc);
    if (err) {
        return err;
    }
    return json_parse(&p, v);
}

global const char *
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

global const char *
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
    case JSON_INVALID_ALLOCATOR:    return "invalid allocator";
    case JSON_OUT_OF_MEMORY:        return "out of memory";
    }

    return NULL;
}

internal void
json_destroy_string(json_String *string, mem_Allocator alloc)
{
    size_t size = sizeof(*string) + string->len + 1;
    JSON_LOGFLN("FREE ", "(json_String *)%p | %zu bytes", string, size);
    mem_free_bytes(alloc, string, size, NULL);
}

internal void
json_destroy_array(json_Array a, mem_Allocator alloc)
{
    for (size_t i = 0; i < a.len; i += 1) {
        json_destroy_value(a.data[i], alloc);
    }
    JSON_LOGFLN("FREE ", "(json_Value  *)%p | %zu bytes", a.data,
              sizeof(*a.data) * a.cap);
    mem_free_array(alloc, a.data, a.cap, NULL);
}

internal void
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

    JSON_LOGFLN("FREE ", "(json_Member *)%p | %zu bytes", cast(void *)o.data,
              sizeof(*o.data) * o.cap);
    mem_free_array(alloc, o.data, o.cap, NULL);
}

global void
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

internal uint32_t
json_string_hash(const char *data, size_t len)
{
    local_persist const uint32_t
    PRIME  = 0x01000193,
    OFFSET = 0x811c9dc5;

    uint32_t hash = OFFSET;
    for (size_t i = 0, n = len; i < n; i += 1) {
        hash ^= cast(uint32_t)data[i];
        hash *= PRIME;
    }
    return hash;
}

internal json_Error
json_mem_error(mem_Allocator_Error err)
{
    if (err == MEM_OUT_OF_MEMORY) {
        return JSON_OUT_OF_MEMORY;
    }
    return JSON_INVALID_ALLOCATOR;
}

global json_Error
json_string_new(
    size_t write_len,
    const char *text, size_t text_len,
    mem_Allocator alloc,
    json_String **ps
) {
    json_String *s;
    size_t size;
    char *data, *writer;
    mem_Allocator_Error err = MEM_OK;
    char prev_char = 0, curr_char = 0;

    size = sizeof(*s) + write_len + 1;
    
    // Use default, pointer-sized alignment to ensure the len and hash
    // members themselves are aligned.
    s = cast(json_String *)mem_alloc_bytes(alloc, size, &err);
    if (err) {
        return json_mem_error(err);
    }

    JSON_LOGFLN(
        "NEW  ",
        "(json_String *)%p | len=%zu, size=%zu",
        cast(void *)s, write_len, size
    );

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
                // JSON_LOGFLN("INFO ", "hex4=%#x, hex_count=%zu",
                //             hex4, byte_count);

                // This can occur if we are called by the user outside
                // of a parse call.
                if (hex4 == UINT16_MAX) {
                    json_destroy_string(s, alloc);
                    return JSON_INVALID_STRING;
                }

                // Skip '\\', 'u' and 3 of the hex chars.
                // The 4th hex char is skipped by the increment.
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

    // JSON_LOGFLN("INFO ", "s = {hash=%u, len=%zu, data=\"%s\"}",
    //             s->hash, write_len, data);
    *ps = s;
    return JSON_OK;
}

global char *
json_string_data(json_String *s, size_t *n)
{
    if (n != NULL) {
        *n = s->len;
    }
    return cast(char *)(s + 1);
}

internal bool
json_string_equals_lstring(
    json_String *a,
    const char *s, size_t n,
    uint32_t hash
) {
    // Quick comparison could be equal?
    size_t a_len;
    const char *a_data = json_string_data(a, &a_len);
    if (a->hash == hash && a_len == n) {
        return memcmp(a_data, s, n) == 0;
    }
    return false;
}

internal bool
json_string_equals(json_String *a, json_String *b)
{
    size_t n;
    const char *s = json_string_data(b, &n);
    return json_string_equals_lstring(a, s, n, b->hash);
}

global json_Error
json_array_append(json_Array *a, json_Value v, mem_Allocator alloc)
{
    if (a->len + 1 > a->cap) {
        json_Value *new_data;
        size_t old_cap, new_cap;
        mem_Allocator_Error err = MEM_OK;

        old_cap = a->cap;
        new_cap = old_cap * 2;
        if (new_cap == 0) {
            new_cap = 8;
        }

        new_data = mem_resize_array(
            json_Value,
            alloc,
            a->data,
            old_cap, new_cap,
            &err
        );

        if (new_data == NULL) {
            return json_mem_error(err);
        }

        a->data = new_data;
        a->cap  = new_cap;
    }
    a->data[a->len++] = v;
    return JSON_OK;
}

internal json_Member *
json_members_get_ptr(
    json_Member *data, size_t cap,
    const char *k, size_t n, uint32_t hash
) {
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

global json_Value
json_object_get(json_Object *o, const char *k)
{
    size_t n = strlen(k);
    return json_object_get_lstring(o, k, n);
}

global json_Value
json_object_get_lstring(json_Object *o, const char *k, size_t n)
{
    json_Member *memb;
    uint32_t hash;

    hash = json_string_hash(k, n);
    memb = json_members_get_ptr(o->data, o->cap, k, n, hash);

    // Since all elements are zero-initialized, unset members will have
    // be null.
    return memb->value;
}

internal json_Error
json_object_resize(json_Object *o, size_t new_cap, mem_Allocator alloc)
{
    json_Member *old_data, *new_data;
    size_t old_cap;
    mem_Allocator_Error err = MEM_OK;

    old_data = o->data;
    old_cap  = o->cap;
    new_data = mem_alloc_array(json_Member, alloc, new_cap, &err);
    if (new_data == NULL) {
        return json_mem_error(err);
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


global json_Error
json_object_insert(
    json_Object *o,
    const char *k,
    json_Value v,
    mem_Allocator alloc
) {
    size_t n = strlen(k);
    return json_object_insert_lstring(o, k, n, v, alloc);
}

global json_Error
json_object_insert_lstring(
    json_Object *o,
    const char *k, size_t n,
    json_Value v,
    mem_Allocator alloc
) {
    json_String *s;
    json_Error err;

    err = json_string_new(n, k, n, alloc, &s);
    if (err) {
        return err;
    }

    err = json_object_insert_jstring(o, s, v, alloc);
    if (err) {
        json_destroy_string(s, alloc);
    }
    return err;
}

global json_Error
json_object_insert_jstring(
    json_Object *o,
    json_String *k,
    json_Value v,
    mem_Allocator alloc
) {
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

internal void
print_helper(json_Value v, int depth);

internal void
print_indent(int count)
{
    for (int i = 0; i < count; i += 1) {
        printf("\t");
    }
}

internal void
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

internal void
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
    print_indent(depth);
    printf("]");
}

internal void
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
    print_indent(depth);
    printf("}");
}

internal void
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

global void
json_print_value(json_Value v)
{
    print_helper(v, /*depth=*/0);
}

#if 1

// horrible
internal int
read_file(const char *name, mem_Allocator alloc, char **p, size_t *n)
{
    FILE *fp;
    char *buf;
    size_t buf_len, written;
    long tmp;
    int err = 0;

    // Error handling in C is quite atrocious
    fp = fopen(name, "rb");
    if (fp == NULL) {
        JSON_LOGFLN("ERROR", "Failed to open file '%s'.", name);
        return 1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        err = 2;
        JSON_LOGFLN("ERROR", "fseek() failed for file '%s'.", name);
        goto cleanup_file;
    }

    tmp = ftell(fp);
    if (tmp == -1L) {
        JSON_LOGFLN("ERROR", "ftell() failed for file '%s'.", name);
        goto cleanup_file;
    }

    buf_len = cast(size_t)tmp;
    rewind(fp);

    buf = mem_alloc_array(char, alloc, buf_len + 1, NULL);
    if (buf == NULL) {
        err = 2;
        JSON_LOGFLN("ERROR", "Failed to alloc %zu bytes to read '%s'.",
                    buf_len + 1, name);
        goto cleanup_file;
    }

    written = fread(buf, sizeof(buf[0]), buf_len, fp);
    if (written != buf_len) {
        err = 2;
        mem_free_array(alloc, buf, buf_len + 1, NULL);
        buf = NULL;
        goto cleanup_file;
    }

    buf[buf_len] = 0;
    *p = buf;
    *n = buf_len;

cleanup_file:
    fclose(fp);
    return err;
}

internal void
run(const char *input, size_t input_len, mem_Allocator alloc)
{
    json_Value v;
    json_Error err;

    err = json_parse_lstring(input, input_len, alloc, &v);
    if (!err) {
        json_print_value(v);
        printf("\n");
        json_destroy_value(v, alloc);
    } else {
        JSON_LOGLN("ERROR", json_error_string(err));
    }
}

#ifdef _WIN32
#define HELP_MESSAGE   "<Ctrl-Z><Enter>"
#else
#define HELP_MESSAGE   "<Ctrl-D>"
#endif

internal void
repl(mem_Allocator alloc)
{
    char *s;
    size_t n;
    char buf[1024];

    fprintf(stdout, "Type " HELP_MESSAGE " to exit.\n");
    for (;;) {
        fputs("json> ", stdout);
        s = fgets(buf, sizeof(buf), stdin);
        // EOF?
        if (s == NULL) {
            return;
        }

        n    = strcspn(s, "\r\n");
        s[n] = 0;
        run(s, n, alloc);
    }
}

int main(int argc, char *argv[])
{
    mem_Allocator alloc = mem_global_heap_allocator();
    if (argc > 1) {
        char *buf;
        size_t buf_len;
        int f_err;

        if (argv[1][0] == '-' && argv[1][1] == 0) {
            repl(alloc);
            return 0;
        }

        f_err = read_file(argv[1], alloc, &buf, &buf_len);
        if (!f_err) {
            run(buf, buf_len, alloc);
            mem_free_array(alloc, buf, buf_len + 1, NULL);
        }
        return f_err;
    }

    local_persist const char test[] = "{\n"
"   \"\\u0068\\u0069\":        \"\\u006d\\u006f\\u006d\",\n" // "hi": "mom"
"   \"\\u006d\\u006f\\u006d\": \"\\u0068\\u0069\"\n"        // "mom": "hi"
"}\n";

    run(test, sizeof(test) - 1, alloc);
    return 0;
}

#endif
