// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJO_MOJO_DISPLAY_EXPORT_H_
#define UI_DISPLAY_MOJO_MOJO_DISPLAY_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(MOJO_DISPLAY_IMPLEMENTATION)
#define MOJO_DISPLAY_EXPORT __declspec(dllexport)
#else
#define MOJO_DISPLAY_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(MOJO_DISPLAY_IMPLEMENTATION)
#define MOJO_DISPLAY_EXPORT __attribute__((visibility("default")))
#else
#define MOJO_DISPLAY_EXPORT
#endif

#endif  // defined(WIN32)

#else  // !defined(COMPONENT_BUILD)
#define MOJO_DISPLAY_EXPORT
#endif

#endif  // UI_DISPLAY_MOJO_MOJO_DISPLAY_EXPORT_H_
