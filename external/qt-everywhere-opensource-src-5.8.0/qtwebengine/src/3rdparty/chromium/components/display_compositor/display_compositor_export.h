// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_EXPORT_H_
#define COMPONENTS_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DISPLAY_COMPOSITOR_IMPLEMENTATION)
#define DISPLAY_COMPOSITOR_EXPORT __declspec(dllexport)
#else
#define DISPLAY_COMPOSITOR_EXPORT __declspec(dllimport)
#endif  // defined(DISPLAY_COMPOSITOR_IMPLEMENTATION)

#else  // !defined(WIN32)

#if defined(DISPLAY_COMPOSITOR_IMPLEMENTATION)
#define DISPLAY_COMPOSITOR_EXPORT __attribute__((visibility("default")))
#else
#define DISPLAY_COMPOSITOR_EXPORT
#endif  // defined(DISPLAY_COMPOSITOR_IMPLEMENTATION)

#endif  // defined(WIN32)

#else  // !defined(COMPONENT_BUILD)

#define DISPLAY_COMPOSITOR_EXPORT

#endif  // define(COMPONENT_BUILD)

#endif  // COMPONENTS_DISPLAY_COMPOSITOR_DISPLAY_COMPOSITOR_EXPORT_H_
