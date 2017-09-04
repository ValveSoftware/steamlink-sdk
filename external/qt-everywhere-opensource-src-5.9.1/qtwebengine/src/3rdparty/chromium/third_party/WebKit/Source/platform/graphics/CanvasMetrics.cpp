// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasMetrics.h"

#include "platform/Histogram.h"
#include "wtf/Threading.h"

namespace blink {

void CanvasMetrics::countCanvasContextUsage(
    const CanvasContextUsage canvasContextUsage) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, usageHistogram,
      new EnumerationHistogram("WebCore.CanvasContextUsage",
                               CanvasContextUsage::NumberOfUsages));
  usageHistogram.count(canvasContextUsage);
}

}  // namespace blink
