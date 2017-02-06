// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_DATA_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_DATA_H_

#include <memory>

#include "base/macros.h"
#include "extensions/renderer/service_worker_request_sender.h"
#include "extensions/renderer/v8_schema_registry.h"

namespace extensions {
class WorkerThreadDispatcher;

// Per ServiceWorker data in worker thread.
// Contains: RequestSender, V8SchemaRegistry.
// TODO(lazyboy): Also put worker ScriptContexts in this.
class ServiceWorkerData {
 public:
  ServiceWorkerData(WorkerThreadDispatcher* dispatcher, int embedded_worker_id);
  ~ServiceWorkerData();

  V8SchemaRegistry* v8_schema_registry() { return v8_schema_registry_.get(); }
  RequestSender* request_sender() { return request_sender_.get(); }
  int embedded_worker_id() const { return embedded_worker_id_; }

 private:
  const int embedded_worker_id_;

  std::unique_ptr<V8SchemaRegistry> v8_schema_registry_;
  std::unique_ptr<ServiceWorkerRequestSender> request_sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerData);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_DATA_H_
