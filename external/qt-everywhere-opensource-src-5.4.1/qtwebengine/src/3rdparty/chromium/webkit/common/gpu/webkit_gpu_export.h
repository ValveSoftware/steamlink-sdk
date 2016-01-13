// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_GPU_WEBKIT_GPU_EXPORT_H_
#define WEBKIT_COMMON_GPU_WEBKIT_GPU_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WEBKIT_GPU_IMPLEMENTATION)
#define WEBKIT_GPU_EXPORT __declspec(dllexport)
#else
#define WEBKIT_GPU_EXPORT __declspec(dllimport)
#endif  // defined(WEBKIT_GPU_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(WEBKIT_GPU_IMPLEMENTATION)
#define WEBKIT_GPU_EXPORT __attribute__((visibility("default")))
#else
#define WEBKIT_GPU_EXPORT
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define WEBKIT_GPU_EXPORT
#endif

#endif  // WEBKIT_COMMON_GPU_WEBKIT_GPU_EXPORT_H_
