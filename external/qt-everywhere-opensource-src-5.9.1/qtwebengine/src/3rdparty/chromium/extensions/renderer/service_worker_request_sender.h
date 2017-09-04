// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SERVICE_WORKER_REQUEST_SENDER_H_
#define EXTENSIONS_RENDERER_SERVICE_WORKER_REQUEST_SENDER_H_

#include "extensions/renderer/request_sender.h"

namespace extensions {
class WorkerThreadDispatcher;

// A RequestSender variant for Extension Service Worker.
class ServiceWorkerRequestSender : public RequestSender {
 public:
  ServiceWorkerRequestSender(WorkerThreadDispatcher* dispatcher,
                             int64_t service_worker_version_id);
  ~ServiceWorkerRequestSender() override;

  void SendRequest(content::RenderFrame* render_frame,
                   bool for_io_thread,
                   ExtensionHostMsg_Request_Params& params) override;
  void HandleWorkerResponse(int request_id,
                            int64_t service_worker_version_id,
                            bool success,
                            const base::ListValue& response,
                            const std::string& error);

 private:
  WorkerThreadDispatcher* const dispatcher_;
  const int64_t service_worker_version_id_;

  // request id -> GUID map for each outstanding requests.
  std::map<int, std::string> request_id_to_guid_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestSender);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SERVICE_WORKER_REQUEST_SENDER_H_
