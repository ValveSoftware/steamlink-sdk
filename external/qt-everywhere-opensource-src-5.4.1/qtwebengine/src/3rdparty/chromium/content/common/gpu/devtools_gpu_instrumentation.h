// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_DEVTOOLS_GPU_INSTRUMENTATION_H_
#define CONTENT_COMMON_GPU_DEVTOOLS_GPU_INSTRUMENTATION_H_

#include <vector>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"

namespace content {

class DevToolsGpuAgent;
class GpuChannel;

class GpuEventsDispatcher : public base::NonThreadSafe {
 public:
  enum EventPhase {
    kEventStart,
    kEventFinish
  };

  GpuEventsDispatcher();
  ~GpuEventsDispatcher();

  void AddProcessor(DevToolsGpuAgent* processor);
  void RemoveProcessor(DevToolsGpuAgent* processor);

  static void FireEvent(EventPhase phase, GpuChannel* channel) {
    if (!IsEnabled())
      return;
    DoFireEvent(phase, channel);
  }

private:
  static bool IsEnabled() { return enabled_; }
  static void DoFireEvent(EventPhase, GpuChannel* channel);

  static bool enabled_;
  std::vector<DevToolsGpuAgent*> processors_;
};

namespace devtools_gpu_instrumentation {

class ScopedGpuTask {
 public:
  explicit ScopedGpuTask(GpuChannel* channel) :
      channel_(channel) {
    GpuEventsDispatcher::FireEvent(GpuEventsDispatcher::kEventStart, channel_);
  }
  ~ScopedGpuTask() {
    GpuEventsDispatcher::FireEvent(GpuEventsDispatcher::kEventFinish, channel_);
  }
 private:
  GpuChannel* channel_;
  DISALLOW_COPY_AND_ASSIGN(ScopedGpuTask);
};

} // namespace devtools_gpu_instrumentation

} // namespace content

#endif  // CONTENT_COMMON_GPU_DEVTOOLS_GPU_INSTRUMENTATION_H_
