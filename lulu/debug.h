#ifndef LULU_DEBUG_H
#define LULU_DEBUG_H

#include "internal.h"
#include "chunk.h"

LULU_INTERNAL_FUNC void
debug_disassemble(const Chunk *c);

LULU_INTERNAL_FUNC void
debug_disassemble_at(const Chunk *c, usize offset);

#endif // !LULU_DEBUG_H
