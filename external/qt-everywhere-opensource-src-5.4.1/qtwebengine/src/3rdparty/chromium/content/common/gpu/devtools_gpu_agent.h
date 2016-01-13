// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_DEVTOOLS_GPU_AGENT_H_
#define CONTENT_COMMON_GPU_DEVTOOLS_GPU_AGENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "content/common/gpu/devtools_gpu_instrumentation.h"

using base::TimeTicks;
struct GpuTaskInfo;

namespace IPC {
class Message;
}

namespace content {

class GpuChannel;

class DevToolsGpuAgent : public base::NonThreadSafe {
 public:
  explicit DevToolsGpuAgent(GpuChannel* gpu_channel);
  virtual ~DevToolsGpuAgent();

  void ProcessEvent(TimeTicks timestamp,
                    GpuEventsDispatcher::EventPhase,
                    GpuChannel* channel);

  bool StartEventsRecording(int32 route_id);
  void StopEventsRecording();

 private:
  typedef std::vector<GpuTaskInfo> GpuTaskInfoList;

  bool Send(IPC::Message* msg);

  GpuChannel* gpu_channel_;
  scoped_ptr<GpuTaskInfoList> tasks_;
  TimeTicks last_flush_time_;
  int32 route_id_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsGpuAgent);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_DEVTOOLS_GPU_AGENT_H_
