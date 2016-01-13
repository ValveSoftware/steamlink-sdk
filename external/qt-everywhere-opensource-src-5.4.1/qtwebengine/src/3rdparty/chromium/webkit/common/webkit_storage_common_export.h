// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_WEBKIT_STORAGE_COMMON_EXPORT_H_
#define WEBKIT_COMMON_WEBKIT_STORAGE_COMMON_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WEBKIT_STORAGE_COMMON_IMPLEMENTATION)
#define WEBKIT_STORAGE_COMMON_EXPORT __declspec(dllexport)
#else
#define WEBKIT_STORAGE_COMMON_EXPORT __declspec(dllimport)
#endif  // defined(WEBKIT_STORAGE_COMMON_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(WEBKIT_STORAGE_COMMON_IMPLEMENTATION)
#define WEBKIT_STORAGE_COMMON_EXPORT __attribute__((visibility("default")))
#else
#define WEBKIT_STORAGE_COMMON_EXPORT
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define WEBKIT_STORAGE_COMMON_EXPORT
#endif

#endif  // WEBKIT_COMMON_WEBKIT_STORAGE_COMMON_EXPORT_H_
