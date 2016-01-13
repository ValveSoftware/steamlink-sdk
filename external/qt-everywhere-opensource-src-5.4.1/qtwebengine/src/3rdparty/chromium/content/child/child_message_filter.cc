// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "content/child/child_thread.h"
#include "content/child/thread_safe_sender.h"
#include "ipc/message_filter.h"

namespace content {

class ChildMessageFilter::Internal : public IPC::MessageFilter {
 public:
  explicit Internal(ChildMessageFilter* filter) : filter_(filter) {}

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE {
    scoped_refptr<base::TaskRunner> runner =
        filter_->OverrideTaskRunnerForMessage(msg);
    if (runner && !runner->RunsTasksOnCurrentThread()) {
      if (!runner->PostTask(
              FROM_HERE,
              base::Bind(
                  base::IgnoreResult(&ChildMessageFilter::OnMessageReceived),
                  filter_, msg)))
        filter_->OnStaleMessageReceived(msg);
      return true;
    }

    return filter_->OnMessageReceived(msg);
  }

 private:
  virtual ~Internal() {}
  scoped_refptr<ChildMessageFilter> filter_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

bool ChildMessageFilter::Send(IPC::Message* message) {
  return thread_safe_sender_->Send(message);
}

base::TaskRunner* ChildMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  return NULL;
}

ChildMessageFilter::ChildMessageFilter()
    : internal_(NULL),
      thread_safe_sender_(ChildThread::current()->thread_safe_sender()) {}

ChildMessageFilter::~ChildMessageFilter() {}

IPC::MessageFilter* ChildMessageFilter::GetFilter() {
  if (!internal_)
    internal_ = new Internal(this);
  return internal_;
}

}  // namespace content
