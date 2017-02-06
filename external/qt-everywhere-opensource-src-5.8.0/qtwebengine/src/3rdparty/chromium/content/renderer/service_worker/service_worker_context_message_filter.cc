// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_context_message_filter.h"

#include "content/child/child_thread_impl.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "ipc/ipc_message_macros.h"

namespace content {

ServiceWorkerContextMessageFilter::ServiceWorkerContextMessageFilter()
    : WorkerThreadMessageFilter(
          ChildThreadImpl::current()->thread_safe_sender()) {
}

ServiceWorkerContextMessageFilter::~ServiceWorkerContextMessageFilter() {}

bool ServiceWorkerContextMessageFilter::ShouldHandleMessage(
    const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == EmbeddedWorkerContextMsgStart;
}

void ServiceWorkerContextMessageFilter::OnFilteredMessageReceived(
    const IPC::Message& msg) {
  ServiceWorkerContextClient* context =
      ServiceWorkerContextClient::ThreadSpecificInstance();
  if (!context) {
    LOG(ERROR) << "Stray message is sent to nonexistent worker";
    return;
  }
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerContextClient, msg)
    IPC_MESSAGE_FORWARD(EmbeddedWorkerContextMsg_MessageToWorker,
                        context,
                        ServiceWorkerContextClient::OnMessageReceived)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
}

bool ServiceWorkerContextMessageFilter::GetWorkerThreadIdForMessage(
    const IPC::Message& msg,
    int* ipc_thread_id) {
  return base::PickleIterator(msg).ReadInt(ipc_thread_id);
}

}  // namespace content
