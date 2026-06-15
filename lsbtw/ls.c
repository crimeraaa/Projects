// stfu microslop
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

// standard
#include <windows.h>
#include <stdio.h>
#include <string.h>

// local
#include "../common.h"

internal bool
error(const char *name)
{
    DWORD id = GetLastError();
    char buf[256];

    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessage?redirectedfrom=MSDN
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_IGNORE_INSERTS,
            /*lpSource    =*/NULL, // Unused with FORMAT_MESSAGE_FROM_SYSTEM.
            /*dwMessageId =*/id,
            /*dwLanguageId=*/MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            /*lpBuffer    =*/buf,
            /*nSize       =*/sizeof(buf),
            /*Arguments   =*/NULL);
    printf("[ERROR] %s() failed: %s", name, buf);
    return false;
}

internal bool
wildcard_appended(char *buf, size_t buf_len, const char *arg)
{
    size_t arg_len = strlen(arg);
    // Remove trailing '\\' from our view.
    if (arg[arg_len - 1] == '\\') {
        arg_len -= 1;
    }

    // Adding '\\', '*', and '\0' would overflow?
    char wildcard[] = "\\*";
    if (arg_len > buf_len - sizeof(wildcard)) {
        return false;
    }
    memcpy(buf, arg, arg_len);
    memcpy(&buf[arg_len], wildcard, sizeof(wildcard));
    return true;
}

enum Unit {
    UNIT_BYTE,
    UNIT_KILOBYTE,
    UNIT_MEGABYTE,
    UNIT_GIGABYTE,

    // Not an actual enum member.
    UNIT_COUNT,
};

internal const char
UNIT_SUFFIXES[UNIT_COUNT] = {'B', 'K', 'M', 'G'};

enum Factor {
    FACTOR_BINARY = 1024,
    FACTOR_METRIC = 1000,
};

struct Size {
    enum Unit unit;
    int data[UNIT_COUNT];
};

internal size_t
size_set(struct Size *sz, enum Unit unit, size_t count, size_t factor)
{
    size_t rest  = count / factor;
    size_t units = count % factor;
    sz->data[unit] = cast(int)units;
    return rest;
}

internal enum Unit
size_init_metric(struct Size *sz, size_t byte_count)
{
    enum Unit unit = UNIT_BYTE;
    size_t count = byte_count;
    for (; unit < UNIT_COUNT; unit += 1) {
        // Conceptually, after the first iteration, we are no longer
        // referring to the byte count. We refer to the kilobyte count,
        // megabyte count, and so on.
        count = size_set(sz, unit, count, FACTOR_METRIC);
        if (count == 0) {
            break;
        }
    }
    return unit;
}

internal const char *
unit_string(char *buf, size_t buf_len, size_t byte_count)
{
    struct Size sz;
    enum Unit unit = size_init_metric(&sz, byte_count);
    if (unit == UNIT_BYTE) {
        snprintf(buf, buf_len, "% 7i", sz.data[unit]);
    } else {
        char suffix = UNIT_SUFFIXES[unit];
        int  frac   = sz.data[unit - 1] / 100;
        snprintf(buf, buf_len, "% 4i.%i%c", sz.data[unit], frac, suffix);
    }
    return buf;
}

internal bool
files_listed(const char *arg)
{
    char path[MAX_PATH];
    WIN32_FIND_DATAA data;
    HANDLE search;

    // Necessary to allow expansion by Find{First,Next}FileA().
    if (!wildcard_appended(path, sizeof(path), arg)) {
        printf("[ERROR] Overly long path '%.32s...'\n", arg);
        return false;
    }

    search = FindFirstFileA(path, &data);
    if (search == INVALID_HANDLE_VALUE) {
        return error("FindFirstFileA");
    }

    // List all the files in the given directory along with some other info.
    printf("listing of %s:\n", arg);
    do {
        bool is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!is_dir) {
            char buf[64];
            LARGE_INTEGER sz;
            sz.LowPart  = data.nFileSizeLow;
            sz.HighPart = data.nFileSizeHigh;
            printf("%s", unit_string(buf, sizeof(buf), cast(size_t)sz.QuadPart));
        } else {
            printf("%-7s", " ");
        }
        printf(" %s%s\n", data.cFileName, is_dir ? "\\" : "");
    } while (FindNextFileA(search, &data));
    
    if (!FindClose(search)) {
        return error("FindClose");
    }
    return true;
}

int
main(int argc, char *argv[])
{
    // Add the wildcard to ensure FindFirstFile() will expand the directory.
    if (argc == 1) {
        files_listed(".");
    } else {
        for (int i = 1; i < argc; i += 1) {
            if (i > 1) {
                printf("\n");
            }
            if (!files_listed(argv[i])) {
                return 1;
            }
        }
    }
    return 0;
}
