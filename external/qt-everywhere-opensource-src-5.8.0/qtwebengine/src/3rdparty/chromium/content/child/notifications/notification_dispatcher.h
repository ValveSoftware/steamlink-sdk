// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_DISPATCHER_H_
#define CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_DISPATCHER_H_

#include <map>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "content/child/worker_thread_message_filter.h"

namespace content {

class NotificationDispatcher : public WorkerThreadMessageFilter {
 public:
  explicit NotificationDispatcher(ThreadSafeSender* thread_safe_sender);

  // Generates a, process-unique new notification Id mapped to |thread_id|, and
  // return the notification Id. This method can be called on any thread.
  int GenerateNotificationId(int thread_id);

 protected:
  ~NotificationDispatcher() override;

 private:
  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

  using NotificationIdToThreadId = std::map<int, int>;

  base::Lock notification_id_map_lock_;
  NotificationIdToThreadId notification_id_map_;
  int next_notification_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NotificationDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_DISPATCHER_H_
