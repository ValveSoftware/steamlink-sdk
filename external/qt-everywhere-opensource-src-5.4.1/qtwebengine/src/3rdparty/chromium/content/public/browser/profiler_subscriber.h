// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PROFILER_SUBSCRIBER_H_
#define CONTENT_PUBLIC_BROWSER_PROFILER_SUBSCRIBER_H_

#include "content/common/content_export.h"

namespace tracked_objects {
struct ProcessDataSnapshot;
}

namespace content {

// Objects interested in receiving profiler data derive from ProfilerSubscriber.
class CONTENT_EXPORT ProfilerSubscriber {
 public:
  // Send number of pending processes to subscriber. |end| is set to true if it
  // is the last time. This is called on the UI thread.
  virtual void OnPendingProcesses(int sequence_number,
                                  int pending_processes,
                                  bool end) = 0;

  // Send |profiler_data| back to subscriber.
  // This is called on the UI thread.
  virtual void OnProfilerDataCollected(
      int sequence_number,
      const tracked_objects::ProcessDataSnapshot& profiler_data,
      int process_type) = 0;

 protected:
  virtual ~ProfilerSubscriber() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROFILER_SUBSCRIBER_H_
