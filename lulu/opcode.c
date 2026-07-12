#include "opcode.h"

static OpForm const
OPCODE_FORMATS[] = {
#define X(e, s, f) OpForm_##f,
    OPCODE_KINDS(X)
#undef X
};

LULU_INTERNAL_FUNC OpForm
opform_get(OpCode op)
{
    return OPCODE_FORMATS[op];
}
