#include <stdarg.h>     // va_list
#include <stdio.h>      // fprintf
#include <stdlib.h>     // abort

#define E6502_DEBUG                1
#define LOG_STRINGIFY(X)           #X
#define LOG_EXPAND_STRINGIFY(X)    LOG_STRINGIFY(X)

enum log_Mode {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
};

#if E6502_DEBUG

static void
(log_write)(enum log_Mode mode, const char *fmt, ...)
{
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define RESET   "\x1b[0m"
    static const char
    LOG_MODE_STRINGS[][16] = {
        "INFO",
        YELLOW "WARN"  RESET,
        RED    "ERROR" RESET,
        RED    "FATAL" RESET,
    };
#undef RESET
#undef YELLOW
#undef GREEN
#undef RED

    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%-5s] --- ", LOG_MODE_STRINGS[mode]);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

#define log_write(mode, fmt, ...) \
    (log_write)(mode, \
                __FILE__ ":" LOG_EXPAND_STRINGIFY(__LINE__) " " fmt "\n", \
                __VA_ARGS__)

#else // !E6502_DEBUG

#define log_write(...)

#endif // E6502_DEBUG

#define log_infof(fmt, ...)     log_write(LOG_INFO, fmt, __VA_ARGS__)
#define log_info(message)       log_infof("%s", message)

#define log_warnf(fmt, ...)     log_write(LOG_WARN, fmt, __VA_ARGS__)
#define log_warn(message)       log_warnf("%s", message)

#define log_errorf(fmt, ...)    log_write(LOG_ERROR, fmt, __VA_ARGS__)
#define log_error(message)      log_errorf("%s", message)

#define log_fatalf(fmt, ...)    log_write(LOG_FATAL, fmt, __VA_ARGS__)
#define log_fatal(message)      log_fatalf("%s", message)

#define log_assertf(expr, fmt, ...)                                            \
do {                                                                           \
    if (!(expr)) {                                                             \
        log_fatalf("Assertion '" #expr "' failed! (" fmt ")", __VA_ARGS__);    \
        abort();                                                               \
    }                                                                          \
} while (0)

#define log_assertln(expr, msg) log_assertf(expr, "%s", msg)
#define log_assert(expr)        log_assertln(expr, "Aborting")

#undef LOG_MODES_LIST
