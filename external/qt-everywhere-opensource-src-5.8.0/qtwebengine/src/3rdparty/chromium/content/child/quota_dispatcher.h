// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_QUOTA_DISPATCHER_H_
#define CONTENT_CHILD_QUOTA_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <set>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/child/worker_thread.h"
#include "storage/common/quota/quota_types.h"

class GURL;

namespace IPC {
class Message;
}

namespace blink {
class WebStorageQuotaCallbacks;
}

namespace content {

class ThreadSafeSender;
class QuotaMessageFilter;

// Dispatches and sends quota related messages sent to/from a child
// process from/to the main browser process.  There is one instance
// per each thread.  Thread-specific instance can be obtained by
// ThreadSpecificInstance().
class QuotaDispatcher : public WorkerThread::Observer {
 public:
  class Callback {
   public:
    virtual ~Callback() {}
    virtual void DidQueryStorageUsageAndQuota(int64_t usage, int64_t quota) = 0;
    virtual void DidGrantStorageQuota(int64_t usage, int64_t granted_quota) = 0;
    virtual void DidFail(storage::QuotaStatusCode status) = 0;
  };

  QuotaDispatcher(ThreadSafeSender* thread_safe_sender,
                  QuotaMessageFilter* quota_message_filter);
  ~QuotaDispatcher() override;

  // |thread_safe_sender| and |quota_message_filter| are used if
  // calling this leads to construction.
  static QuotaDispatcher* ThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      QuotaMessageFilter* quota_message_filter);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  void OnMessageReceived(const IPC::Message& msg);

  void QueryStorageUsageAndQuota(const GURL& gurl,
                                 storage::StorageType type,
                                 Callback* callback);
  void RequestStorageQuota(int render_view_id,
                           const GURL& gurl,
                           storage::StorageType type,
                           uint64_t requested_size,
                           Callback* callback);

  // Creates a new Callback instance for WebStorageQuotaCallbacks.
  static Callback* CreateWebStorageQuotaCallbacksWrapper(
      blink::WebStorageQuotaCallbacks callbacks);

 private:
  // Message handlers.
  void DidQueryStorageUsageAndQuota(int request_id,
                                    int64_t current_usage,
                                    int64_t current_quota);
  void DidGrantStorageQuota(int request_id,
                            int64_t current_usage,
                            int64_t granted_quota);
  void DidFail(int request_id, storage::QuotaStatusCode error);

  IDMap<Callback, IDMapOwnPointer> pending_quota_callbacks_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<QuotaMessageFilter> quota_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(QuotaDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_QUOTA_DISPATCHER_H_
