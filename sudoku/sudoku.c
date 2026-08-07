// standard
#include <stdio.h>  // printf
#include <string.h> // memset

// local
#include "sudoku.h"

#define cast(T)     (T)

static char const sample[] =
    "  5 1  98"
    "6 9      "
    " 4 2   56"
    "286 4 53 "
    "5  39    "
    "    52   "
    "3614 89  "
    "8  573 64"
    "4      2 "
;

//https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
#define ESC "\x1b"
#define CURSOR_PREVLINE(n)   ESC "[" n "F"
#define CURSOR_BACKWARD(n) ESC "[" n "D"

int
main(void)
{
    sudoku_Game G;
    memset(&G, 0, sizeof(G));
    sudoku_init_string(&G, sample, sizeof(sample) - 1);

    for (;;) {
        int c, lines;

        lines = sudoku_print(&G) + 1;
        printf("(lines + 1 = %i) Continue? (y/n) ", lines);
        c = fgetc(stdin);
        switch (c) {
        case 'y':
        case 'Y':
            // Remove all succeeding input.
            do {
                c = fgetc(stdin);
                if (c == '\n') {
                    break;
                }
            } while (1);
            break;
        case 'n':
        case 'N':
        case EOF:
            fputc('\n', stdout);
            goto gtfo;
        default:
            fprintf(stdout, "Unknown option '%c'.\n", c);
            goto gtfo;
        }
        fprintf(stdout, CURSOR_PREVLINE("%i"), lines);
    }
gtfo:
    return 0;
}

static SUDOKU_LIMB_TYPE *
sudoku_get_limb(sudoku_Game *G, int row, int col, SUDOKU_LIMB_TYPE *bit_index)
{
    SUDOKU_LIMB_TYPE row_offset, cell_index, limb_index;

    // Assume a column-major representation.
    row_offset  = cast(SUDOKU_LIMB_TYPE)row * SUDOKU_ROWS;

    // Convert 2-dimensional coordinates into a 1-dimensional index.
    // This should be in the inclusive range [0,81].
    cell_index  = row_offset + cast(SUDOKU_LIMB_TYPE)col;

    // Each cell N-bits long, the cell's starting bit
    // index is a multiple of it.
    cell_index *= SUDOKU_CELL_BITS;

    // Since we divide the (N*R*C)-bit array into 'limbs' (i.e. smaller-sized
    // integers that compose a larger one), determine which one we live in.
    limb_index  = cell_index / SUDOKU_LIMB_BITS;

    // Of that limb, determine which bit we start at.
    // Modulo by power of 2 is faster using bitwise AND.
    *bit_index  = cell_index & (SUDOKU_LIMB_BITS - 1);
    return &G->limbs[limb_index];
}

int
sudoku_get(sudoku_Game *G, int row, int col)
{
    SUDOKU_LIMB_TYPE *limb, bit_index;
    limb = sudoku_get_limb(G, row, col, &bit_index);
    return cast(int)((*limb >> bit_index) & SUDOKU_CELL_MASK);
}

void
sudoku_set(sudoku_Game *G, int row, int col, int value)
{
    SUDOKU_LIMB_TYPE *limb, bit_index, mask_in, mask_out;

    limb     = sudoku_get_limb(G, row, col, &bit_index);
    mask_in  = cast(SUDOKU_LIMB_TYPE)value << bit_index;

    // Use this to clear out the original value without messing up
    // any of the other bits.
    mask_out = ~(cast(SUDOKU_LIMB_TYPE)SUDOKU_CELL_MASK << bit_index);

    // Clear out the original value's bits, without messing up any of the
    // other ones, before setting the new value into place.
    *limb = (*limb & mask_out) | mask_in;
}

void
sudoku_copy(sudoku_Game *G, sudoku_Game const *other)
{
    memcpy(G, other, sizeof(*G));
}

#if defined(__GNUC__)
#define SUDOKU_UNREACHABLE()    __builtin_unreachable()
#elif defined(_MSC_VER)
#define SUDOKU_UNREACHABLE()    __assume(0)
#endif

static int
sudoku_char2int(char c)
{
    switch (c) {
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    }
    SUDOKU_UNREACHABLE();
    return 0;
}

static char
sudoku_int2char(int cell)
{
    switch (cell) {
    case 0: return ' ';
    case 1: return '1';
    case 2: return '2';
    case 3: return '3';
    case 4: return '4';
    case 5: return '5';
    case 6: return '6';
    case 7: return '7';
    case 8: return '8';
    case 9: return '9';
    }
    SUDOKU_UNREACHABLE();
    return 0;
}

void
sudoku_init_string(sudoku_Game *G, char const *s, size_t n)
{
    size_t row = 0, col = 0;
    for (size_t i = 0; i < n; i++) {
        int cell = sudoku_char2int(s[i]);
        sudoku_set(G, row, col, cell);

        col += 1;
        if (col >= SUDOKU_COLUMNS) {
            row += 1;
            col  = 0;
        }
    }
}


typedef struct sudoku_Range sudoku_Range;
struct sudoku_Range {
    int start, stop;
};

static sudoku_Range
sudoku_make_range(int index)
{
    sudoku_Range range = {0, 0};
    switch (index) {
    case 0:
    case 1:
    case 2: range.start = 0; break;
    case 3:
    case 4:
    case 5: range.start = 3; break;
    case 6:
    case 7:
    case 8: range.start = 6; break;
    default:
        SUDOKU_UNREACHABLE();
        return range;
    }

    range.stop = range.start + SUDOKU_BOX_LENGTH;
    return range;
}

static int
sudoku_cell_is_valid(sudoku_Game *G, int const row, int const col)
{
    int curr_cell = sudoku_get(G, row, col);
    // Empty cells are implicitly valid since they can't compare to anything.
    if (!curr_cell) {
        return 1;
    }

    // In this row, check all its child columns for conflicts.
    for (int i = 0; i < SUDOKU_COLUMNS; i++) {
        // Ignore ourselves, because we're obviously equal to it!
        if (i == col) {
            continue;
        }

        int row_cell = sudoku_get(G, row, i);
        if (curr_cell == row_cell) {
            return 0;
        }
    }

    // In this column, check all its child rows for conflicts.
    for (int j = 0; j < SUDOKU_ROWS; j++) {
        if (j == row) {
            continue;
        }

        int col_cell = sudoku_get(G, j, col);
        if (curr_cell == col_cell) {
            return 0;
        }
    }

    sudoku_Range box_row, box_col;
    box_row = sudoku_make_range(row);
    box_col = sudoku_make_range(col);
    for (int i = box_row.start; i < box_row.stop; i++) {
        for (int j = box_col.start; j < box_col.stop; j++) {
            if (i == row && j == col) {
                continue;
            }

            int box_cell = sudoku_get(G, i, j);
            if (curr_cell == box_cell) {
                return 0;
            }
        }
    }
    return 1;
}

int
sudoku_is_valid(sudoku_Game *S)
{
    for (int row = 0; row < SUDOKU_ROWS; row++) {
        for (int col = 0; col < SUDOKU_COLUMNS; col++) {
            if (!sudoku_cell_is_valid(S, row, col)) {
                return 0;
            }
        }
    }
    return 1;
}

void
sudoku_solve(sudoku_Game *G)
{

}


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
int
sudoku_print(sudoku_Game *G)
{
    char buf[] = 
    "┌───┬───┬───┰───┬───┬───┰───┬───┬───┐\n"
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n"
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n"
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n"
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n"
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n"
    "┝━━━┿━━━┿━━━╋━━━┿━━━┿━━━╋━━━┿━━━┿━━━┥\n"
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n"
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n"
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n"
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n"
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n"
    "┝━━━┿━━━┿━━━╋━━━┿━━━┿━━━╋━━━┿━━━┿━━━┥\n"
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n"
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n"
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n"
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n"
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n"
    "└───┴───┴───┸───┴───┴───┸───┴───┴───┘\n";

    char *iter = buf;
    for (int row = 0; row < SUDOKU_ROWS; row++) {
        for (int col = 0; col < SUDOKU_COLUMNS; col++) {
            while (*iter != 0 && *iter != 'x') {
                iter++;
            }
            *iter++ = sudoku_int2char(sudoku_get(G, row, col));
        }
    }
    printf("%s", buf);
    return 19;
}

#undef cast
