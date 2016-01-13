// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_H_

#include <list>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/content_export.h"

namespace content {

class DevToolsAgentHost;

// All methods are supposed to be called on the IO thread.
// This class is not used when "enable-embedded-shared-worker" flag is set.
class WorkerDevToolsManager {
 public:
  typedef std::pair<int, int> WorkerId;
  class WorkerDevToolsAgentHost;

  // Returns the WorkerDevToolsManager singleton.
  static WorkerDevToolsManager* GetInstance();

  // Called on the UI thread.
  static DevToolsAgentHost* GetDevToolsAgentHostForWorker(
      int worker_process_id,
      int worker_route_id);

  void ForwardToDevToolsClient(int worker_process_id,
                               int worker_route_id,
                               const std::string& message);
  void SaveAgentRuntimeState(int worker_process_id,
                             int worker_route_id,
                             const std::string& state);

  // Called on the IO thread.
  // Returns true when the worker must be paused on start.
  bool WorkerCreated(WorkerProcessHost* process,
                     const WorkerProcessHost::WorkerInstance& instance);
  void WorkerDestroyed(WorkerProcessHost* process, int worker_route_id);
  void WorkerContextStarted(WorkerProcessHost* process, int worker_route_id);

 private:
  friend struct DefaultSingletonTraits<WorkerDevToolsManager>;
  class DetachedClientHosts;
  struct InspectedWorker;
  typedef std::list<InspectedWorker> InspectedWorkersList;

  WorkerDevToolsManager();
  virtual ~WorkerDevToolsManager();

  void RemoveInspectedWorkerData(const WorkerId& id);
  InspectedWorkersList::iterator FindInspectedWorker(int host_id, int route_id);

  void ConnectDevToolsAgentHostToWorker(int worker_process_id,
                                        int worker_route_id);
  void ForwardToWorkerDevToolsAgent(int worker_process_host_id,
                                    int worker_route_id,
                                    const IPC::Message& message);
  static void ForwardToDevToolsClientOnUIThread(
      int worker_process_id,
      int worker_route_id,
      const std::string& message);
  static void SaveAgentRuntimeStateOnUIThread(
      int worker_process_id,
      int worker_route_id,
      const std::string& state);
  static void NotifyConnectionFailedOnIOThread(int worker_process_id,
                                               int worker_route_id);
  static void NotifyConnectionFailedOnUIThread(int worker_process_id,
                                               int worker_route_id);
  static void SendResumeToWorker(const WorkerId& id);

  InspectedWorkersList inspected_workers_;

  struct TerminatedInspectedWorker;
  typedef std::list<TerminatedInspectedWorker> TerminatedInspectedWorkers;
  // List of terminated workers for which there may be a devtools client on
  // the UI thread. Worker entry is added into this list when inspected worker
  // is terminated and will be removed in one of two cases:
  // - shared worker with the same URL and name is started(in wich case we will
  // try to reattach existing DevTools client to the new worker).
  // - DevTools client which was inspecting terminated worker is closed on the
  // UI thread and and WorkerDevToolsManager is notified about that on the IO
  // thread.
  TerminatedInspectedWorkers terminated_workers_;

  typedef std::map<WorkerId, WorkerId> PausedWorkers;
  // Map from old to new worker id for the inspected workers that have been
  // terminated and started again in paused state. Worker data will be removed
  // from this list in one of two cases:
  // - DevTools client is closed on the UI thread, WorkerDevToolsManager was
  // notified about that on the IO thread and sent "resume" message to the
  // worker.
  // - Existing DevTools client was reattached to the new worker.
  PausedWorkers paused_workers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_H_
