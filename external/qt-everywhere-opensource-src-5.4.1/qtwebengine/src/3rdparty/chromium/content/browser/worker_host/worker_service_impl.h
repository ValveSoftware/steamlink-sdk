// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/worker_service.h"

class GURL;
struct ViewHostMsg_CreateWorker_Params;

namespace content {
class ResourceContext;
class WorkerServiceObserver;
class WorkerStoragePartition;
class WorkerPrioritySetter;

class CONTENT_EXPORT WorkerServiceImpl
    : public NON_EXPORTED_BASE(WorkerService) {
 public:
  // Returns the WorkerServiceImpl singleton.
  static WorkerServiceImpl* GetInstance();

  // Releases the priority setter to avoid memory leak error.
  void PerformTeardownForTesting();

  // WorkerService implementation:
  virtual bool TerminateWorker(int process_id, int route_id) OVERRIDE;
  virtual std::vector<WorkerInfo> GetWorkers() OVERRIDE;
  virtual void AddObserver(WorkerServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(WorkerServiceObserver* observer) OVERRIDE;

  // These methods correspond to worker related IPCs.
  void CreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                    int route_id,
                    WorkerMessageFilter* filter,
                    ResourceContext* resource_context,
                    const WorkerStoragePartition& worker_partition,
                    bool* url_mismatch);
  void ForwardToWorker(const IPC::Message& message,
                       WorkerMessageFilter* filter);
  void DocumentDetached(unsigned long long document_id,
                        WorkerMessageFilter* filter);

  void OnWorkerMessageFilterClosing(WorkerMessageFilter* filter);

  int next_worker_route_id() { return ++next_worker_route_id_; }

  // Given a worker's process id, return the IDs of the renderer process and
  // render frame that created it.  For shared workers, this returns the first
  // parent.
  // TODO(dimich): This code assumes there is 1 worker per worker process, which
  // is how it is today until V8 can run in separate threads.
  bool GetRendererForWorker(int worker_process_id,
                            int* render_process_id,
                            int* render_frame_id) const;
  const WorkerProcessHost::WorkerInstance* FindWorkerInstance(
      int worker_process_id);

  void NotifyWorkerDestroyed(
      WorkerProcessHost* process,
      int worker_route_id);

  void NotifyWorkerProcessCreated();

  // Used when we run each worker in a separate process.
  static const int kMaxWorkersWhenSeparate;
  static const int kMaxWorkersPerFrameWhenSeparate;

 private:
  friend struct DefaultSingletonTraits<WorkerServiceImpl>;

  WorkerServiceImpl();
  virtual ~WorkerServiceImpl();

  // Given a WorkerInstance, create an associated worker process.
  bool CreateWorkerFromInstance(WorkerProcessHost::WorkerInstance instance);

  // Checks if we can create a worker process based on the process limit when
  // we're using a strategy of one process per core.
  bool CanCreateWorkerProcess(
      const WorkerProcessHost::WorkerInstance& instance);

  // Checks if the frame associated with the passed RenderFrame can create a
  // worker process based on the process limit when we're using a strategy of
  // one worker per process.
  bool FrameCanCreateWorkerProcess(
      int render_process_id, int render_frame_id, bool* hit_total_worker_limit);

  // Tries to see if any of the queued workers can be created.
  void TryStartingQueuedWorker();

  WorkerProcessHost::WorkerInstance* FindSharedWorkerInstance(
      const GURL& url,
      const base::string16& name,
      const WorkerStoragePartition& worker_partition,
      ResourceContext* resource_context);

  scoped_refptr<WorkerPrioritySetter> priority_setter_;

  int next_worker_route_id_;

  WorkerProcessHost::Instances queued_workers_;

  ObserverList<WorkerServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WorkerServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SERVICE_H_
