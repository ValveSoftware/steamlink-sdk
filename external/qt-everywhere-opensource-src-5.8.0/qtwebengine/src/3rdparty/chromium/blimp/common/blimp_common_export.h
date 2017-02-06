// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_BLIMP_COMMON_EXPORT_H_
#define BLIMP_COMMON_BLIMP_COMMON_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(BLIMP_COMMON_IMPLEMENTATION)
#define BLIMP_COMMON_EXPORT __declspec(dllexport)
#else
#define BLIMP_COMMON_EXPORT __declspec(dllimport)
#endif  // defined(BLIMP_COMMON_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(BLIMP_COMMON_IMPLEMENTATION)
#define BLIMP_COMMON_EXPORT __attribute__((visibility("default")))
#else
#define BLIMP_COMMON_EXPORT
#endif  // defined(BLIMP_COMMON_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define BLIMP_COMMON_EXPORT
#endif

#endif  // BLIMP_COMMON_BLIMP_COMMON_EXPORT_H_
