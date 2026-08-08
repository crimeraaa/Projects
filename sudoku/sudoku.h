#ifndef SUDOKU_H
#define SUDOKU_H

#include <limits.h>

/*=== BEGIN: CONFIGURABLE ================================================{{{ */

#define SUDOKU_LIMB_TYPE    unsigned int

/*
Refer to: https://en.wikipedia.org/wiki/Box-drawing_characters

             0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
    U+2500   ─   ━ 	 │	 ┃ 	 ┄	 ┅	 ┆	 ┇	 ┈	 ┉	 ┊	 ┋	 ┌	 ┍	 ┎	 ┏ 
    U+251x	 ┐	 ┑	 ┒	 ┓	 └	 ┕	 ┖	 ┗	 ┘	 ┙	 ┚	 ┛	 ├	 ┝	 ┞	 ┟
    U+252x	 ┠	 ┡	 ┢	 ┣	 ┤	 ┥	 ┦	 ┧	 ┨	 ┩	 ┪	 ┫	 ┬	 ┭	 ┮	 ┯
    U+253x	 ┰	 ┱	 ┲	 ┳	 ┴	 ┵	 ┶	 ┷	 ┸	 ┹	 ┺	 ┻	 ┼	 ┽	 ┾	 ┿
    U+254x	 ╀	 ╁	 ╂	 ╃	 ╄	 ╅	 ╆	 ╇	 ╈	 ╉	 ╊	 ╋	 ╌	 ╍	 ╎	 ╏
    U+255x	 ═	 ║	 ╒	 ╓	 ╔	 ╕	 ╖	 ╗	 ╘	 ╙	 ╚	 ╛	 ╜	 ╝	 ╞	 ╟
    U+256x	 ╠	 ╡	 ╢	 ╣	 ╤	 ╥	 ╦	 ╧	 ╨	 ╩	 ╪	 ╫	 ╬	 ╭	 ╮	 ╯
    U+257x	 ╰	 ╱	 ╲	 ╳	 ╴	 ╵	 ╶	 ╷	 ╸	 ╹	 ╺	 ╻	 ╼	 ╽	 ╾	 ╿
 */
#define SUDOKU_GRID_STRING                    \
    "┌───┬───┬───┰───┬───┬───┰───┬───┬───┐\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "┝━━━┿━━━┿━━━╋━━━┿━━━┿━━━╋━━━┿━━━┿━━━┥\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "┝━━━┿━━━┿━━━╋━━━┿━━━┿━━━╋━━━┿━━━┿━━━┥\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "└───┴───┴───┸───┴───┴───┸───┴───┴───┘\n" \

/*=== END:   CONFIGURABLE   ==============================================}}} */

typedef SUDOKU_LIMB_TYPE sudoku_Limb;

/* Cells are in the inclusive 4-bit range [0b0000, 0b1001] */
#define SUDOKU_CELL_BITS    4
#define SUDOKU_CELL_MIN     0
#define SUDOKU_CELL_MAX     9
#define SUDOKU_CELL_MASK    0xf

#define SUDOKU_LINE_LENGTH  9
#define SUDOKU_BOX_LENGTH   3
#define SUDOKU_BOX_HEIGHT   3
#define SUDOKU_ROWS         SUDOKU_LINE_LENGTH
#define SUDOKU_COLS         SUDOKU_LINE_LENGTH
#define SUDOKU_GRID_SIZE    (SUDOKU_ROWS * SUDOKU_COLS)
#define SUDOKU_GRID_BITS    (SUDOKU_GRID_SIZE * SUDOKU_CELL_BITS)
#define SUDOKU_LIMB_BITS    (sizeof(sudoku_Limb) * CHAR_BIT)

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

typedef struct sudoku_State sudoku_Game;
struct sudoku_State {
    /*
     Cells are stored in a column-major fashion.
     */
    sudoku_Limb grid_limbs[SUDOKU_LIMB_COUNT];

    /*
     Save how many newlines are stored in the string representation.
     This is mainly useful when working with ANSI escape sequences.
     */
    int  grid_line_count;

    /*
     Determine which indexes into the grid string represent our cells.
     This allows us to mutate them easily.
     */
    int  grid_indexes[SUDOKU_ROWS][SUDOKU_COLS];

    /*
     Fixed-size buffer for the string representation of the grid.
     */
    char grid_string[sizeof(SUDOKU_GRID_STRING)];
};

void
sudoku_init(sudoku_Game *G);

void
sudoku_init_string(sudoku_Game *G, char const *input, size_t input_len);

int
sudoku_get(sudoku_Game *G, int row, int col);

void
sudoku_set(sudoku_Game *G, int row, int col, int value);

int
sudoku_solve(sudoku_Game *G, int *step_count);

int
sudoku_is_valid(sudoku_Game *G);

int
sudoku_cell_is_valid(sudoku_Game *G, int row, int col, int value);

char const *
sudoku_to_string(sudoku_Game *G, size_t *n);

#endif /* SUDOKU_H */
