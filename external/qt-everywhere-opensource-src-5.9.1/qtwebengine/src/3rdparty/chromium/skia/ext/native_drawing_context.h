// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_NATIVE_DRAWING_CONTEXT_H_
#define SKIA_EXT_NATIVE_DRAWING_CONTEXT_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(USE_CAIRO)
typedef struct _cairo cairo_t;
#elif defined(OS_MACOSX)
typedef struct CGContext* CGContextRef;
#endif

namespace skia {

#if defined(OS_WIN)
typedef HDC NativeDrawingContext;
#elif defined(USE_CAIRO)
typedef cairo_t* NativeDrawingContext;
#elif defined(OS_MACOSX)
typedef CGContextRef NativeDrawingContext;
#else
typedef void* NativeDrawingContext;
#endif

}  // namespace skia

#endif  // SKIA_EXT_NATIVE_DRAWING_CONTEXT_H_
