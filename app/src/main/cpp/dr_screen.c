#include <string.h>

#include "picowalker_structures.h"
#include "screen.h"

#include "pw_android.h"

/*
 * Software framebuffer driver: 96x64 pixels, one byte per pixel holding the
 * original Pokewalker 2bpp colour index (0=white .. 3=black).
 */

static uint8_t fb[PW_ANDROID_FB_PIXELS];
static volatile int fb_dirty = 1;

/* Same palette as picowalker-sdl (ARGB8888). */
static const uint32_t palette[4] = {0xffacaea4, 0xff7c7e74, 0xff5c5e54, 0xff2c2e24};

static inline void fb_put(int x, int y, uint8_t v) {
    if (x < 0 || x >= PW_SCREEN_WIDTH || y < 0 || y >= PW_SCREEN_HEIGHT) return;
    fb[y * PW_SCREEN_WIDTH + x] = v;
}

void pw_screen_init() {
    memset(fb, PW_SCREEN_WHITE, sizeof(fb));
    fb_dirty = 1;
}

void pw_screen_clear() {
    memset(fb, PW_SCREEN_WHITE, sizeof(fb));
    fb_dirty = 1;
}

void pw_screen_fill_area(pw_screen_pos_t x, pw_screen_pos_t y, pw_screen_dim_t w, pw_screen_dim_t h,
    pw_screen_color_t colour) {
    int x0 = x, y0 = y;
    int x1 = x0 + (int)w, y1 = y0 + (int)h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > PW_SCREEN_WIDTH) x1 = PW_SCREEN_WIDTH;
    if (y1 > PW_SCREEN_HEIGHT) y1 = PW_SCREEN_HEIGHT;

    for (int yy = y0; yy < y1; yy++) {
        uint8_t *row = &fb[yy * PW_SCREEN_WIDTH];
        for (int xx = x0; xx < x1; xx++) row[xx] = (uint8_t)colour;
    }
    fb_dirty = 1;
}

void pw_screen_clear_area(pw_screen_pos_t x, pw_screen_pos_t y, pw_screen_dim_t w, pw_screen_dim_t h) {
    pw_screen_fill_area(x, y, w, h, PW_SCREEN_WHITE);
}

void pw_screen_draw_horiz_line(
    pw_screen_pos_t x, pw_screen_pos_t y, pw_screen_dim_t len, pw_screen_color_t colour) {
    pw_screen_fill_area(x, y, len, 1, colour);
}

void pw_screen_draw_text_box(
    pw_screen_pos_t x, pw_screen_pos_t y, pw_screen_pos_t w, pw_screen_pos_t h, pw_screen_color_t colour) {
    if (w <= 0 || h <= 0) return;

    /* 1px border rectangle, same as the hardware driver */
    pw_screen_fill_area(x, y, w, 1, colour);                          /* top */
    pw_screen_fill_area(x, (pw_screen_pos_t)(y + h - 1), w, 1, colour); /* bottom */
    pw_screen_fill_area(x, y, 1, h, colour);                          /* left */
    pw_screen_fill_area((pw_screen_pos_t)(x + w - 1), y, 1, h, colour); /* right */
}

/*
 * Decode a Pokewalker image (2 bit planes, 2 bytes per 8 pixels) and blit it
 * into the framebuffer. Mirrors decode_img() from the hardware/SDL drivers:
 * the effective size is recomputed from the image dimensions.
 */
void pw_screen_draw_img(pw_img_t *img, pw_screen_pos_t x, pw_screen_pos_t y) {
    if (img == NULL || img->data == NULL || img->width == 0 || img->height == 0) return;

    size_t nbytes = (size_t)img->width * (size_t)img->height * 2 / 8;
    img->size = nbytes;

    for (size_t i = 0; i + 1 < nbytes; i += 2) {
        uint8_t upper = img->data[i];
        uint8_t lower = img->data[i + 1];

        for (size_t j = 0; j < 8; j++) {
            uint8_t pixel = (uint8_t)((((upper >> j) & 1) << 1) | ((lower >> j) & 1));

            size_t col = (i / 2) % img->width;
            size_t row = 8 * (i / (2 * img->width)) + j;
            if (row >= img->height) continue;

            fb_put((int)x + (int)col, (int)y + (int)row, pixel);
        }
    }
    fb_dirty = 1;
}

void pw_screen_sleep() {}
void pw_screen_wake() {}

void pw_screen_set_brightness(uint8_t brightness) {
    (void)brightness;
    /* Phone screens handle brightness themselves. */
}

bool pw_and_screen_take_frame(uint32_t *out) {
    if (!fb_dirty) return false;
    fb_dirty = 0;

    for (size_t i = 0; i < PW_ANDROID_FB_PIXELS; i++) out[i] = palette[fb[i]];
    return true;
}
