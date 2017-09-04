// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_DISPATCHER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_DISPATCHER_H_

#include <map>
#include <memory>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/public/common/console_message_level.h"
#include "ipc/ipc_listener.h"

namespace blink {

class WebEmbeddedWorker;

}  // namespace blink

namespace content {

class EmbeddedWorkerDevToolsAgent;
class ServiceWorkerContextClient;
struct EmbeddedWorkerStartParams;

// A tiny dispatcher which handles embedded worker start/stop messages.
class EmbeddedWorkerDispatcher : public IPC::Listener {
 public:
  EmbeddedWorkerDispatcher();
  ~EmbeddedWorkerDispatcher() override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const IPC::Message& message) override;

  void WorkerContextDestroyed(int embedded_worker_id);

 private:
  friend class EmbeddedWorkerInstanceClientImpl;

  // A thin wrapper of WebEmbeddedWorker which also adds and releases process
  // references automatically.
  class WorkerWrapper {
   public:
    WorkerWrapper(blink::WebEmbeddedWorker* worker,
                  int devtools_agent_route_id);
    ~WorkerWrapper();

    blink::WebEmbeddedWorker* worker() { return worker_.get(); }

   private:
    ScopedChildProcessReference process_ref_;
    std::unique_ptr<blink::WebEmbeddedWorker> worker_;
    std::unique_ptr<EmbeddedWorkerDevToolsAgent> dev_tools_agent_;
  };

  void OnStartWorker(const EmbeddedWorkerStartParams& params);
  void OnStopWorker(int embedded_worker_id);
  void OnResumeAfterDownload(int embedded_worker_id);
  void OnAddMessageToConsole(int embedded_worker_id,
                             ConsoleMessageLevel level,
                             const std::string& message);

  // These methods are used by EmbeddedWorkerInstanceClientImpl to keep
  // consistency between chromium IPC and mojo IPC.
  // TODO(shimazu): Remove them after all messages for EmbeddedWorker are
  // replaced by mojo IPC. (Tracking issue: https://crbug.com/629701)
  std::unique_ptr<WorkerWrapper> StartWorkerContext(
      const EmbeddedWorkerStartParams& params,
      std::unique_ptr<ServiceWorkerContextClient> context_client);
  void RegisterWorker(int embedded_worker_id,
                      std::unique_ptr<WorkerWrapper> wrapper);
  void UnregisterWorker(int embedded_worker_id);
  void RecordStopWorkerTimer(int embedded_worker_id);

  IDMap<WorkerWrapper, IDMapOwnPointer> workers_;
  std::map<int /* embedded_worker_id */, base::TimeTicks> stop_worker_times_;
  base::WeakPtrFactory<EmbeddedWorkerDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_DISPATCHER_H_
