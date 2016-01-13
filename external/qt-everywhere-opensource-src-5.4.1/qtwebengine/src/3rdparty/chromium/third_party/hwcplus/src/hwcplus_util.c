// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <android/log.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>

#define LOG_BUF_SIZE 1024

static int default_log_fn(int prio, const char* tag, const char* msg);

static hwcplus_log_fn_t hwcplus_log_fn = default_log_fn;

void hwcplus_set_log_fn(hwcplus_log_fn_t fn) {
    hwcplus_log_fn = fn;
}

#ifndef HAVE_STRLCPY
size_t strlcpy(char* dst, const char* src, size_t siz) {
    char* d = dst;
    const char* s = src;
    size_t n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0')
                break;
        }
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (siz != 0)
            *d = '\0'; /* NUL-terminate dst */
        while (*s++) {
        }
    }

    return(s - src - 1); /* count does not include NUL */
}
#endif

static int default_log_fn(int prio, const char* tag, const char* msg) {
    fprintf(stderr, "<%d> %s %s\n", prio, tag, msg);
}

int __android_log_write(int prio, const char* tag, const char* msg) {
    hwcplus_log_fn(prio, tag, msg);
}

int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
    va_list ap;
    char buf[LOG_BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    return __android_log_write(prio, tag, buf);
}

void __android_log_assert(const char* cond,
                          const char* tag,
                          const char* fmt,
                          ...) {
    char buf[LOG_BUF_SIZE];

    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
        va_end(ap);
    } else {
        /* Msg not provided, log condition.  N.B. Do not use cond directly as
         * format string as it could contain spurious '%' syntax (e.g.
         * "%d" in "blocks%devs == 0").
         */
        if (cond)
            snprintf(buf, LOG_BUF_SIZE, "Assertion failed: %s", cond);
        else
            snprintf(buf, LOG_BUF_SIZE, "Unspecified assertion failed");
    }

    __android_log_write(ANDROID_LOG_FATAL, tag, buf);

    __builtin_trap(); /* trap so we have a chance to debug the situation */
}

int property_get(const char* key, char* value, const char* default_value) {
    printf("property_get %s\n", key);
    const char* r = default_value;
    if (!r)
        r = "";
    strncpy(value, r, PROPERTY_VALUE_MAX);
    return strlen(r);
}
