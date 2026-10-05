#include <time.h>

#include "picowalker_structures.h"
#include "timer.h"

/*
 * Time driver: monotonic clock for intervals, wall clock for the RTC.
 * The RTC epoch used by the walker is 2000-01-01 00:00:00 UTC.
 */

#define UNIX_TO_PW_EPOCH 946684800u /* seconds between 1970-01-01 and 2000-01-01 */

static uint64_t ts_to_us(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 1000000ull + (uint64_t)ts->tv_nsec / 1000ull;
}

uint64_t pw_time_get_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts_to_us(&ts);
}

uint64_t pw_time_get_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

pw_dhms_t pw_time_get_dhms() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    /* Use local time so the walker's day/hour boundaries match the user. */
    struct tm tmv;
    localtime_r(&ts.tv_sec, &tmv);

    pw_dhms_t dhms;
    dhms.days = (uint16_t)((ts.tv_sec + tmv.tm_gmtoff) / 86400);
    dhms.hours = (uint8_t)tmv.tm_hour;
    dhms.minutes = (uint8_t)tmv.tm_min;
    dhms.seconds = (uint8_t)tmv.tm_sec;
    return dhms;
}

uint32_t pw_time_get_rtc() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    if ((uint64_t)ts.tv_sec < (uint64_t)UNIX_TO_PW_EPOCH) return 0;
    return (uint32_t)((uint64_t)ts.tv_sec - (uint64_t)UNIX_TO_PW_EPOCH);
}

void pw_time_init_rtc(uint32_t last_sync) {
    (void)last_sync;
}

void pw_time_set_rtc(uint32_t rtc) {
    (void)rtc;
    /* Cannot set the system clock from an app; the wall clock is used directly. */
}

void pw_time_delay_ms(uint32_t ms) {
    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

void pw_time_delay_us(uint32_t us) {
    struct timespec ts = {.tv_sec = us / 1000000, .tv_nsec = (long)(us % 1000000) * 1000L};
    nanosleep(&ts, NULL);
}
