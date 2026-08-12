#ifndef TUI_WINDOWS_H
#define TUI_WINDOWS_H

/* stfu microslop */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinUser.h>

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

/*
 I am NOT using the `DWORD` typedef lil bro
 */
typedef BYTE  u8;
typedef WORD  u16;
typedef DWORD u32;

typedef signed char  i8;
typedef signed short i16;
typedef signed long  i32;

typedef struct tui_Windows tui_Windows;
struct tui_Windows {
    HANDLE     handle;
    CHAR_INFO *grid;
    CHAR_INFO *grid_end;
    COORD      grid_size;
};


#endif // !TUI_WINDOWS_H
