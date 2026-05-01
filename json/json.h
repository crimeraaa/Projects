#ifndef JSON_H
#define JSON_H

// standard
#include <stdbool.h>    // bool, true, false
#include <stddef.h>     // size_t
#include <stdint.h>     // uint32_t

// standard, configurable
#if 1
#include <stdio.h>  // printf
#define JSON_EPRINTF(...)           fprintf(stderr, __VA_ARGS__)
#else
#define JSON_EPRINTF(...)
#endif

#define JSON_LOGF(mode, fmt, ...)   JSON_EPRINTF("[" mode "] " fmt, __VA_ARGS__)
#define JSON_LOG(mode, msg)         JSON_LOGF(mode, "%s", msg)
#define JSON_LOGFLN(mode, fmt, ...) JSON_LOGF(mode, fmt "\n", __VA_ARGS__)
#define JSON_LOGLN(mode, msg)       JSON_LOGFLN(mode, "%s", msg)


// local
#include "../mem/mem.h"

enum json_Type {
    // Zero value. No useable value is present.
    JSON_NULL,

    // Primitive types.
    JSON_BOOLEAN, JSON_NUMBER,

    // Allocated types.
    JSON_STRING, JSON_ARRAY, JSON_OBJECT,
};

enum json_Error {
    // Zero value. No error occurred.
    JSON_OK,

    // Tokenizer errors
    JSON_ILLEGAL_CHARACTER, JSON_INVALID_NUMBER, JSON_INVALID_STRING,
    JSON_UNTERMINATED_STRING,

    // Parsing errors
    JSON_UNEXPECTED_TOKEN,

    // Allocation errors
    JSON_INVALID_ALLOCATOR, JSON_OUT_OF_MEMORY,
};

typedef enum json_Type  json_Type;
typedef enum json_Error json_Error;


/** value ::= "null"
 *          | "true"
 *          | "false"
 *          | number
 *          | string
 *          | array
 *          | object
 */
typedef struct json_Value json_Value;


/** object  ::= '{' members* '}'
 *  members ::= ""
 *            | member
 *            | member ',' members
 */
typedef struct json_Object json_Object;


/** array       ::= '[' elements ']'
 *  elements    ::= ""
 *                | value
 *                | value ',' elements
 */
typedef struct json_Array  json_Array;


/** string      ::= '"' characters* '"'
 *  characters  ::= character characters
 *  character   ::= [^\"]
 *                | '\' escape
 *  escape      :: [abfntv"\]
 */
typedef struct json_String json_String;

/** member ::= string ':' value */
typedef struct json_Member json_Member;

struct json_Object {
    // 1D array of 0 or more key-value pairs (table entries).
    json_Member *data;

    // How many members are actively used within `data`.
    size_t len;

    // How many members have been allocated for `data`.
    size_t cap;
};

struct json_Array {
    // 1D array of 0 or more JSON values.
    json_Value *data;

    // How many values are actively used within `data`.
    size_t len;

    // How many members have been allocated for `data`.
    size_t cap;
};

struct json_Value {
    json_Type type;
    union {
        bool         boolean;
        double       number;
        json_Object  object;
        json_Array   array;
        json_String *string;
    };
};

struct json_Member {
    json_String *key;
    json_Value   value;
};

#if defined (__GNUC__)
#define JSON_PACKED  __attribute__((__packed__))
#elif defined(_MSC_VER)

// Align succeeding declarations and types to 1 byte.
// This is the same as packing their members tightly.
#pragma pack(push, 1)
#endif

#ifndef JSON_PACKED
#define JSON_PACKED
#endif

// This is more like a header for a C99-style flexible array. The actual
// string data comes right after, which can be retrieved via pointer
// arithmetic. This should never be allocated on the stack.
struct JSON_PACKED json_String {
    // Number of bytes in the data, excluding the nul terminator.
    size_t len;

    // Saved hash value to help speed up comparisons.
    uint32_t hash;
};

#undef JSON_PACKED
#if defined(_MSC_VER)
#pragma pack(pop)
#endif

extern json_Error
json_parse_lstring(const char *s, size_t n, mem_Allocator allocator,
                   json_Value *v);

extern const char *
json_error_string(json_Error e);

extern const char *
json_type_name(json_Type t);

#define json_value_type(v)      ((v).type)
#define json_value_type_name(v) json_type_name(json_value_type(v))

extern void
json_destroy_value(json_Value value, mem_Allocator alloc);

extern json_Error
json_string_new(size_t write_len, const char *text, size_t text_len,
                mem_Allocator alloc, json_String **ps);

extern char *
json_string_data(json_String *s, size_t *n);

extern json_Error
json_array_append(json_Array *a, json_Value v, mem_Allocator alloc);

extern json_Value
json_object_get(json_Object *o, const char *k);

extern json_Value
json_object_get_lstring(json_Object *o, const char *k, size_t n);

extern json_Error
json_object_insert(json_Object *o, const char *k, json_Value v,
                   mem_Allocator alloc);

extern json_Error
json_object_insert_lstring(json_Object *o, const char *k, size_t n,
                           json_Value v, mem_Allocator alloc);

extern json_Error
json_object_insert_jstring(json_Object *o, json_String *k, json_Value v,
                           mem_Allocator alloc);

extern void
json_print_value(json_Value v);

#define json_make_null()        (json_Value){JSON_NULL,    false}
#define json_make_boolean(b)    (json_Value){JSON_BOOLEAN, b}
#define json_init_number(v, n) do                                              \
{                                                                              \
    json_Value *_v = v;                                                        \
    _v->type       = JSON_NUMBER;                                              \
    _v->number     = n;                                                        \
} while (false)

#define json_init_string(v, s) do                                              \
{                                                                              \
    json_Value *_v = v;                                                        \
    _v->type       = JSON_STRING;                                              \
    _v->string     = s;                                                        \
} while (false)

#define json_init_array(v, a) do                                               \
{                                                                              \
    json_Value *_v = v;                                                        \
    _v->type       = JSON_ARRAY;                                               \
    _v->array      = a;                                                        \
} while (false)

#define json_init_object(v, o) do                                              \
{                                                                              \
    json_Value *_v = v;                                                        \
    _v->type       = JSON_OBJECT;                                              \
    _v->object     = o;                                                        \
} while (false)

#endif // !JSON_H
