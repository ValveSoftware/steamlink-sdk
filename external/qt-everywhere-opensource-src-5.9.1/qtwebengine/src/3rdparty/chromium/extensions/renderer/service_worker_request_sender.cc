// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_request_sender.h"

#include "base/guid.h"
#include "content/public/child/worker_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/worker_thread_dispatcher.h"

namespace extensions {

ServiceWorkerRequestSender::ServiceWorkerRequestSender(
    WorkerThreadDispatcher* dispatcher,
    int64_t service_worker_version_id)
    : dispatcher_(dispatcher),
      service_worker_version_id_(service_worker_version_id) {}

ServiceWorkerRequestSender::~ServiceWorkerRequestSender() {}

void ServiceWorkerRequestSender::SendRequest(
    content::RenderFrame* render_frame,
    bool for_io_thread,
    ExtensionHostMsg_Request_Params& params) {
  DCHECK(!render_frame && !for_io_thread);
  int worker_thread_id = content::WorkerThread::GetCurrentId();
  DCHECK_GT(worker_thread_id, 0);
  params.worker_thread_id = worker_thread_id;
  params.service_worker_version_id = service_worker_version_id_;
  std::string guid = base::GenerateGUID();
  request_id_to_guid_[params.request_id] = guid;

  // Keeps the worker alive during extension function call. Balanced in
  // HandleWorkerResponse().
  dispatcher_->Send(new ExtensionHostMsg_IncrementServiceWorkerActivity(
      service_worker_version_id_, guid));

  dispatcher_->Send(new ExtensionHostMsg_RequestWorker(params));
}

void ServiceWorkerRequestSender::HandleWorkerResponse(
    int request_id,
    int64_t service_worker_version_id,
    bool success,
    const base::ListValue& response,
    const std::string& error) {
  RequestSender::HandleResponse(request_id, success, response, error);

  std::map<int, std::string>::iterator iter =
      request_id_to_guid_.find(request_id);
  DCHECK(iter != request_id_to_guid_.end());
  dispatcher_->Send(new ExtensionHostMsg_DecrementServiceWorkerActivity(
      service_worker_version_id_, iter->second));
  request_id_to_guid_.erase(iter);
}

}  // namespace extensions
