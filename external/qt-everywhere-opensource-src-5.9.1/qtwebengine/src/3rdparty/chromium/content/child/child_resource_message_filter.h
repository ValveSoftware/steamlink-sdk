// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_RESOURCE_MESSAGE_FILTER_H_
#define CONTENT_CHILD_CHILD_RESOURCE_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ipc/message_filter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class ResourceDispatcher;

// Supplies ResourceDispatcher with timestamps for some resource messages.
//
// Background: ResourceDispatcher converts browser process time to child
// process time. This is done to achieve coherent timeline. Conversion is
// a linear transformation such that given browser process time range is
// mapped to corresponding child process time range. Timestamps for child
// process time range should be taken by IO thread when resource messages
// arrive. Otherwise, timestamps may be affected by long rendering / JS task.
//
// When specific message is processed by this filter, new task charged
// with timestamp is posted to main thread. This task is processed just before
// resource message and invokes ResourceDispatcher::set_io_timestamp.
class ChildResourceMessageFilter : public IPC::MessageFilter {
 public:
  explicit ChildResourceMessageFilter(ResourceDispatcher* resource_dispatcher);

  // IPC::MessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  void SetMainThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
    main_thread_task_runner_ = main_thread_task_runner;
  }

 private:
  ~ChildResourceMessageFilter() override;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  ResourceDispatcher* resource_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ChildResourceMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_RESOURCE_MESSAGE_FILTER_H_
