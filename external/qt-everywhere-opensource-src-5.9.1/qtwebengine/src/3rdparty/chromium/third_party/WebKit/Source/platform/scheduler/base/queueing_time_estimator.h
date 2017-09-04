// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_QUEUEING_TIME_ESTIMATOR_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_QUEUEING_TIME_ESTIMATOR_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "public/platform/WebCommon.h"

namespace blink {
namespace scheduler {

// Records the expected queueing time for a high priority task occurring
// randomly during each interval of length |window_duration|.
class BLINK_PLATFORM_EXPORT QueueingTimeEstimator {
 public:
  class BLINK_PLATFORM_EXPORT Client {
   public:
    virtual void OnQueueingTimeForWindowEstimated(
        base::TimeDelta queueing_time) = 0;
    Client() {}
    virtual ~Client() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  QueueingTimeEstimator(Client* client, base::TimeDelta window_duration);

  void OnToplevelTaskCompleted(base::TimeTicks task_start_time,
                               base::TimeTicks task_end_time);

 private:
  bool TimePastWindowEnd(base::TimeTicks task_end_time);
  Client* client_;  // NOT OWNED.

  base::TimeDelta current_expected_queueing_time_;
  base::TimeDelta window_duration_;
  base::TimeTicks window_start_time_;

  DISALLOW_COPY_AND_ASSIGN(QueueingTimeEstimator);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_QUEUEING_TIME_ESTIMATOR_H_
