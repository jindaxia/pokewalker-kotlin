#include <stdbool.h>
#include <stdint.h>

#include "accel.h"
#include "timer.h"

#include "pw_android.h"

/*
 * Step counter fed by Android's step sensors (or manual input).
 * `pending_steps` accumulates between the core's sampling points.
 */

#define ACTIVITY_WINDOW_MS 3000

static volatile uint32_t pending_steps = 0;
static volatile uint64_t last_activity_ms = 0;

void pw_accel_init() {
    pending_steps = 0;
    last_activity_ms = 0;
}

void pw_accel_sleep() {}
void pw_accel_wake() {}

uint32_t pw_accel_get_new_steps() {
    return __atomic_exchange_n(&pending_steps, 0u, __ATOMIC_SEQ_CST);
}

uint8_t pw_accel_get_activity() {
    if (last_activity_ms == 0) return 0;
    return (pw_time_get_ms() - last_activity_ms < ACTIVITY_WINDOW_MS) ? 1 : 0;
}

void pw_and_accel_add_steps(uint32_t n) {
    if (n == 0) return;
    __atomic_fetch_add(&pending_steps, n, __ATOMIC_SEQ_CST);
    last_activity_ms = pw_time_get_ms();
}
