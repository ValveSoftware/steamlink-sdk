// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines convertion table from YUV to RGB.

#ifndef MEDIA_BASE_SIMD_YUV_TO_RGB_TABLE_H_
#define MEDIA_BASE_SIMD_YUV_TO_RGB_TABLE_H_

#include "base/basictypes.h"
#include "build/build_config.h"

extern "C" {

#if defined(COMPILER_MSVC)
#define SIMD_ALIGNED(var) __declspec(align(16)) var
#else
#define SIMD_ALIGNED(var) var __attribute__((aligned(16)))
#endif

// Align the table to 16-bytes to allow faster reading.
extern SIMD_ALIGNED(const int16 kCoefficientsRgbY[256 * 4][4]);
extern SIMD_ALIGNED(const int16 kCoefficientsRgbY_JPEG[256 * 4][4]);

}  // extern "C"

#endif  // MEDIA_BASE_SIMD_YUV_TO_RGB_TABLE_H_
