#ifndef MEM_H
#define MEM_H

#include <stddef.h>
#include <stdalign.h>

enum mem_Allocator_Mode {
	// The allocator must return a new, zero-initialized block of memory.
	//
	// Otherwise, NULL should be returned. If the error out-parameter is
	// also non-NULL then it should be set in this case.
	//
	MEM_NEW,

	// The allocator must resize the given block of memory to some new size.
	// If it is growing then the new, unoccupied region must be
	// zero-initialized.
	//
	// Otherwise NULL must be returned. If the error out-parameter is also
	// non-NULL then it should also be set in this case.
	//
	MEM_RESIZE,

	// The allocator must return a new block of memory without
	// zero-initializing it. This may be done for performance reasons.
	//
	// Otherwise, NULL should be returned. If the error out-parameter is non-null
	// then it should also be set in this case.
	//
	MEM_NEW_NONZERO,

	// The allocator must resize the given block of memory to some new size.
	// If it is growing then the new, unoccupied region must not be
	// zero-initialized. This may be done for performance reasons.
	//
	// Otherwise NULL must be returned. If the error out-parameter is non-NULL,
	// then it should also be set in this case.
	//
	MEM_RESIZE_NONZERO,

	// The allocator must free the given block of memory.
	//
	// If this operation somehow fails then the error out-parameter should
	// be set iff it is non-NULL.
	//
	MEM_FREE,

	// The allocator must free ALL blocks of memory it owns. All allocations
	// are invalidated. This is done mainly for arena allocators.
	//
	MEM_FREE_ALL,
};

enum mem_Allocator_Error {
	// No error occurred.
	//
	MEM_OK,

	// The allocator was unable to fulfill a new or resize request because.
	// there is no more backing memory and/or it cannot acquire more of it.
	//
	MEM_OUT_OF_MEMORY,

	// The allocator was unable to fulfill a request because it is not
	// intended to handle it. E.g. arena allocators cannot do frees.
	//
	MEM_NOT_IMPLEMENTED,
};


typedef enum mem_Allocator_Mode  mem_Allocator_Mode;
typedef enum mem_Allocator_Error mem_Allocator_Error;
typedef void *(*mem_Allocator_Fn)(
	mem_Allocator_Error *error,
	mem_Allocator_Mode	 mode,
	void				*old_memory,
	size_t				 old_size,
	size_t				 new_size,
	size_t				 alignment,
	void				*user_data
);

typedef struct mem_Allocator mem_Allocator;
struct mem_Allocator {
	// The callback function to be used by the allocator.
	//
	mem_Allocator_Fn fn;

	// Arbitrary context data for the allocator which allows it to carry
	// state across multiple invocations. It must be casted to some concrete
	// type within the callback function itself.
	//
	void *user_data;
};

#define mem_raw_make(error, size, align, allocator) \
	((allocator).fn(error, MEM_NEW, NULL, 0, size, align, (allocator).user_data))

#define mem_raw_resize(error, memory, old_size, new_size, align, allocator) \
	((allocator).fn(error, MEM_RESIZE, memory, old_size, new_size, align, (allocator).user_data))

#define mem_raw_delete(error, memory, count, size, allocator) \
	(void)((allocator).fn(error, MEM_FREE, memory, (count) * (size), 0, (allocator).user_data))

#define mem_raw_new(error, size, align, allocator)	 mem_raw_make(error, 1, size, align, allocator)
#define mem_raw_free(error, memory, size, allocator) mem_raw_delete(error, memory, 1, size, allocator)

#define mem_new(T, error, allocator)		 mem_make(T, 1, allocator)
#define mem_free(error, memory, allocator)	 mem_delete(error, memory, 1, allocator)
#define mem_make(T, error, count, allocator) mem_resize(T, error, NULL, 0, count, allocator)
#define mem_resize(T, error, memory, old_count, new_count, allocator) \
	(T *)(mem_raw_resize(error, memory, sizeof(T) * (old_count), sizeof(T) * (new_count), alignof(T), allocator)

#define mem_delete(error, memory, count, allocator) \
	mem_raw_free(error, memory, sizeof(*(memory)) * (count), allocator)

#ifndef MEM_NO_STDC_ALLOCATOR

extern const mem_Allocator mem_stdc_allocator;

#ifdef MEM_H_IMPLEMENTATION

#include <stdlib.h>

static void *
mem_stdc_allocator_fn(
	mem_Allocator_Error *error,
	mem_Allocator_Mode   mode,
	void			    *old_memory,
	size_t				 old_size,
	size_t				 new_size,
	size_t				 alignment,
	void				*user_data,
) {
	void *new_memory = NULL;

	switch (mode) {
	case MEM_NEW:
	case MEM_RESIZE:
		new_memory = realloc(old_memory, new_size);
		if (new_memory == NULL) {
			if (error != NULL) {
				*error = MEM_OUT_OF_MEMORY;
			}
		} else if (new_size > old_size) {
			unsigned char *growth_begin;
			size_t growth_size;

			// Zero-initialize the unoccupied region.
			growth_begin = cast(unsigned char *)new_memory + old_size;
			growth_size  = new_size - old_size;
			memset(growth_begin, 0, growth_size);
		}
		break;

	case MEM_NEW_NONZERO:
	case MEM_RESIZE_NONZERO:
		new_memory = realloc(old_memory, new_size);
		if (new_memory == NULL && error != NULL) {
			*error = MEM_OUT_OF_MEMORY;
		}
		break;
	
	case MEM_FREE:
		free(old_memory);
		break;
	
	default:
		if (error != NULL) {
			*error = MEM_NOT_IMPLEMENTED;
		}
		return old_memory;
	}
	return new_memory;
}

const mem_Allocator mem_stdc_allocator = {mem_stdc_allocator_fn, NULL};

#endif // MEM_H_IMPLEMENTATION

#endif // !MEM_NO_STDC_ALLOCATOR

#endif // MEM_H
