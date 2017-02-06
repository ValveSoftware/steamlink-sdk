// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/resource_dispatch_throttler.h"

#include "base/auto_reset.h"
#include "base/trace_event/trace_event.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/common/resource_messages.h"
#include "ipc/ipc_message_macros.h"

namespace content {
namespace {

bool IsResourceRequest(const IPC::Message& msg) {
  return msg.type() == ResourceHostMsg_RequestResource::ID;
}

}  // namespace

ResourceDispatchThrottler::ResourceDispatchThrottler(
    IPC::Sender* proxied_sender,
    scheduler::RendererScheduler* scheduler,
    base::TimeDelta flush_period,
    uint32_t max_requests_per_flush)
    : proxied_sender_(proxied_sender),
      scheduler_(scheduler),
      flush_period_(flush_period),
      max_requests_per_flush_(max_requests_per_flush),
      flush_timer_(
          FROM_HERE,
          flush_period_,
          base::Bind(&ResourceDispatchThrottler::Flush, base::Unretained(this)),
          false /* is_repeating */),
      sent_requests_since_last_flush_(0) {
  DCHECK(proxied_sender);
  DCHECK(scheduler);
  DCHECK_NE(flush_period_, base::TimeDelta());
  DCHECK(max_requests_per_flush_);
  flush_timer_.SetTaskRunner(scheduler->LoadingTaskRunner());
}

ResourceDispatchThrottler::~ResourceDispatchThrottler() {
  FlushAll();
}

bool ResourceDispatchThrottler::Send(IPC::Message* msg) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (msg->is_sync()) {
    // Flush any pending requests, preserving dispatch order between async and
    // sync requests.
    FlushAll();
    return ForwardMessage(msg);
  }

  // Always defer message forwarding if there are pending messages, ensuring
  // message dispatch ordering consistency.
  if (!throttled_messages_.empty()) {
    TRACE_EVENT_INSTANT0("loader", "ResourceDispatchThrottler::ThrottleMessage",
                         TRACE_EVENT_SCOPE_THREAD);
    throttled_messages_.push_back(msg);
    return true;
  }

  if (!IsResourceRequest(*msg))
    return ForwardMessage(msg);

  if (!scheduler_->IsHighPriorityWorkAnticipated()) {
    // Treat an unthrottled request as a flush.
    LogFlush();
    return ForwardMessage(msg);
  }

  if (Now() > (last_flush_time_ + flush_period_)) {
    // If sufficient time has passed since the previous flush, we can
    // effectively mark the pipeline as flushed.
    LogFlush();
    return ForwardMessage(msg);
  }

  if (sent_requests_since_last_flush_ < max_requests_per_flush_)
    return ForwardMessage(msg);

  TRACE_EVENT_INSTANT0("loader", "ResourceDispatchThrottler::ThrottleRequest",
                       TRACE_EVENT_SCOPE_THREAD);
  throttled_messages_.push_back(msg);
  ScheduleFlush();
  return true;
}

base::TimeTicks ResourceDispatchThrottler::Now() const {
  return base::TimeTicks::Now();
}

void ResourceDispatchThrottler::ScheduleFlush() {
  DCHECK(!flush_timer_.IsRunning());
  flush_timer_.Reset();
}

void ResourceDispatchThrottler::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT1("loader", "ResourceDispatchThrottler::Flush",
               "total_throttled_messages", throttled_messages_.size());
  LogFlush();

  // If high-priority work is no longer anticipated, dispatch can be safely
  // accelerated. Avoid completely flushing in such case in the event that
  // a large number of requests have been throttled.
  uint32_t max_requests = scheduler_->IsHighPriorityWorkAnticipated()
                              ? max_requests_per_flush_
                              : max_requests_per_flush_ * 2;

  while (!throttled_messages_.empty() &&
         (sent_requests_since_last_flush_ < max_requests ||
          !IsResourceRequest(*throttled_messages_.front()))) {
    IPC::Message* msg = throttled_messages_.front();
    throttled_messages_.pop_front();
    ForwardMessage(msg);
  }

  if (!throttled_messages_.empty())
    ScheduleFlush();
}

void ResourceDispatchThrottler::FlushAll() {
  LogFlush();
  if (throttled_messages_.empty())
    return;

  TRACE_EVENT1("loader", "ResourceDispatchThrottler::FlushAll",
               "total_throttled_messages", throttled_messages_.size());
  std::deque<IPC::Message*> throttled_messages;
  throttled_messages.swap(throttled_messages_);
  for (auto& message : throttled_messages)
    ForwardMessage(message);
  // There shouldn't be re-entrancy issues when forwarding an IPC, but validate
  // as a safeguard.
  DCHECK(throttled_messages_.empty());
}

void ResourceDispatchThrottler::LogFlush() {
  sent_requests_since_last_flush_ = 0;
  last_flush_time_ = Now();
}

bool ResourceDispatchThrottler::ForwardMessage(IPC::Message* msg) {
  if (IsResourceRequest(*msg))
    ++sent_requests_since_last_flush_;

  return proxied_sender_->Send(msg);
}

}  // namespace content
