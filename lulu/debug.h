#ifndef LULU_DEBUG_H
#define LULU_DEBUG_H

#include "internal.h"
#include "chunk.h"

LULU_INTERNAL_FUNC void
debug_disassemble(Chunk const *c);

LULU_INTERNAL_FUNC void
debug_disassemble_at(Chunk const *c, usize offset);

#endif // !LULU_DEBUG_H
