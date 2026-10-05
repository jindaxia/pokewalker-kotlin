#include <string.h>

#include "flash.h"
#include "picowalker_structures.h"

/* Generated from the original Pokewalker flash dump (rom/flash_images.bin). */
extern uint8_t pw_flash_images[];

static const size_t flash_offsets[] = {0x0000, 0x0100, 0x0120, 0x0140, 0x0160, 0x0170, 0x0180};
static const size_t flash_sizes[] = {0x0100, 0x0020, 0x0020, 0x0020, 0x0010, 0x0010, 0x0010};

void pw_flash_read(pw_flash_img_t img_index, uint8_t *buf) {
    size_t idx = (size_t)img_index;
    if (idx >= sizeof(flash_offsets) / sizeof(flash_offsets[0])) return;

    memcpy(buf, &pw_flash_images[flash_offsets[idx]], flash_sizes[idx]);
}
