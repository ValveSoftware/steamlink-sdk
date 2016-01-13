// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/devtools_gpu_agent.h"

#include "base/logging.h"
#include "content/common/devtools_messages.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"

namespace content {

DevToolsGpuAgent::DevToolsGpuAgent(GpuChannel* gpu_channel) :
    gpu_channel_(gpu_channel),
    route_id_(MSG_ROUTING_NONE) {
}

DevToolsGpuAgent::~DevToolsGpuAgent() {
}

bool DevToolsGpuAgent::StartEventsRecording(int32 route_id) {
  DCHECK(CalledOnValidThread());
  if (route_id_ != MSG_ROUTING_NONE) {
    // Events recording is already in progress, so "fail" the call by
    // returning false.
    return false;
  }
  route_id_ = route_id;
  tasks_.reset(new GpuTaskInfoList());
  GpuEventsDispatcher* dispatcher =
      gpu_channel_->gpu_channel_manager()->gpu_devtools_events_dispatcher();
  dispatcher->AddProcessor(this);
  return true;
}

void DevToolsGpuAgent::StopEventsRecording() {
  DCHECK(CalledOnValidThread());
  if (route_id_ == MSG_ROUTING_NONE)
    return;
  GpuEventsDispatcher* dispatcher =
      gpu_channel_->gpu_channel_manager()->gpu_devtools_events_dispatcher();
  dispatcher->RemoveProcessor(this);
  route_id_ = MSG_ROUTING_NONE;
}

void DevToolsGpuAgent::ProcessEvent(
    TimeTicks timestamp,
    GpuEventsDispatcher::EventPhase phase,
    GpuChannel* channel) {
  DCHECK(CalledOnValidThread());
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  GpuTaskInfo task;
  task.timestamp = (timestamp - TimeTicks()).InSecondsF();
  task.phase = phase;
  task.foreign = channel != gpu_channel_;
  task.gpu_memory_used_bytes = channel->GetMemoryUsage();
  task.gpu_memory_limit_bytes = gpu_channel_->gpu_channel_manager()->
      gpu_memory_manager()->GetMaximumClientAllocation();

  const int kFlushIntervalMs = 100;
  const unsigned kMaxPendingItems = 100;
  if (!tasks_->empty() &&
      ((timestamp - last_flush_time_).InMilliseconds() >= kFlushIntervalMs ||
       tasks_->size() >= kMaxPendingItems)) {
    Send(new DevToolsAgentMsg_GpuTasksChunk(route_id_, *tasks_));
    tasks_->clear();
    last_flush_time_ = timestamp;
  }
  tasks_->push_back(task);
}

bool DevToolsGpuAgent::Send(IPC::Message* msg) {
  scoped_ptr<IPC::Message> message(msg);
  return gpu_channel_ && gpu_channel_->Send(message.release());
}

}  // namespace content
