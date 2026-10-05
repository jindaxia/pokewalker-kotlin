#include <android/log.h>
#include <stdarg.h>

#include "debug_log.h"

void pw_log_vprintf(const char *fmt, va_list list) {
    __android_log_vprint(ANDROID_LOG_DEBUG, "picowalker", fmt, list);
}
