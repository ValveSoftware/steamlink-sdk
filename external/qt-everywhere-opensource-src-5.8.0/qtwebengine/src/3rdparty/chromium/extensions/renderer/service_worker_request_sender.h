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
                             int embedded_worker_id);
  ~ServiceWorkerRequestSender() override;

  void SendRequest(content::RenderFrame* render_frame,
                   bool for_io_thread,
                   ExtensionHostMsg_Request_Params& params) override;

 private:
  WorkerThreadDispatcher* const dispatcher_;
  const int embedded_worker_id_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestSender);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SERVICE_WORKER_REQUEST_SENDER_H_
