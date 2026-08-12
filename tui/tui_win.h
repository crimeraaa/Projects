#ifndef TUI_WINDOWS_H
#define TUI_WINDOWS_H

/* stfu microslop */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h> /* BYTE, DWORD, COORD, HANDLE, WORD */

#include <stddef.h>  /* size_t */
#include <stdbool.h> /* bool, false, true */
#include <limits.h>  /* CHAR_BIT */

/*
 I am NOT using the `DWORD` typedef lil bro
 */
typedef BYTE      u8;
typedef WORD      u16;
typedef DWORD     u32;
typedef DWORDLONG u64;

typedef INT8  i8;
typedef INT16 i16;
typedef INT32 i32;
typedef INT64 i64;

typedef struct tui_Windows tui_Windows;
struct tui_Windows {
    HANDLE     handle;
    CHAR_INFO *grid;
    CHAR_INFO *grid_end;
    COORD      grid_size;
};

#endif // !TUI_WINDOWS_H
