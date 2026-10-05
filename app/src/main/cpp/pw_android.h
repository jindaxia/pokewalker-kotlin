#ifndef PW_ANDROID_H
#define PW_ANDROID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "picowalker_structures.h"

/*
 * Internal interfaces shared between the Android driver modules,
 * the JNI bridge and the core loop runner.
 */

#define PW_ANDROID_FB_PIXELS (PW_SCREEN_WIDTH * PW_SCREEN_HEIGHT)

/* dr_screen.c - convert the 2bpp-index framebuffer to ARGB8888.
 * Returns true (and fills `out`, PW_ANDROID_FB_PIXELS entries) when the
 * frame changed since the previous call. */
bool pw_and_screen_take_frame(uint32_t *out);

/* jni_bridge.c - push a converted frame to the Kotlin side. */
void pw_and_notify_frame(const uint32_t *pixels, size_t n);
void pw_and_jni_attach(void);
void pw_and_jni_detach(void);

/* dr_eeprom.c */
void pw_and_eeprom_set_path(const char *path);
void pw_and_eeprom_free(void);

/* dr_accel.c - fed by the Android step sensors / manual input. */
void pw_and_accel_add_steps(uint32_t n);

/* dr_buttons.c - touch input from Kotlin. */
void pw_and_button_event(pw_buttons_t b, bool pressed);

/* dr_power.c - battery info from Kotlin. */
void pw_and_power_set_battery(uint8_t percent, bool charging, bool plugged);

/* dr_ir.c - TCP bridge configuration from Kotlin. */
void pw_and_ir_set_config(const char *host, int port, bool enabled);
int pw_and_ir_status(void); /* 0=off, 1=configured/disconnected, 2=connected */
void pw_and_ir_shutdown(void);

/* runner.c */
bool pw_and_runner_start(void);
bool pw_and_runner_stop(void);
bool pw_and_runner_running(void);

#endif /* PW_ANDROID_H */
