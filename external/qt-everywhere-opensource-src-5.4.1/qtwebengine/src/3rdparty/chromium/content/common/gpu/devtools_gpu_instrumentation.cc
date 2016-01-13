// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/devtools_gpu_instrumentation.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "content/common/gpu/devtools_gpu_agent.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"

namespace content {

bool GpuEventsDispatcher::enabled_ = false;

GpuEventsDispatcher::GpuEventsDispatcher() {
}

GpuEventsDispatcher::~GpuEventsDispatcher() {
}

void GpuEventsDispatcher::AddProcessor(DevToolsGpuAgent* processor) {
  DCHECK(CalledOnValidThread());
  processors_.push_back(processor);
  enabled_ = !processors_.empty();
}

void GpuEventsDispatcher::RemoveProcessor(DevToolsGpuAgent* processor) {
  DCHECK(CalledOnValidThread());
  processors_.erase(
      std::remove(processors_.begin(), processors_.end(), processor),
      processors_.end());
  enabled_ = !processors_.empty();
}

// static
void GpuEventsDispatcher::DoFireEvent(EventPhase phase,
                                      GpuChannel* channel) {
  TimeTicks timestamp = base::TimeTicks::NowFromSystemTraceTime();
  GpuEventsDispatcher* self =
      channel->gpu_channel_manager()->gpu_devtools_events_dispatcher();
  DCHECK(self->CalledOnValidThread());
  std::vector<DevToolsGpuAgent*>::iterator it;
  for (it = self->processors_.begin(); it != self->processors_.end(); ++it) {
    (*it)->ProcessEvent(timestamp, phase, channel);
  }
}

} // namespace
