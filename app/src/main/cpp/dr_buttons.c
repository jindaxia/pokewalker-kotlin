#include <stdbool.h>

#include "buttons.h"
#include "picowalker_structures.h"

#include "pw_android.h"

static volatile uint32_t pressed_mask = 0;

void pw_button_init() {
    pressed_mask = 0;
}

bool pw_button_is_pressed(pw_buttons_t b) {
    return (pressed_mask & (uint32_t)b) != 0;
}

/*
 * Called from the UI thread on touch down/up.
 * On a new press the core's input callback fires, like the GPIO IRQ on the
 * real hardware.
 */
void pw_and_button_event(pw_buttons_t b, bool pressed) {
    uint32_t mask = (uint32_t)b;

    if (pressed) {
        bool was_pressed = (pressed_mask & mask) != 0;
        pressed_mask |= mask;
        if (!was_pressed) {
            pw_button_callback(b);
        }
    } else {
        pressed_mask &= ~mask;
    }
}
