#include <stdarg.h> // va_*
#include <stdlib.h> // abort
#include <stdio.h>  // fprintf

#include "cpu.h"

#define X_EXPAND(x)             #x
#define X_STRINGIFY(x)          X_EXPAND(x)
#define SOURCE_CODE_LOCATION    __FILE__ ":" X_STRINGIFY(__LINE__)
#define LOG_MODE_LIST(X) \
    X(INFO), \
    X(WARN), \
    X(ERROR), \
    X(FATAL) \


enum log_Mode {
#define X(mode) LOG_##mode
    LOG_MODE_LIST(X),
#undef X
};


#ifdef EMU6502_DEBUG

void
log_printf(enum log_Mode mode, const char *fmt, ...)
{
    static const char *
    LOG_MODE_STRINGS[] = {
#define X(mode) #mode
        LOG_MODE_LIST(X),
#undef X
    };

    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%-5s] ", LOG_MODE_STRINGS[mode]);
    vfprintf(stderr, fmt, args);
    va_end(args);

    if (mode == LOG_FATAL) {
        abort();
    }
}

#define log_printf(mode, fmt, ...) \
    (log_printf)(mode, SOURCE_CODE_LOCATION ": " fmt "\n", __VA_ARGS__)

#else

#define log_printf(...)

#endif // !NDEBUG

#define log_infof(fmt, ...)  log_printf(LOG_INFO,  fmt, __VA_ARGS__)
#define log_warnf(fmt, ...)  log_printf(LOG_WARN,  fmt, __VA_ARGS__)
#define log_errorf(fmt, ...) log_printf(LOG_ERROR, fmt, __VA_ARGS__)
#define log_fatalf(fmt, ...) log_printf(LOG_FATAL, fmt, __VA_ARGS__)
