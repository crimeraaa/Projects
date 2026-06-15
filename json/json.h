#ifndef JSON_H
#define JSON_H

// standard
#include <stdbool.h>    // bool, true, false
#include <stddef.h>     // size_t
#include <stdint.h>     // uint32_t

// standard, configurable
#if 1
#include <stdio.h>  // fprintf
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

global json_Value
json_make_null(void);

global json_Value
json_make_boolean(bool b);

global json_Value
json_make_number(double n);

global json_Value
json_make_string(json_String *s);

global json_Value
json_make_array(json_Array a);

global json_Value
json_make_object(json_Object o);

global json_Error
json_parse_lstring(const char *s, size_t n, mem_Allocator alloc,
                   json_Value *v);

global const char *
json_error_string(json_Error e);

global const char *
json_type_name(json_Type t);

#define json_value_type(v)      ((v).type)
#define json_value_type_name(v) json_type_name(json_value_type(v))

global void
json_destroy_value(json_Value value, mem_Allocator alloc);

global json_Error
json_string_new(
    size_t write_len,
    const char *text, size_t text_len,
    mem_Allocator alloc,
    json_String **ps
);

global char *
json_string_data(json_String *s, size_t *n);

global json_Error
json_array_append(json_Array *a, json_Value v, mem_Allocator alloc);

global json_Value
json_object_get(json_Object *o, const char *k);

global json_Value
json_object_get_lstring(json_Object *o, const char *k, size_t n);

global json_Error
json_object_insert(
    json_Object *o,
    const char *k,
    json_Value v,
    mem_Allocator alloc
);

global json_Error
json_object_insert_lstring(
    json_Object *o,
    const char *k, size_t n,
    json_Value v,
    mem_Allocator alloc
);

global json_Error
json_object_insert_jstring(
    json_Object *o,
    json_String *k,
    json_Value v,
    mem_Allocator alloc
);

global void
json_print_value(json_Value v);

#endif // !JSON_H
