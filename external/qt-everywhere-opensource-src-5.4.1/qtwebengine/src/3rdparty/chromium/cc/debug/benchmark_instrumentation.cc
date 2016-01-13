// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "cc/debug/benchmark_instrumentation.h"

namespace cc {

// Please do not change the trace events in this file without updating
// tools/perf/measurements/rendering_stats.py accordingly.
// The benchmarks search for events and their arguments by name.

void BenchmarkInstrumentation::IssueMainThreadRenderingStatsEvent(
    const MainThreadRenderingStats& stats) {
  TRACE_EVENT_INSTANT1("benchmark",
                       "BenchmarkInstrumentation::MainThreadRenderingStats",
                       TRACE_EVENT_SCOPE_THREAD,
                       "data", stats.AsTraceableData());
}

void BenchmarkInstrumentation::IssueImplThreadRenderingStatsEvent(
    const ImplThreadRenderingStats& stats) {
  TRACE_EVENT_INSTANT1("benchmark",
                       "BenchmarkInstrumentation::ImplThreadRenderingStats",
                       TRACE_EVENT_SCOPE_THREAD,
                       "data", stats.AsTraceableData());
}

}  // namespace cc
