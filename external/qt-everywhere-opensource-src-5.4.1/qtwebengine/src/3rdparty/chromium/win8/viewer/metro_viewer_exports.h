// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRO_VIEWER_METRO_VIEWER_H_
#define METRO_VIEWER_METRO_VIEWER_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(METRO_VIEWER_IMPLEMENTATION)
#define METRO_VIEWER_EXPORT __declspec(dllexport)
#else
#define METRO_VIEWER_EXPORT __declspec(dllimport)
#endif  // defined(METRO_VIEWER_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(METRO_VIEWER_IMPLEMENTATION)
#define METRO_VIEWER_EXPORT __attribute__((visibility("default")))
#else
#define METRO_VIEWER_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define METRO_VIEWER_EXPORT
#endif

#endif  // METRO_VIEWER_METRO_VIEWER_H_
