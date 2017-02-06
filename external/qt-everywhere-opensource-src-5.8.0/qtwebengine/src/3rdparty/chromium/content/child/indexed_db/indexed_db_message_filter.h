// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_
#define CONTENT_CHILD_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/child/worker_thread_message_filter.h"

struct IndexedDBDatabaseMetadata;
struct IndexedDBMsg_CallbacksUpgradeNeeded_Params;

namespace content {

class IndexedDBMessageFilter : public WorkerThreadMessageFilter {
 public:
  explicit IndexedDBMessageFilter(ThreadSafeSender* thread_safe_sender);

 protected:
  ~IndexedDBMessageFilter() override;

 private:
  // WorkerThreadMessageFilter:
  bool ShouldHandleMessage(const IPC::Message& msg) const override;
  void OnFilteredMessageReceived(const IPC::Message& msg) override;
  bool GetWorkerThreadIdForMessage(const IPC::Message& msg,
                                   int* ipc_thread_id) override;

  // ChildMessageFilter:
  void OnStaleMessageReceived(const IPC::Message& msg) override;

  void OnStaleSuccessIDBDatabase(int32_t ipc_thread_id,
                                 int32_t ipc_callbacks_id,
                                 int32_t ipc_database_callbacks_id,
                                 int32_t ipc_object_id,
                                 const IndexedDBDatabaseMetadata&);
  void OnStaleUpgradeNeeded(const IndexedDBMsg_CallbacksUpgradeNeeded_Params&);

  DISALLOW_COPY_AND_ASSIGN(IndexedDBMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_
