// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_DISPATCHER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_DISPATCHER_H_

#include <map>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/common/console_message_level.h"
#include "ipc/ipc_listener.h"

struct EmbeddedWorkerMsg_StartWorker_Params;
class GURL;

namespace content {

// A tiny dispatcher which handles embedded worker start/stop messages.
class EmbeddedWorkerDispatcher : public IPC::Listener {
 public:
  EmbeddedWorkerDispatcher();
  ~EmbeddedWorkerDispatcher() override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const IPC::Message& message) override;

  void WorkerContextDestroyed(int embedded_worker_id);

 private:
  class WorkerWrapper;

  void OnStartWorker(const EmbeddedWorkerMsg_StartWorker_Params& params);
  void OnStopWorker(int embedded_worker_id);
  void OnResumeAfterDownload(int embedded_worker_id);
  void OnAddMessageToConsole(int embedded_worker_id,
                             ConsoleMessageLevel level,
                             const std::string& message);

  IDMap<WorkerWrapper, IDMapOwnPointer> workers_;
  std::map<int /* embedded_worker_id */, base::TimeTicks> stop_worker_times_;
  base::WeakPtrFactory<EmbeddedWorkerDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_DISPATCHER_H_
