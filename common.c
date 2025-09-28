#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include "common.h"

noreturn void syserr(const char* fmt, ...) {
    va_list fmt_args;
    const int org_errno = errno;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    fprintf(stderr, " (%d; %s)\n", org_errno, strerror(org_errno));
    exit(1);
}

noreturn void fatal(const char* fmt, ...) {
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    fprintf(stderr, "\n");
    exit(1);
}

void error(const char* fmt, ...) {
    va_list fmt_args;
    const int org_errno = errno;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    if (org_errno != 0) {
        fprintf(stderr, " (%d; %s)", org_errno, strerror(org_errno));
    }
    fprintf(stderr, "\n");
}

void error_msg(const network_info *net, const size_t len) {
    char tmp[32]; /* Enough space for 10 bytes in hex + spacing */
    const size_t to_print = len < 10 ? len : 10;
    size_t offset = 0;

    for (size_t i = 0; i < to_print; i++) {
        offset += snprintf(tmp + offset, sizeof(tmp) - offset, "%02x", net->buffer[i]);
    }

    fprintf(stderr, "ERROR MSG %s\n", tmp);
}

static uint64_t get_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        syserr("clock_gettime failed");
    }
    // Convert to milliseconds: seconds * 1000 + nanoseconds / 1000000
    return (uint64_t)ts.tv_sec * SECOND + (uint64_t)ts.tv_nsec / 1000000;
}

uint64_t get_time(const time_info *time) {
    return get_time_ms() - time->start_time;
}

void init_timer(time_info *time) {
    time->start_time = get_time_ms();
}

uint64_t get_synced_time(const time_info *time) {
    return get_time(time) - time->offset;
}

void set_time_offset(time_info *time, const int64_t new_offset) {
    time->offset = new_offset;
}
