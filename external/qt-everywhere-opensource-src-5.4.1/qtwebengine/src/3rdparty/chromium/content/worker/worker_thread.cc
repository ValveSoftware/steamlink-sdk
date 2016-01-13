// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/worker_thread.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/appcache_frontend_impl.h"
#include "content/child/db_message_filter.h"
#include "content/child/indexed_db/indexed_db_message_filter.h"
#include "content/child/runtime_features.h"
#include "content/child/web_database_observer_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/common/content_switches.h"
#include "content/worker/websharedworker_stub.h"
#include "content/worker/worker_webkitplatformsupport_impl.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"
#include "third_party/WebKit/public/web/WebDatabase.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "v8/include/v8.h"

using blink::WebRuntimeFeatures;

namespace content {

static base::LazyInstance<base::ThreadLocalPointer<WorkerThread> > lazy_tls =
    LAZY_INSTANCE_INITIALIZER;

WorkerThread::WorkerThread() {
  lazy_tls.Pointer()->Set(this);
  webkit_platform_support_.reset(new WorkerWebKitPlatformSupportImpl(
      thread_safe_sender(),
      sync_message_filter(),
      quota_message_filter()));

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kJavaScriptFlags)) {
    std::string flags(
        command_line.GetSwitchValueASCII(switches::kJavaScriptFlags));
    v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  }
  SetRuntimeFeaturesDefaultsAndUpdateFromArgs(command_line);

  blink::initialize(webkit_platform_support_.get());

  appcache_dispatcher_.reset(
      new AppCacheDispatcher(this, new AppCacheFrontendImpl()));

  db_message_filter_ = new DBMessageFilter();
  channel()->AddFilter(db_message_filter_.get());

  indexed_db_message_filter_ = new IndexedDBMessageFilter(
      thread_safe_sender());
  channel()->AddFilter(indexed_db_message_filter_->GetFilter());

}

void WorkerThread::OnShutdown() {
  // The worker process is to be shut down gracefully. Ask the browser
  // process to shut it down forcefully instead and wait on the message, so that
  // there are no races between threads when the process is shutting down.
  Send(new WorkerProcessHostMsg_ForceKillWorker());
}

WorkerThread::~WorkerThread() {
}

void WorkerThread::Shutdown() {
  ChildThread::Shutdown();

  if (webkit_platform_support_) {
    webkit_platform_support_->web_database_observer_impl()->
        WaitForAllDatabasesToClose();
  }

  // Shutdown in reverse of the initialization order.
  indexed_db_message_filter_ = NULL;

  channel()->RemoveFilter(db_message_filter_.get());
  db_message_filter_ = NULL;

  blink::shutdown();
  lazy_tls.Pointer()->Set(NULL);
}

WorkerThread* WorkerThread::current() {
  return lazy_tls.Pointer()->Get();
}

bool WorkerThread::OnControlMessageReceived(const IPC::Message& msg) {
  // Appcache messages are handled by a delegate.
  if (appcache_dispatcher_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerThread, msg)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_CreateWorker, OnCreateWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool WorkerThread::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerThread, msg)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_Shutdown, OnShutdown)
    IPC_MESSAGE_UNHANDLED(handled = ChildThread::OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WorkerThread::OnCreateWorker(
    const WorkerProcessMsg_CreateWorker_Params& params) {
  // WebSharedWorkerStub own themselves.
  new WebSharedWorkerStub(
      params.url,
      params.name,
      params.content_security_policy,
      params.security_policy_type,
      params.pause_on_start,
      params.route_id);
}

// The browser process is likely dead. Terminate all workers.
void WorkerThread::OnChannelError() {
  set_on_channel_error_called(true);

  for (WorkerStubsList::iterator it = worker_stubs_.begin();
       it != worker_stubs_.end(); ++it) {
    (*it)->OnChannelError();
  }
}

void WorkerThread::RemoveWorkerStub(WebSharedWorkerStub* stub) {
  worker_stubs_.erase(stub);
}

void WorkerThread::AddWorkerStub(WebSharedWorkerStub* stub) {
  worker_stubs_.insert(stub);
}

}  // namespace content
