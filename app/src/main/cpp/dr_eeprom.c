#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eeprom.h"

#include "pw_android.h"

/*
 * 64KB EEPROM backed by a file in the app's private storage.
 * The file is rewritten on every write, exactly like picowalker-sdl.
 */

#define EEPROM_SIZE_BYTES (64 * 1024)

static uint8_t *eeprom_mem = NULL;
static char eeprom_path[1024] = {0};

void pw_and_eeprom_set_path(const char *path) {
    if (path == NULL) return;
    strncpy(eeprom_path, path, sizeof(eeprom_path) - 1);
    eeprom_path[sizeof(eeprom_path) - 1] = '\0';
}

static void eeprom_persist(void) {
    if (eeprom_mem == NULL || eeprom_path[0] == '\0') return;

    /* Write to a temp file then rename, so concurrent readers (export)
     * never observe a partially written save. */
    char tmp_path[sizeof(eeprom_path) + 4];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", eeprom_path);

    FILE *f = fopen(tmp_path, "wb");
    if (f == NULL) return;
    size_t n = fwrite(eeprom_mem, 1, EEPROM_SIZE_BYTES, f);
    fclose(f);

    if (n == EEPROM_SIZE_BYTES) {
        rename(tmp_path, eeprom_path);
    } else {
        remove(tmp_path);
    }
}

void pw_eeprom_init() {
    if (eeprom_mem == NULL) {
        eeprom_mem = (uint8_t *)malloc(EEPROM_SIZE_BYTES);
        if (eeprom_mem == NULL) return;
    }

    memset(eeprom_mem, 0, EEPROM_SIZE_BYTES);

    if (eeprom_path[0] == '\0') return;

    FILE *f = fopen(eeprom_path, "rb");
    if (f == NULL) {
        /* No save yet: leave it zeroed, pw_setup() will initialise it. */
        return;
    }
    size_t n = fread(eeprom_mem, 1, EEPROM_SIZE_BYTES, f);
    fclose(f);

    if (n != EEPROM_SIZE_BYTES) {
        /* Wrong size: treat as no save so the core can re-init. */
        memset(eeprom_mem, 0, EEPROM_SIZE_BYTES);
    }
}

void pw_and_eeprom_free(void) {
    if (eeprom_mem != NULL) {
        free(eeprom_mem);
        eeprom_mem = NULL;
    }
}

int pw_eeprom_read(pw_eeprom_addr_t addr, uint8_t *buf, size_t len) {
    if (eeprom_mem == NULL) return -1;
    if ((size_t)addr + len > EEPROM_SIZE_BYTES) return -1;

    memcpy(buf, &eeprom_mem[addr], len);
    return 0;
}

int pw_eeprom_write(pw_eeprom_addr_t addr, const uint8_t *buf, size_t len) {
    if (eeprom_mem == NULL) return -1;
    if ((size_t)addr + len > EEPROM_SIZE_BYTES) return -1;

    memcpy(&eeprom_mem[addr], buf, len);
    eeprom_persist();
    return 0;
}

void pw_eeprom_set_area(pw_eeprom_addr_t addr, uint8_t v, size_t len) {
    if (eeprom_mem == NULL) return;
    if ((size_t)addr + len > EEPROM_SIZE_BYTES) return;

    memset(&eeprom_mem[addr], v, len);
    eeprom_persist();
}

void pw_eeprom_sleep() {}
void pw_eeprom_wake() {}
