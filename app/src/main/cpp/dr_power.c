#include <stdbool.h>
#include <stdint.h>

#include "debug_log.h"
#include "picowalker_structures.h"
#include "power.h"
#include "timer.h"

#include "pw_android.h"

/*
 * Power driver: the phone's battery stands in for the walker's own cell and
 * the device never actually sleeps - Android owns that. A periodic RTC wake
 * reason is synthesised so the core runs its hourly/daily housekeeping.
 */

#define MEASUREMENT_INTERVAL_MS 15000
#define RTC_WAKE_INTERVAL_MS    1000

static volatile uint8_t battery_percent = 100;
static volatile bool battery_charging = false;
static volatile bool battery_plugged = false;

static uint64_t last_measurement_ms = 0;
static uint64_t last_rtc_wake_ms = 0;
static bool have_measurement = false;
static bool shutdown_logged = false;

/* last reported flag set, used to turn levels into one-shot events */
static uint8_t reported_events = 0;

void pw_power_init() {
    last_measurement_ms = 0;
    last_rtc_wake_ms = pw_time_get_ms();
    have_measurement = false;
    shutdown_logged = false;
    reported_events = 0;
}

void pw_power_start_measurement() {
    /* Battery readings are always available via the cached values. */
}

bool pw_power_result_available() {
    return true;
}

void pw_and_power_set_battery(uint8_t percent, bool charging, bool plugged) {
    battery_percent = percent;
    battery_charging = charging;
    battery_plugged = plugged;
}

pw_power_status_t pw_power_get_status() {
    pw_power_status_t st = {.flags = 0, .percent = battery_percent};
    uint64_t now = pw_time_get_ms();

    if (!have_measurement || now - last_measurement_ms >= MEASUREMENT_INTERVAL_MS) {
        st.flags |= PW_POWER_STATUS_FLAGS_MEASUREMENT;
        last_measurement_ms = now;
        have_measurement = true;
    }

    /* Turn level changes into the one-shot event flags power.c expects. */
    bool was_charging = (reported_events & PW_POWER_STATUS_FLAGS_CHARGING) != 0;
    bool was_plugged = (reported_events & PW_POWER_STATUS_FLAGS_PLUGGED) != 0;

    if (battery_charging && !was_charging) st.flags |= PW_POWER_STATUS_FLAGS_CHARGING;
    if (!battery_charging && was_charging) st.flags |= PW_POWER_STATUS_FLAGS_CHARGE_ENDED;
    if (battery_plugged && !was_plugged) st.flags |= PW_POWER_STATUS_FLAGS_PLUGGED;
    if (!battery_plugged && was_plugged) st.flags |= PW_POWER_STATUS_FLAGS_UNPLUGGED;

    reported_events = 0;
    if (battery_charging) reported_events |= PW_POWER_STATUS_FLAGS_CHARGING;
    if (battery_plugged) reported_events |= PW_POWER_STATUS_FLAGS_PLUGGED;

    return st;
}

void pw_battery_shutdown() {
    if (!shutdown_logged) {
        pw_log_warn("Core requested shutdown for low battery; staying alive on Android\n");
        shutdown_logged = true;
    }
}

bool pw_power_should_sleep() {
    /* Screen sleep / doze is handled by Android, never by the core. */
    return false;
}

pw_wake_reason_t pw_power_get_wake_reason() {
    uint64_t now = pw_time_get_ms();

    if (now - last_rtc_wake_ms >= RTC_WAKE_INTERVAL_MS) {
        last_rtc_wake_ms = now;
        return PW_WAKE_REASON_RTC;
    }
    return 0;
}

void pw_power_clear_wake_reason(pw_wake_reason_t wake_reason) {
    (void)wake_reason;
    /* RTC reasons are generated on demand, nothing to clear. */
}

void pw_power_enter_sleep() {
    /* Not reachable: pw_power_should_sleep() always returns false. */
}

void pw_power_enter_light_sleep() {}

void pw_power_light_sleep_for(uint32_t ms) {
    pw_time_delay_ms(ms);
}
