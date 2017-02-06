// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_request_sender.h"

#include "content/public/child/worker_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/worker_thread_dispatcher.h"

namespace extensions {

ServiceWorkerRequestSender::ServiceWorkerRequestSender(
    WorkerThreadDispatcher* dispatcher,
    int embedded_worker_id)
    : dispatcher_(dispatcher), embedded_worker_id_(embedded_worker_id) {}

ServiceWorkerRequestSender::~ServiceWorkerRequestSender() {}

void ServiceWorkerRequestSender::SendRequest(
    content::RenderFrame* render_frame,
    bool for_io_thread,
    ExtensionHostMsg_Request_Params& params) {
  DCHECK(!render_frame && !for_io_thread);
  int worker_thread_id = content::WorkerThread::GetCurrentId();
  DCHECK_GT(worker_thread_id, 0);
  params.worker_thread_id = worker_thread_id;
  params.embedded_worker_id = embedded_worker_id_;

  dispatcher_->Send(new ExtensionHostMsg_RequestWorker(params));
}

}  // namespace extensions
