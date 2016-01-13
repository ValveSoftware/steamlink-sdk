// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_BENCHMARK_INSTRUMENTATION_H_
#define CC_DEBUG_BENCHMARK_INSTRUMENTATION_H_

#include "cc/debug/rendering_stats.h"

namespace cc {

class CC_EXPORT BenchmarkInstrumentation {
 public:
  static void IssueMainThreadRenderingStatsEvent(
      const MainThreadRenderingStats& stats);
  static void IssueImplThreadRenderingStatsEvent(
      const ImplThreadRenderingStats& stats);
};

}  // namespace cc

#endif  // CC_DEBUG_BENCHMARK_INSTRUMENTATION_H_
