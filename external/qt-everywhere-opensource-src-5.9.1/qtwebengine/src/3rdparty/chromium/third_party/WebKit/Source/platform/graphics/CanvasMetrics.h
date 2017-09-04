// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasMetrics_h
#define CanvasMetrics_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"

namespace blink {

class PLATFORM_EXPORT CanvasMetrics {
  STATIC_ONLY(CanvasMetrics);

 public:
  enum CanvasContextUsage {
    CanvasCreated = 0,
    GPUAccelerated2DCanvasImageBufferCreated = 1,
    DisplayList2DCanvasImageBufferCreated = 2,
    Unaccelerated2DCanvasImageBufferCreated = 3,
    Accelerated2DCanvasGPUContextLost = 4,
    Unaccelerated2DCanvasImageBufferCreationFailed = 5,
    GPUAccelerated2DCanvasImageBufferCreationFailed = 6,
    DisplayList2DCanvasFallbackToRaster = 7,
    GPUAccelerated2DCanvasDeferralDisabled = 8,
    GPUAccelerated2DCanvasSurfaceCreationFailed = 9,
    NumberOfUsages
  };

  static void countCanvasContextUsage(const CanvasContextUsage);
};

}  // namespace blink

#endif  // CanvasMetrics_h
