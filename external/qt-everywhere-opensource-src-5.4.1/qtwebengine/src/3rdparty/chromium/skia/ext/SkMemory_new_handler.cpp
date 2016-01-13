// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <new>

#include "base/process/memory.h"

#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/core/SkThread.h"

// This implementation of sk_malloc_flags() and friends is identical to
// SkMemory_malloc.cpp, except that it disables the CRT's new_handler during
// malloc() and calloc() when SK_MALLOC_THROW is not set (because our normal
// new_handler itself will crash on failure when using tcmalloc).

SK_DECLARE_STATIC_MUTEX(gSkNewHandlerMutex);

static inline void* throw_on_failure(size_t size, void* p) {
    if (size > 0 && p == NULL) {
        // If we've got a NULL here, the only reason we should have failed is running out of RAM.
        sk_out_of_memory();
    }
    return p;
}

void sk_throw() {
    SkASSERT(!"sk_throw");
    abort();
}

void sk_out_of_memory(void) {
    SkASSERT(!"sk_out_of_memory");
    abort();
}

void* sk_realloc_throw(void* addr, size_t size) {
    return throw_on_failure(size, realloc(addr, size));
}

void sk_free(void* p) {
    if (p) {
        free(p);
    }
}

void* sk_malloc_throw(size_t size) {
    return throw_on_failure(size, malloc(size));
}

static void* sk_malloc_nothrow(size_t size) {
    // TODO(b.kelemen): we should always use UncheckedMalloc but currently it
    // doesn't work as intended everywhere.
#if  defined(LIBC_GLIBC) || defined(USE_TCMALLOC) || \
     (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_ANDROID)
    void* result;
    // It's the responsibility of the caller to check the return value.
    ignore_result(base::UncheckedMalloc(size, &result));
    return result;
#else
    // This is not really thread safe.  It only won't collide with itself, but we're totally
    // unprotected from races with other code that calls set_new_handler.
    SkAutoMutexAcquire lock(gSkNewHandlerMutex);
    std::new_handler old_handler = std::set_new_handler(NULL);
    void* p = malloc(size);
    std::set_new_handler(old_handler);
    return p;
#endif
}

void* sk_malloc_flags(size_t size, unsigned flags) {
    if (flags & SK_MALLOC_THROW) {
        return sk_malloc_throw(size);
    }
    return sk_malloc_nothrow(size);
}

void* sk_calloc_throw(size_t size) {
    return throw_on_failure(size, calloc(size, 1));
}

void* sk_calloc(size_t size) {
    // TODO(b.kelemen): we should always use UncheckedCalloc but currently it
    // doesn't work as intended everywhere.
#if  defined(LIBC_GLIBC) || defined(USE_TCMALLOC) || \
     (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_ANDROID)
    void* result;
    // It's the responsibility of the caller to check the return value.
    ignore_result(base::UncheckedCalloc(size, 1, &result));
    return result;
#else
    SkAutoMutexAcquire lock(gSkNewHandlerMutex);
    std::new_handler old_handler = std::set_new_handler(NULL);
    void* p = calloc(size, 1);
    std::set_new_handler(old_handler);
    return p;
#endif
}
