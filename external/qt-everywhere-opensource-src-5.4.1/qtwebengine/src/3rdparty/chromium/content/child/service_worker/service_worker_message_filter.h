// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_

#include "content/child/child_message_filter.h"
#include "content/common/content_export.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

struct ServiceWorkerObjectInfo;
class ThreadSafeSender;

class CONTENT_EXPORT ServiceWorkerMessageFilter
    : public NON_EXPORTED_BASE(ChildMessageFilter) {
 public:
  explicit ServiceWorkerMessageFilter(ThreadSafeSender* thread_safe_sender);

 protected:
  virtual ~ServiceWorkerMessageFilter();

 private:
  // ChildMessageFilter implementation:
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& msg) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnStaleMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Message handlers for stale messages.
  void OnStaleRegistered(int thread_id,
                         int request_id,
                         const ServiceWorkerObjectInfo& info);
  void OnStaleSetServiceWorker(int thread_id,
                               int provider_id,
                               const ServiceWorkerObjectInfo& info);

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_MESSAGE_FILTER_H_
