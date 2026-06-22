#include "libultraship/log/luslog.h"

#include <stdio.h>
#include <stdarg.h>

// Lean log sink: write to stdout (the libogc console / Dolphin log). No spdlog,
// no file rotation; level and source location are accepted but not formatted in.
void luslog(const char* file, int32_t line, int32_t logLevel, const char* msg) {
    (void)file;
    (void)line;
    (void)logLevel;
    printf("%s\n", msg);
}

void lusprintf(const char* file, int32_t line, int32_t logLevel, const char* fmt, ...) {
    (void)file;
    (void)line;
    (void)logLevel;
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}
