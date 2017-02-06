// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/worker_service.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationErrors.h"

struct ViewHostMsg_CreateWorker_Params;

namespace IPC {
class Message;
}

namespace content {

class SharedWorkerInstance;
class SharedWorkerHost;
class SharedWorkerMessageFilter;
class ResourceContext;
class WorkerServiceObserver;
class WorkerStoragePartitionId;

// The implementation of WorkerService. We try to place workers in an existing
// renderer process when possible.
class CONTENT_EXPORT SharedWorkerServiceImpl
    : public NON_EXPORTED_BASE(WorkerService) {
 public:
  // Returns the SharedWorkerServiceImpl singleton.
  static SharedWorkerServiceImpl* GetInstance();

  // WorkerService implementation:
  bool TerminateWorker(int process_id, int route_id) override;
  std::vector<WorkerInfo> GetWorkers() override;
  void AddObserver(WorkerServiceObserver* observer) override;
  void RemoveObserver(WorkerServiceObserver* observer) override;

  // These methods correspond to worker related IPCs.
  void CreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                    int route_id,
                    SharedWorkerMessageFilter* filter,
                    ResourceContext* resource_context,
                    const WorkerStoragePartitionId& partition_id,
                    blink::WebWorkerCreationError* creation_error);
  void ForwardToWorker(const IPC::Message& message,
                       SharedWorkerMessageFilter* filter);
  void DocumentDetached(unsigned long long document_id,
                        SharedWorkerMessageFilter* filter);
  void WorkerContextClosed(int worker_route_id,
                           SharedWorkerMessageFilter* filter);
  void WorkerContextDestroyed(int worker_route_id,
                              SharedWorkerMessageFilter* filter);
  void WorkerReadyForInspection(int worker_route_id,
                                SharedWorkerMessageFilter* filter);
  void WorkerScriptLoaded(int worker_route_id,
                          SharedWorkerMessageFilter* filter);
  void WorkerScriptLoadFailed(int worker_route_id,
                              SharedWorkerMessageFilter* filter);
  void WorkerConnected(int message_port_id,
                       int worker_route_id,
                       SharedWorkerMessageFilter* filter);
  void AllowFileSystem(int worker_route_id,
                       const GURL& url,
                       IPC::Message* reply_msg,
                       SharedWorkerMessageFilter* filter);
  void AllowIndexedDB(int worker_route_id,
                      const GURL& url,
                      const base::string16& name,
                      bool* result,
                      SharedWorkerMessageFilter* filter);

  void OnSharedWorkerMessageFilterClosing(
      SharedWorkerMessageFilter* filter);

  // Checks the worker dependency of renderer processes and calls
  // IncrementWorkerRefCount and DecrementWorkerRefCount of
  // RenderProcessHostImpl on UI thread if necessary.
  void CheckWorkerDependency();

  void NotifyWorkerDestroyed(int worker_process_id, int worker_route_id);

 private:
  class SharedWorkerPendingInstance;
  class SharedWorkerReserver;

  friend struct base::DefaultSingletonTraits<SharedWorkerServiceImpl>;
  friend class SharedWorkerServiceImplTest;

  typedef void (*UpdateWorkerDependencyFunc)(const std::vector<int>&,
                                             const std::vector<int>&);
  typedef bool (*TryIncrementWorkerRefCountFunc)(bool);
  // Pair of render_process_id and worker_route_id.
  typedef std::pair<int, int> ProcessRouteIdPair;
  typedef base::ScopedPtrHashMap<ProcessRouteIdPair,
                                 std::unique_ptr<SharedWorkerHost>>
      WorkerHostMap;
  typedef base::ScopedPtrHashMap<int,
                                 std::unique_ptr<SharedWorkerPendingInstance>>
      PendingInstanceMap;

  SharedWorkerServiceImpl();
  ~SharedWorkerServiceImpl() override;

  void ResetForTesting();

  // Reserves the render process to create Shared Worker. This reservation
  // procedure will be executed on UI thread and
  // RenderProcessReservedCallback() or RenderProcessReserveFailedCallback()
  // will be called on IO thread.
  void ReserveRenderProcessToCreateWorker(
      std::unique_ptr<SharedWorkerPendingInstance> pending_instance,
      blink::WebWorkerCreationError* creation_error);

  // Called after the render process is reserved to create Shared Worker in it.
  void RenderProcessReservedCallback(int pending_instance_id,
                                     int worker_process_id,
                                     int worker_route_id,
                                     bool is_new_worker,
                                     bool pause_on_start);

  // Called after the fast shutdown is detected while reserving the render
  // process to create Shared Worker in it.
  void RenderProcessReserveFailedCallback(int pending_instance_id,
                                          int worker_process_id,
                                          int worker_route_id,
                                          bool is_new_worker);

  SharedWorkerHost* FindSharedWorkerHost(
      SharedWorkerMessageFilter* filter,
      int worker_route_id);

  SharedWorkerHost* FindSharedWorkerHost(const SharedWorkerInstance& instance);
  SharedWorkerPendingInstance* FindPendingInstance(
      const SharedWorkerInstance& instance);

  // Returns the IDs of the renderer processes which are executing
  // SharedWorkers connected to other renderer processes.
  const std::set<int> GetRenderersWithWorkerDependency();

  void ChangeUpdateWorkerDependencyFuncForTesting(
      UpdateWorkerDependencyFunc new_func);
  void ChangeTryIncrementWorkerRefCountFuncForTesting(bool (*new_func)(int));

  std::set<int> last_worker_depended_renderers_;
  // Function ptr to update worker dependency, tests may override this.
  UpdateWorkerDependencyFunc update_worker_dependency_;

  // Function ptr to increment worker ref count, tests may override this.
  static bool (*s_try_increment_worker_ref_count_)(int);

  WorkerHostMap worker_hosts_;
  PendingInstanceMap pending_instances_;
  int next_pending_instance_id_;

  base::ObserverList<WorkerServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_
