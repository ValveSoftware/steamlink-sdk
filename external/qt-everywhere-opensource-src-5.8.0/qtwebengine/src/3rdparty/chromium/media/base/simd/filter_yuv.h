// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SIMD_FILTER_YUV_H_
#define MEDIA_BASE_SIMD_FILTER_YUV_H_

#include <stdint.h>

#include "media/base/media_export.h"

namespace media {

// These methods are exported for testing purposes only.  Library users should
// only call the methods listed in yuv_convert.h.

MEDIA_EXPORT void FilterYUVRows_C(uint8_t* ybuf,
                                  const uint8_t* y0_ptr,
                                  const uint8_t* y1_ptr,
                                  int source_width,
                                  uint8_t source_y_fraction);

MEDIA_EXPORT void FilterYUVRows_SSE2(uint8_t* ybuf,
                                     const uint8_t* y0_ptr,
                                     const uint8_t* y1_ptr,
                                     int source_width,
                                     uint8_t source_y_fraction);

}  // namespace media

#endif  // MEDIA_BASE_SIMD_FILTER_YUV_H_
