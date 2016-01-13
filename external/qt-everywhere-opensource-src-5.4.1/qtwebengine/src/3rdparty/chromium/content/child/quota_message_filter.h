// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_QUOTA_MESSAGE_FILTER_H_
#define CONTENT_CHILD_QUOTA_MESSAGE_FILTER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/child/child_message_filter.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class ThreadSafeSender;

class QuotaMessageFilter : public ChildMessageFilter {
 public:
  explicit QuotaMessageFilter(ThreadSafeSender* thread_safe_sender);

  // Generates a new request_id, registers { request_id, thread_id } map to
  // the message filter and returns the request_id.
  // This method can be called on any thread.
  int GenerateRequestID(int thread_id);

  // Clears all requests from the thread_id.
  void ClearThreadRequests(int thread_id);

 protected:
  virtual ~QuotaMessageFilter();

 private:
  // ChildMessageFilter implementation:
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& msg) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  typedef std::map<int, int> RequestIdToThreadId;

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  base::Lock request_id_map_lock_;
  RequestIdToThreadId request_id_map_;
  int next_request_id_;

  DISALLOW_COPY_AND_ASSIGN(QuotaMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_QUOTA_MESSAGE_FILTER_H_
