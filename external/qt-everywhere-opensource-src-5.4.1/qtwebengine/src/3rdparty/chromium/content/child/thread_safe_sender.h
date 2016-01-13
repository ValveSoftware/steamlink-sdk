// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_THREAD_SAFE_SENDER_H_
#define CONTENT_CHILD_THREAD_SAFE_SENDER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"

namespace base {
class MessageLoopProxy;
}

namespace IPC {
class SyncMessageFilter;
}

namespace content {
class ChildThread;

// The class of Sender returned by ChildThread::thread_safe_sender().
class CONTENT_EXPORT ThreadSafeSender
    : public IPC::Sender,
      public base::RefCountedThreadSafe<ThreadSafeSender> {
 public:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  friend class ChildThread;  // for construction
  friend class IndexedDBDispatcherTest;
  friend class WebIDBCursorImplTest;
  friend class base::RefCountedThreadSafe<ThreadSafeSender>;

  ThreadSafeSender(base::MessageLoopProxy* main_loop,
                   IPC::SyncMessageFilter* sync_filter);
  virtual ~ThreadSafeSender();

  scoped_refptr<base::MessageLoopProxy> main_loop_;
  scoped_refptr<IPC::SyncMessageFilter> sync_filter_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeSender);
};

}  // namespace content

#endif  // CONTENT_CHILD_THREAD_SAFE_SENDER_H_
