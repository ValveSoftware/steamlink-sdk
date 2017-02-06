// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLPAPER_EXPORT_H_
#define COMPONENTS_WALLPAPER_EXPORT_H_

// Defines WALLPAPER_EXPORT so that functionality implemented by the wallpaper
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WALLPAPER_IMPLEMENTATION)
#define WALLPAPER_EXPORT __declspec(dllexport)
#else
#define WALLPAPER_EXPORT __declspec(dllimport)
#endif  // defined(WALLPAPER_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(WALLPAPER_IMPLEMENTATION)
#define WALLPAPER_EXPORT __attribute__((visibility("default")))
#else
#define WALLPAPER_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define WALLPAPER_EXPORT
#endif

#endif  // COMPONENTS_WALLPAPER_EXPORT_H_
