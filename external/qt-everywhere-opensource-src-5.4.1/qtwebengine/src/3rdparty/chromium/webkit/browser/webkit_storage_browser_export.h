// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_WEBKIT_STORAGE_BROWSER_EXPORT_H_
#define WEBKIT_BROWSER_WEBKIT_STORAGE_BROWSER_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WEBKIT_STORAGE_BROWSER_IMPLEMENTATION)
#define WEBKIT_STORAGE_BROWSER_EXPORT __declspec(dllexport)
#define WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE __declspec(dllexport)
#else
#define WEBKIT_STORAGE_BROWSER_EXPORT __declspec(dllimport)
#define WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(BROWSER_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(WEBKIT_STORAGE_BROWSER_IMPLEMENTATION)
#define WEBKIT_STORAGE_BROWSER_EXPORT __attribute__((visibility("default")))
#define WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define WEBKIT_STORAGE_BROWSER_EXPORT
#define WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define WEBKIT_STORAGE_BROWSER_EXPORT
#define WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE
#endif

#endif  // WEBKIT_BROWSER_WEBKIT_STORAGE_BROWSER_EXPORT_H_
