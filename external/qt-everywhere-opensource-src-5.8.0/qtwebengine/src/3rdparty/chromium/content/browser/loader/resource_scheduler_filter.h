// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_FILTER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_FILTER_H_

#include "content/public/browser/browser_message_filter.h"

namespace content {

// This class listens for incoming ViewHostMsgs that are applicable to the
// ResourceScheduler and invokes the appropriate notifications. It must be
// inserted before the RenderMessageFilter, because the ResourceScheduler runs
// on the IO thread and we want to see the messages before the view messages are
// bounced to the UI thread.
class ResourceSchedulerFilter : public BrowserMessageFilter {
 public:
  explicit ResourceSchedulerFilter(int child_id);

  // BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~ResourceSchedulerFilter() override;

  int child_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_FILTER_H_
