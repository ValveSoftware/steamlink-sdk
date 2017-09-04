// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PUSH_MESSAGING_PUSH_DISPATCHER_H_
#define CONTENT_CHILD_PUSH_MESSAGING_PUSH_DISPATCHER_H_

#include <map>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "content/child/worker_thread_message_filter.h"

namespace content {

class PushDispatcher : public WorkerThreadMessageFilter {
 public:
  explicit PushDispatcher(ThreadSafeSender* thread_safe_sender);

  // Generates a process-unique new request id. Stores it in a map as key to
  // |thread_id| and returns it. This method can be called on any thread.
  // Note that the registration requests from document contexts do not go via
  // this class and their request ids may overlap with the ones generated here.
  int GenerateRequestId(int thread_id);

 protected:
  ~PushDispatcher() override;

 private:
  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

  base::Lock request_id_map_lock_;
  std::map<int, int> request_id_map_;  // Maps request id to thread id.
  int next_request_id_;

  DISALLOW_COPY_AND_ASSIGN(PushDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_PUSH_MESSAGING_PUSH_DISPATCHER_H_
