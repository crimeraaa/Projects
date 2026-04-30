#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define cast(T)	(T)


/** @brief	Signature for input reading callback functions.
 *
 * @param [out]	  len		Length, in bytes, of the returned buffer.
 * @param [inout] user_data	Arbitrary user-supplied data/context.
 *
 * @returns data
 *	Pointer to some readable buffer within/from `user_data`.
 *	Must return non-NULL to signify valid input remains. Otherwise, NULL
 *	must be returned to signal EOF.
 */
typedef const char *(*Reader_Fn)(size_t *len, void *user_data);

typedef struct String_Reader String_Reader;
struct String_Reader {
	const char *data;
	size_t		len;
};

const char *
string_reader_callback(size_t *len, void *user_data)
{
	String_Reader *r;
	const char *p;

	r	 = cast(String_Reader *)user_data;
	p	 = r->data;
	*len = r->len;

	// First (and only) read? Mark the string as read.
	if (p != NULL && r->len > 0) {
		r->data = NULL;
		r->len  = 0;
	}
	return p;
}

typedef struct File_Reader File_Reader;
struct File_Reader {
	FILE *stream;
	char  buffer[BUFSIZ];
};

const char *
file_reader_callback(size_t *len, void *user_data)
{
	File_Reader *r;
	size_t n;

	r = cast(File_Reader *)user_data;
	// Nothing more to read?
	if (feof(r->stream)) {
		return NULL;
	}

	// Delegate the actual buffering to the C standard library implementation.
	n	 = fread(r->buffer, sizeof(r->buffer), 1, r->stream);
	*len = n;
	return (n > 0) ? r->buffer : NULL;
}

void
file_reader_init(File_Reader *r, const char *file_name)
{
	FILE *f = fopen(file_name, "r");
	if (f == NULL) {
		fprintf(stderr, "Failed to open file '%s'.\n", file_name);
		return;
	}

	r->stream = f;
}


