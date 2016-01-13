// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_V2_PUBLIC_V2_EXPORT_H_
#define UI_V2_PUBLIC_V2_EXPORT_H_

// Defines V2_EXPORT so that functionality implemented by the V2 module can be
// exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(V2_IMPLEMENTATION)
#define V2_EXPORT __declspec(dllexport)
#else
#define V2_EXPORT __declspec(dllimport)
#endif  // defined(V2_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(V2_IMPLEMENTATION)
#define V2_EXPORT __attribute__((visibility("default")))
#else
#define V2_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define V2_EXPORT
#endif

#endif  // UI_V2_PUBLIC_V2_EXPORT_H_