// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BITMAP_UPLOADER_BITMAP_UPLOADER_EXPORT_H_
#define COMPONENTS_BITMAP_UPLOADER_BITMAP_UPLOADER_EXPORT_H_

// Defines BITMAP_UPLOADER_EXPORT so that functionality implemented by the
// bitmap_uploader module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(BITMAP_UPLOADER_IMPLEMENTATION)
#define BITMAP_UPLOADER_EXPORT __declspec(dllexport)
#else
#define BITMAP_UPLOADER_EXPORT __declspec(dllimport)
#endif  // defined(BITMAP_UPLOADER_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(BITMAP_UPLOADER_IMPLEMENTATION)
#define BITMAP_UPLOADER_EXPORT __attribute__((visibility("default")))
#else
#define BITMAP_UPLOADER_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define BITMAP_UPLOADER_EXPORT
#endif

#endif  // COMPONENTS_BITMAP_UPLOADER_BITMAP_UPLOADER_EXPORT_H_
