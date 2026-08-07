#ifndef SUDOKU_H
#define SUDOKU_H

#include <limits.h>

/* Cells are in the inclusive 4-bit range [0b0000, 0b1001] */
#define SUDOKU_CELL_BITS    4
#define SUDOKU_CELL_MIN     0
#define SUDOKU_CELL_MAX     9
#define SUDOKU_CELL_MASK    0xf

#define SUDOKU_LINE_LENGTH  9
#define SUDOKU_BOX_LENGTH   3
#define SUDOKU_ROWS         SUDOKU_LINE_LENGTH
#define SUDOKU_COLUMNS      SUDOKU_LINE_LENGTH
#define SUDOKU_AREA         (SUDOKU_ROWS * SUDOKU_COLUMNS)

#define SUDOKU_GRID_BITS    (SUDOKU_AREA * SUDOKU_CELL_BITS)
#define SUDOKU_LIMB_BITS    (sizeof(SUDOKU_LIMB_TYPE) * CHAR_BIT)

/*
 We assume that integer division results in a floored quotient,
 so the grid's bit size divided by the limb's bit size is always
 one off. E.g:

 324 bits /  8 bits = (40 limbs / grid) *  8 bits = 320 bits
 324 bits / 16 bits = (20 limbs / grid) * 16 bits = 320 bits
 324 bits / 32 bits = (10 limbs / grid) * 32 bits = 320 bits
 324 bits / 64 bits =  (5 limbs / grid) * 64 bits = 320 bits

 So we always need one more limb to validly store the grid in its entirety.
 */
#define SUDOKU_LIMB_COUNT   ((SUDOKU_GRID_BITS / SUDOKU_LIMB_BITS) + 1)
#define SUDOKU_LIMB_TYPE    unsigned int

typedef struct sudoku_State sudoku_Game;
struct sudoku_State {
    /* Cells are stored in a column-major fashion. */
    SUDOKU_LIMB_TYPE limbs[SUDOKU_LIMB_COUNT];
};

void
sudoku_init_string(sudoku_Game *G, char const *text, size_t text_len);

void
sudoku_solve(sudoku_Game *G);

void
sudoku_copy(sudoku_Game *G, sudoku_Game const *other);

int
sudoku_get(sudoku_Game *G, int row, int col);

void
sudoku_set(sudoku_Game *G, int row, int col, int value);

int
sudoku_is_valid(sudoku_Game *G);

int
sudoku_print(sudoku_Game *G);

#endif /* SUDOKU_H */
