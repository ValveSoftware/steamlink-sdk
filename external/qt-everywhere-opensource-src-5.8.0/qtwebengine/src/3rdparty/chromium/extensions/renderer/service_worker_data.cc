// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_data.h"

#include "extensions/renderer/service_worker_request_sender.h"
#include "extensions/renderer/worker_thread_dispatcher.h"

namespace extensions {

ServiceWorkerData::ServiceWorkerData(WorkerThreadDispatcher* dispatcher,
                                     int embedded_worker_id)
    : embedded_worker_id_(embedded_worker_id),
      v8_schema_registry_(new V8SchemaRegistry),
      request_sender_(
          new ServiceWorkerRequestSender(dispatcher, embedded_worker_id)) {}

ServiceWorkerData::~ServiceWorkerData() {}

}  // namespace extensions
