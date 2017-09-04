// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/child/worker_thread_message_filter.h"

namespace content {

class ServiceWorkerContextMessageFilter : public WorkerThreadMessageFilter {
 public:
  ServiceWorkerContextMessageFilter();

 protected:
  ~ServiceWorkerContextMessageFilter() override;

  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_MESSAGE_FILTER_H_
