// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_EMBEDDED_WORKER_DISPATCHER_H_
#define CONTENT_CHILD_SERVICE_WORKER_EMBEDDED_WORKER_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"

struct EmbeddedWorkerMsg_StartWorker_Params;
class GURL;

namespace WebKit {
class WebEmbeddedWorker;
}

namespace content {

// A tiny dispatcher which handles embedded worker start/stop messages.
class EmbeddedWorkerDispatcher : public IPC::Listener {
 public:
  EmbeddedWorkerDispatcher();
  virtual ~EmbeddedWorkerDispatcher();

  // IPC::Listener overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void WorkerContextDestroyed(int embedded_worker_id);

 private:
  class WorkerWrapper;

  void OnStartWorker(const EmbeddedWorkerMsg_StartWorker_Params& params);
  void OnStopWorker(int embedded_worker_id);

  IDMap<WorkerWrapper, IDMapOwnPointer> workers_;
  base::WeakPtrFactory<EmbeddedWorkerDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_EMBEDDED_WORKER_DISPATCHER_H_
