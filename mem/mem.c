#include <string.h>	// memcpy, strlen
#include <stdio.h>	// printf

#define MEM_IMPLEMENTATION
#include "mem.h"

#define DYNAMIC_MIN_SIZE	8

typedef struct String_Builder String_Builder;
struct String_Builder {
	mem_Allocator allocator;
	char *data;
	size_t len;
	size_t cap;
};

void
sb_init_none(String_Builder *b, mem_Allocator allocator)
{
	b->allocator = allocator;
	b->data	     = NULL;
	b->len		 = 0;
	b->cap		 = 0;
}

mem_Allocator_Error
sb_init_cap(String_Builder *b, mem_Allocator allocator, size_t cap)
{
	mem_Allocator_Error error = MEM_OK;
	b->allocator = allocator;
	b->data	  = mem_alloc_array(char, &error, allocator, cap);
	b->len		  = 0;
	b->cap		  = (b->data != NULL) ? cap : 0;
	return error;
}

mem_Allocator_Error
sb_destroy(String_Builder *b)
{
	mem_Allocator_Error error = MEM_OK;
	mem_free_array(&error, b->allocator, b->data, b->cap);
	if (error == MEM_OK) {
		b->data = NULL;
		b->len  = 0;
		b->cap  = 0;
	}
	return error;
}

mem_Allocator_Error
sb_write_char(String_Builder *b, char c)
{
	mem_Allocator_Error error = MEM_OK;

	// Appending this character plus the nul char would overflow?
	if (b->len + 2 > b->cap) {
		char *new_data;
		size_t new_cap;

		new_cap = (b->cap > DYNAMIC_MIN_SIZE) ? b->cap * 2 : DYNAMIC_MIN_SIZE;
		new_data = mem_resize_array(char, &error, b->allocator, b->data, b->cap, new_cap);
		if (new_data == NULL) {
			return error;
		}
		b->data = new_data;
		b->cap  = new_cap;
	}

	b->len += 1;
	b->data[b->len - 1] = c;
	b->data[b->len]	  = '\0';
	return error;
}

mem_Allocator_Error
sb_write_string(String_Builder *b, const char *string, size_t string_len)
{
	mem_Allocator_Error error = MEM_OK;
	size_t new_len;

	new_len = b->len + string_len + 2;
	if (new_len > b->cap) {
		char *new_data;
		size_t new_cap;

		new_cap = b->cap * 2;
		if (new_len > new_cap) {
			new_cap = new_len;
		}

		// New capacity is not a power of 2?
		if ((new_cap & (new_cap - 1)) != 0) {
			size_t tmp = 1;
			while (tmp < new_cap) {
				tmp *= 2;
			}
			new_cap = tmp;
		}

		new_data = mem_resize_array(char, &error, b->allocator, b->data, b->cap, new_cap);
		if (new_data == NULL) {
			return error;
		}

		b->data = new_data;
		b->cap	 = new_cap;
	}

	memcpy(&b->data[b->len], string, string_len);
	b->data[new_len] = '\0';
	b->len			  = new_len;
	return error;
}

mem_Allocator_Error
sb_write_cstring(String_Builder *b, const char *cstring)
{
	return sb_write_string(b, cstring, strlen(cstring));
}

#define sb_write_literal(b, lit)	sb_write_string(b, lit, sizeof(lit) - 1)

char
sb_pop_char(String_Builder *b)
{
	size_t n, i;
	char c;

	// Nothing to do?
	n = b->len;
	if (n == 0) {
		return '\0';
	}

	// Read last valid character.
	i = n - 1;
	c = b->data[i];

	// Update, ensuring nul-termiantion.
	b->data[i] = '\0';
	b->len		= i;
	return c;
}

const char *
sb_to_string(String_Builder *b, size_t *len)
{
	if (len != NULL) {
		*len = b->len;
	}
	return b->data;
}


int main(void)
{
	String_Builder b;
	const char *s;
	size_t n;

	sb_init_none(&b, mem_global_heap_allocator());
	sb_write_char(&b, 'H');
	sb_write_char(&b, 'i');
	sb_write_char(&b, ' ');
	sb_write_literal(&b, "mom!");

	s = sb_to_string(&b, &n);
	printf("'%s' (%zu bytes)\n", s, n);

	sb_destroy(&b);
	return 0;
}
