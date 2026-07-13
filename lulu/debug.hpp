#pragma once

#include "internal.hpp"
#include "chunk.hpp"

LULU_INTERNAL_FUNC void
debug_disassemble(Chunk const *c);

LULU_INTERNAL_FUNC void
debug_disassemble_at(Chunk const *c, usize offset);

