// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RENDERING_STATS_INSTRUMENTATION_H_
#define CC_DEBUG_RENDERING_STATS_INSTRUMENTATION_H_

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/debug/rendering_stats.h"

namespace cc {

// RenderingStatsInstrumentation is shared among threads and manages conditional
// recording of rendering stats into a private RenderingStats instance.
class CC_EXPORT RenderingStatsInstrumentation {
 public:
  static scoped_ptr<RenderingStatsInstrumentation> Create();
  virtual ~RenderingStatsInstrumentation();

  // Return copy of current main thread rendering stats.
  MainThreadRenderingStats main_thread_rendering_stats();

  // Return copy of current impl thread rendering stats.
  ImplThreadRenderingStats impl_thread_rendering_stats();

  // Return the accumulated, combined rendering stats.
  RenderingStats GetRenderingStats();

  // Add current main thread rendering stats to accumulator and
  // clear current stats.
  void AccumulateAndClearMainThreadStats();

  // Add current impl thread rendering stats to accumulator and
  // clear current stats.
  void AccumulateAndClearImplThreadStats();

  // Read and write access to the record_rendering_stats_ flag is not locked to
  // improve performance. The flag is commonly turned off and hardly changes
  // it's value during runtime.
  bool record_rendering_stats() const { return record_rendering_stats_; }
  void set_record_rendering_stats(bool record_rendering_stats) {
    if (record_rendering_stats_ != record_rendering_stats)
      record_rendering_stats_ = record_rendering_stats;
  }

  base::TimeTicks StartRecording() const;
  base::TimeDelta EndRecording(base::TimeTicks start_time) const;

  void IncrementFrameCount(int64 count, bool main_thread);
  void AddPaint(base::TimeDelta duration, int64 pixels);
  void AddRecord(base::TimeDelta duration, int64 pixels);
  void AddRaster(base::TimeDelta duration, int64 pixels);
  void AddAnalysis(base::TimeDelta duration, int64 pixels);
  void AddVisibleContentArea(int64 area);
  void AddApproximatedVisibleContentArea(int64 area);

 protected:
  RenderingStatsInstrumentation();

 private:
  MainThreadRenderingStats main_thread_rendering_stats_;
  MainThreadRenderingStats main_thread_rendering_stats_accu_;
  ImplThreadRenderingStats impl_thread_rendering_stats_;
  ImplThreadRenderingStats impl_thread_rendering_stats_accu_;

  bool record_rendering_stats_;

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(RenderingStatsInstrumentation);
};

}  // namespace cc

#endif  // CC_DEBUG_RENDERING_STATS_INSTRUMENTATION_H_
