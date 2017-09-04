// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_JS_JS_EXPORT_H_
#define MOJO_EDK_JS_JS_EXPORT_H_

// Defines MOJO_JS_EXPORT so that functionality implemented by //mojo/edk/js can
// be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MOJO_JS_IMPLEMENTATION)
#define MOJO_JS_EXPORT __declspec(dllexport)
#else
#define MOJO_JS_EXPORT __declspec(dllimport)
#endif  // defined(MOJO_JS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MOJO_JS_IMPLEMENTATION)
#define MOJO_JS_EXPORT __attribute__((visibility("default")))
#else
#define MOJO_JS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MOJO_JS_EXPORT
#endif

#endif  // MOJO_EDK_JS_JS_EXPORT_H_
