#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#include "eeprom.h"
#include "globals.h"
#include "picowalker_core.h"
#include "picowalker_structures.h"
#include "states.h"
#include "types.h"

#include "pw_android.h"

/*
 * Hosts picowalker-core on a dedicated thread: pw_setup() once, then the
 * core's own loop function, converting/pushing a frame whenever the screen
 * changed.
 */

/* globals from picowalker.c (non-static) */
extern pw_state_t *current_state;
extern pw_state_t *pending_state;

#define LOOP_SLEEP_MS 4

static volatile bool run_flag = false;
static pthread_t loop_thread;
static bool thread_alive = false;

/*
 * A fresh save has never talked to a game, so the core would sit forever on
 * the "first connect" screen waiting for IR. There is no way back to the
 * menu from there, so flag the walker as initialised and continue into the
 * splash screen. Users who import a real walker save are unaffected.
 */
static void skip_first_connect_if_needed(void) {
    if (walker_info_cache.flags & WALKER_INFO_FLAG_INIT) return;

    walker_info_cache.flags |= WALKER_INFO_FLAG_INIT;
    pw_eeprom_write_walker_info(&walker_info_cache);
    pending_state->sid = STATE_SPLASH;
    pw_log_info("Fresh save detected: skipping first-connect, entering splash\n");
}

static void *loop_main(void *arg) {
    (void)arg;

    pw_and_jni_attach();

    pw_setup();
    skip_first_connect_if_needed();

    uint32_t frame[PW_ANDROID_FB_PIXELS];

    while (run_flag) {
        pw_current_loop();

        if (pw_and_screen_take_frame(frame)) {
            pw_and_notify_frame(frame, PW_ANDROID_FB_PIXELS);
        }

        struct timespec ts = {.tv_sec = 0, .tv_nsec = LOOP_SLEEP_MS * 1000000L};
        nanosleep(&ts, NULL);
    }

    pw_and_ir_shutdown();
    pw_and_eeprom_free();

    pw_and_jni_detach();
    return NULL;
}

bool pw_and_runner_start(void) {
    if (thread_alive) return true;

    run_flag = true;
    if (pthread_create(&loop_thread, NULL, loop_main, NULL) != 0) {
        run_flag = false;
        return false;
    }
    thread_alive = true;
    return true;
}

bool pw_and_runner_stop(void) {
    if (!thread_alive) return true;

    run_flag = false;
    pthread_join(loop_thread, NULL);
    thread_alive = false;
    return true;
}

bool pw_and_runner_running(void) {
    return thread_alive;
}
