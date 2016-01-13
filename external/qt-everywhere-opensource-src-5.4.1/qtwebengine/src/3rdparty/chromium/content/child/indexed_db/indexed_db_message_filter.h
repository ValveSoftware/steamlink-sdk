// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_
#define CONTENT_CHILD_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_

#include "base/memory/ref_counted.h"
#include "content/child/child_message_filter.h"

struct IndexedDBDatabaseMetadata;
struct IndexedDBMsg_CallbacksUpgradeNeeded_Params;

namespace base {
class MessageLoopProxy;
}

namespace IPC {
class Message;
}

namespace content {

class ThreadSafeSender;

class IndexedDBMessageFilter : public ChildMessageFilter {
 public:
  explicit IndexedDBMessageFilter(ThreadSafeSender* thread_safe_sender);

 protected:
  virtual ~IndexedDBMessageFilter();

 private:
  // ChildMessageFilter implementation:
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& msg) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnStaleMessageReceived(const IPC::Message& msg) OVERRIDE;

  void OnStaleSuccessIDBDatabase(int32 ipc_thread_id,
                                 int32 ipc_callbacks_id,
                                 int32 ipc_database_callbacks_id,
                                 int32 ipc_object_id,
                                 const IndexedDBDatabaseMetadata&);
  void OnStaleUpgradeNeeded(const IndexedDBMsg_CallbacksUpgradeNeeded_Params&);

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_INDEXED_DB_MESSAGE_FILTER_H_
