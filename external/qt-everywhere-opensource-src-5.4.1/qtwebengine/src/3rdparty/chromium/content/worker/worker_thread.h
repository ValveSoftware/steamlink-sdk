// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WORKER_THREAD_H_
#define CONTENT_WORKER_WORKER_THREAD_H_

#include <set>

#include "content/child/child_thread.h"

struct WorkerProcessMsg_CreateWorker_Params;

namespace content {
class AppCacheDispatcher;
class DBMessageFilter;
class IndexedDBMessageFilter;
class WebSharedWorkerStub;
class WorkerWebKitPlatformSupportImpl;

class WorkerThread : public ChildThread {
 public:
  WorkerThread();
  virtual ~WorkerThread();
  virtual void Shutdown() OVERRIDE;

  // Returns the one worker thread.
  static WorkerThread* current();

  // Invoked from stub constructors/destructors. Stubs own themselves.
  void AddWorkerStub(WebSharedWorkerStub* stub);
  void RemoveWorkerStub(WebSharedWorkerStub* stub);

  AppCacheDispatcher* appcache_dispatcher() {
    return appcache_dispatcher_.get();
  }

 private:
  virtual bool OnControlMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  void OnCreateWorker(const WorkerProcessMsg_CreateWorker_Params& params);
  void OnShutdown();

  scoped_ptr<WorkerWebKitPlatformSupportImpl> webkit_platform_support_;
  scoped_ptr<AppCacheDispatcher> appcache_dispatcher_;
  scoped_refptr<DBMessageFilter> db_message_filter_;
  scoped_refptr<IndexedDBMessageFilter> indexed_db_message_filter_;

  typedef std::set<WebSharedWorkerStub*> WorkerStubsList;
  WorkerStubsList worker_stubs_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThread);
};

}  // namespace content

#endif  // CONTENT_WORKER_WORKER_THREAD_H_
