// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_context_message_filter.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/child/child_thread.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_task_runner.h"
#include "content/renderer/service_worker/embedded_worker_context_client.h"
#include "ipc/ipc_message_macros.h"

namespace content {

EmbeddedWorkerContextMessageFilter::EmbeddedWorkerContextMessageFilter()
    : main_thread_loop_proxy_(base::MessageLoopProxy::current()),
      thread_safe_sender_(ChildThread::current()->thread_safe_sender()) {}

EmbeddedWorkerContextMessageFilter::~EmbeddedWorkerContextMessageFilter() {}

base::TaskRunner*
EmbeddedWorkerContextMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != EmbeddedWorkerContextMsgStart)
    return NULL;
  int ipc_thread_id = 0;
  const bool success = PickleIterator(msg).ReadInt(&ipc_thread_id);
  DCHECK(success);
  if (!ipc_thread_id)
    return main_thread_loop_proxy_.get();
  return new WorkerThreadTaskRunner(ipc_thread_id);
}

bool EmbeddedWorkerContextMessageFilter::OnMessageReceived(
    const IPC::Message& msg) {
  if (IPC_MESSAGE_CLASS(msg) != EmbeddedWorkerContextMsgStart)
    return false;
  EmbeddedWorkerContextClient* client =
      EmbeddedWorkerContextClient::ThreadSpecificInstance();
  if (!client) {
    LOG(ERROR) << "Stray message is sent to nonexistent worker";
    return true;
  }
  return client->OnMessageReceived(msg);
}

}  // namespace content
