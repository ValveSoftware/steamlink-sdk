// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebExport_h
#define WebExport_h

#include "wtf/Compiler.h"

namespace blink {

// This macro is intended to export symbols in Source/web/ which are still
// private to Blink (for instance, because they are used in unit tests).

#if defined(COMPONENT_BUILD)
#if defined(WIN32)
#if BLINK_WEB_IMPLEMENTATION
#define WEB_EXPORT __declspec(dllexport)
#else
#define WEB_EXPORT __declspec(dllimport)
#endif // BLINK_WEB_IMPLEMENTATION
#else // defined(WIN32)
#define WEB_EXPORT __attribute__((visibility("default")))
#endif
#else // defined(COMPONENT_BUILD)
#define WEB_EXPORT
#endif

} // namespace blink

#endif // WebExport_h
