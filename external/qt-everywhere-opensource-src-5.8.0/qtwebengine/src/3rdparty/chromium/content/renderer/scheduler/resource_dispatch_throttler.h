// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_RESOURCE_DISPATCH_THROTTLER_H_
#define CONTENT_RENDERER_SCHEDULER_RESOURCE_DISPATCH_THROTTLER_H_

#include <stdint.h>

#include <deque>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"

namespace scheduler {
class RendererScheduler;
}

namespace content {

// Utility class for throttling a stream of resource requests targetted to a
// specific IPC sender. The throttling itself is very basic:
//  * When there is no high-priority work imminent to the main thread, as
//    indicated by the RendererScheduler, throttling is disabled.
//  * When >= N requests have been sent in a given time window, requests are
//    throttled. A timer periodically flushes a portion of the queued requests
//    until all such requests have been flushed.
// TODO(jdduke): Remove this class after resource requests become sufficiently
// cheap on the IO thread, crbug.com/440037.
class CONTENT_EXPORT ResourceDispatchThrottler : public IPC::Sender {
 public:
  // |flush_period| and |max_requests_per_flush| must be strictly positive
  // in duration/value.
  ResourceDispatchThrottler(IPC::Sender* proxied_sender,
                            scheduler::RendererScheduler* scheduler,
                            base::TimeDelta flush_period,
                            uint32_t max_requests_per_flush);
  ~ResourceDispatchThrottler() override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

 private:
  friend class ResourceDispatchThrottlerForTest;

  // Virtual for testing.
  virtual base::TimeTicks Now() const;
  virtual void ScheduleFlush();

  void Flush();
  void FlushAll();
  void LogFlush();
  bool ForwardMessage(IPC::Message* msg);

  base::ThreadChecker thread_checker_;

  IPC::Sender* const proxied_sender_;
  scheduler::RendererScheduler* const scheduler_;
  const base::TimeDelta flush_period_;
  const uint32_t max_requests_per_flush_;

  base::Timer flush_timer_;
  base::TimeTicks last_flush_time_;
  uint32_t sent_requests_since_last_flush_;
  std::deque<IPC::Message*> throttled_messages_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDispatchThrottler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_RESOURCE_DISPATCH_THROTTLER_H_
