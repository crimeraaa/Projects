#ifndef LULU_VM_H
#define LULU_VM_H

#include "lulu.h"
#include "chunk.h"

LULU_INTERNAL_FUNC void
vm_execute(lulu_State *L, Chunk *c);

#endif // LULU_VM_H
