// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/worker_document_set.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/worker_service_observer.h"

namespace content {

WorkerService* WorkerService::GetInstance() {
  return SharedWorkerServiceImpl::GetInstance();
}

namespace {

class ScopedWorkerDependencyChecker {
 public:
  explicit ScopedWorkerDependencyChecker(SharedWorkerServiceImpl* service)
      : service_(service) {}
  ScopedWorkerDependencyChecker(SharedWorkerServiceImpl* service,
                                base::Closure done_closure)
      : service_(service), done_closure_(done_closure) {}
  ~ScopedWorkerDependencyChecker() {
    service_->CheckWorkerDependency();
    if (!done_closure_.is_null())
      done_closure_.Run();
  }

 private:
  SharedWorkerServiceImpl* service_;
  base::Closure done_closure_;
  DISALLOW_COPY_AND_ASSIGN(ScopedWorkerDependencyChecker);
};

void UpdateWorkerDependencyOnUI(const std::vector<int>& added_ids,
                                const std::vector<int>& removed_ids) {
  for (size_t i = 0; i < added_ids.size(); ++i) {
    RenderProcessHostImpl* render_process_host_impl =
        static_cast<RenderProcessHostImpl*>(
            RenderProcessHost::FromID(added_ids[i]));
    if (!render_process_host_impl)
      continue;
    render_process_host_impl->IncrementWorkerRefCount();
  }
  for (size_t i = 0; i < removed_ids.size(); ++i) {
    RenderProcessHostImpl* render_process_host_impl =
        static_cast<RenderProcessHostImpl*>(
            RenderProcessHost::FromID(removed_ids[i]));
    if (!render_process_host_impl)
      continue;
    render_process_host_impl->DecrementWorkerRefCount();
  }
}

void UpdateWorkerDependency(const std::vector<int>& added_ids,
                            const std::vector<int>& removed_ids) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UpdateWorkerDependencyOnUI, added_ids, removed_ids));
}

void DecrementWorkerRefCount(int process_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&DecrementWorkerRefCount, process_id));
    return;
  }
  RenderProcessHostImpl* render_process_host_impl =
      static_cast<RenderProcessHostImpl*>(
          RenderProcessHost::FromID(process_id));
  if (render_process_host_impl)
    render_process_host_impl->DecrementWorkerRefCount();
}

bool TryIncrementWorkerRefCount(int worker_process_id) {
  RenderProcessHostImpl* render_process = static_cast<RenderProcessHostImpl*>(
      RenderProcessHost::FromID(worker_process_id));
  if (!render_process || render_process->FastShutdownStarted()) {
    return false;
  }
  render_process->IncrementWorkerRefCount();
  return true;
}

}  // namespace

class SharedWorkerServiceImpl::SharedWorkerPendingInstance {
 public:
  struct SharedWorkerPendingRequest {
    SharedWorkerPendingRequest(SharedWorkerMessageFilter* filter,
                               int route_id,
                               unsigned long long document_id,
                               int render_process_id,
                               int render_frame_route_id)
        : filter(filter),
          route_id(route_id),
          document_id(document_id),
          render_process_id(render_process_id),
          render_frame_route_id(render_frame_route_id) {}
    SharedWorkerMessageFilter* const filter;
    const int route_id;
    const unsigned long long document_id;
    const int render_process_id;
    const int render_frame_route_id;
  };

  typedef ScopedVector<SharedWorkerPendingRequest> SharedWorkerPendingRequests;

  explicit SharedWorkerPendingInstance(
      std::unique_ptr<SharedWorkerInstance> instance)
      : instance_(std::move(instance)) {}
  ~SharedWorkerPendingInstance() {}
  SharedWorkerInstance* instance() { return instance_.get(); }
  SharedWorkerInstance* release_instance() { return instance_.release(); }
  SharedWorkerPendingRequests* requests() { return &requests_; }
  SharedWorkerMessageFilter* FindFilter(int process_id) {
    for (size_t i = 0; i < requests_.size(); ++i) {
      if (requests_[i]->render_process_id == process_id)
        return requests_[i]->filter;
    }
    return NULL;
  }
  void AddRequest(std::unique_ptr<SharedWorkerPendingRequest> request_info) {
    requests_.push_back(request_info.release());
  }
  void RemoveRequest(int process_id) {
    for (SharedWorkerPendingRequests::iterator request_itr = requests_.begin();
         request_itr != requests_.end();) {
      if ((*request_itr)->render_process_id == process_id)
        request_itr = requests_.erase(request_itr);
      else
        ++request_itr;
    }
  }
  void RegisterToSharedWorkerHost(SharedWorkerHost* host) {
    for (size_t i = 0; i < requests_.size(); ++i) {
      SharedWorkerPendingRequest* request = requests_[i];
      host->AddFilter(request->filter, request->route_id);
      host->worker_document_set()->Add(request->filter,
                                       request->document_id,
                                       request->render_process_id,
                                       request->render_frame_route_id);
    }
  }
  void SendWorkerCreatedMessages() {
    for (size_t i = 0; i < requests_.size(); ++i) {
      SharedWorkerPendingRequest* request = requests_[i];
      request->filter->Send(new ViewMsg_WorkerCreated(request->route_id));
    }
  }

 private:
  std::unique_ptr<SharedWorkerInstance> instance_;
  SharedWorkerPendingRequests requests_;
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerPendingInstance);
};

class SharedWorkerServiceImpl::SharedWorkerReserver
    : public base::RefCountedThreadSafe<SharedWorkerReserver> {
 public:
  SharedWorkerReserver(int pending_instance_id,
                       int worker_process_id,
                       int worker_route_id,
                       bool is_new_worker,
                       const SharedWorkerInstance& instance)
      : worker_process_id_(worker_process_id),
        worker_route_id_(worker_route_id),
        is_new_worker_(is_new_worker),
        instance_(instance) {}

  void TryReserve(const base::Callback<void(bool)>& success_cb,
                  const base::Closure& failure_cb,
                  bool (*try_increment_worker_ref_count)(int)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!try_increment_worker_ref_count(worker_process_id_)) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, failure_cb);
      return;
    }
    bool pause_on_start = false;
    if (is_new_worker_) {
      pause_on_start =
          SharedWorkerDevToolsManager::GetInstance()->WorkerCreated(
              worker_process_id_, worker_route_id_, instance_);
    }
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(success_cb, pause_on_start));
  }

 private:
  friend class base::RefCountedThreadSafe<SharedWorkerReserver>;
  ~SharedWorkerReserver() {}

  const int worker_process_id_;
  const int worker_route_id_;
  const bool is_new_worker_;
  const SharedWorkerInstance instance_;
};

// static
bool (*SharedWorkerServiceImpl::s_try_increment_worker_ref_count_)(int) =
    TryIncrementWorkerRefCount;

SharedWorkerServiceImpl* SharedWorkerServiceImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::Singleton<SharedWorkerServiceImpl>::get();
}

SharedWorkerServiceImpl::SharedWorkerServiceImpl()
    : update_worker_dependency_(UpdateWorkerDependency),
      next_pending_instance_id_(0) {
}

SharedWorkerServiceImpl::~SharedWorkerServiceImpl() {}

void SharedWorkerServiceImpl::ResetForTesting() {
  last_worker_depended_renderers_.clear();
  worker_hosts_.clear();
  observers_.Clear();
  update_worker_dependency_ = UpdateWorkerDependency;
  s_try_increment_worker_ref_count_ = TryIncrementWorkerRefCount;
}

bool SharedWorkerServiceImpl::TerminateWorker(int process_id, int route_id) {
  SharedWorkerHost* host =
      worker_hosts_.get(std::make_pair(process_id, route_id));
  if (!host || !host->instance())
    return false;
  host->TerminateWorker();
  return true;
}

std::vector<WorkerService::WorkerInfo> SharedWorkerServiceImpl::GetWorkers() {
  std::vector<WorkerService::WorkerInfo> results;
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    SharedWorkerHost* host = iter->second;
    const SharedWorkerInstance* instance = host->instance();
    if (instance) {
      WorkerService::WorkerInfo info;
      info.url = instance->url();
      info.name = instance->name();
      info.route_id = host->worker_route_id();
      info.process_id = host->process_id();
      info.handle = host->container_render_filter()->PeerHandle();
      results.push_back(info);
    }
  }
  return results;
}

void SharedWorkerServiceImpl::AddObserver(WorkerServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observers_.AddObserver(observer);
}

void SharedWorkerServiceImpl::RemoveObserver(WorkerServiceObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observers_.RemoveObserver(observer);
}

void SharedWorkerServiceImpl::CreateWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    int route_id,
    SharedWorkerMessageFilter* filter,
    ResourceContext* resource_context,
    const WorkerStoragePartitionId& partition_id,
    blink::WebWorkerCreationError* creation_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  *creation_error = blink::WebWorkerCreationErrorNone;
  std::unique_ptr<SharedWorkerInstance> instance(new SharedWorkerInstance(
      params.url, params.name, params.content_security_policy,
      params.security_policy_type, params.creation_address_space,
      resource_context, partition_id, params.creation_context_type));
  std::unique_ptr<SharedWorkerPendingInstance::SharedWorkerPendingRequest>
      request(new SharedWorkerPendingInstance::SharedWorkerPendingRequest(
          filter, route_id, params.document_id, filter->render_process_id(),
          params.render_frame_route_id));
  if (SharedWorkerPendingInstance* pending = FindPendingInstance(*instance)) {
    if (params.url != pending->instance()->url()) {
      *creation_error = blink::WebWorkerCreationErrorURLMismatch;
      return;
    }
    if (params.creation_context_type !=
        pending->instance()->creation_context_type()) {
      *creation_error = blink::WebWorkerCreationErrorSecureContextMismatch;
    }
    pending->AddRequest(std::move(request));
    return;
  }
  std::unique_ptr<SharedWorkerPendingInstance> pending_instance(
      new SharedWorkerPendingInstance(std::move(instance)));
  pending_instance->AddRequest(std::move(request));
  ReserveRenderProcessToCreateWorker(std::move(pending_instance),
                                     creation_error);
}

void SharedWorkerServiceImpl::ForwardToWorker(
    const IPC::Message& message,
    SharedWorkerMessageFilter* filter) {
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    if (iter->second->FilterMessage(message, filter))
      return;
  }
}

void SharedWorkerServiceImpl::DocumentDetached(
    unsigned long long document_id,
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    iter->second->DocumentDetached(filter, document_id);
  }
}

void SharedWorkerServiceImpl::WorkerContextClosed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->WorkerContextClosed();
}

void SharedWorkerServiceImpl::WorkerContextDestroyed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  std::unique_ptr<SharedWorkerHost> host = worker_hosts_.take_and_erase(
      std::make_pair(filter->render_process_id(), worker_route_id));
  if (!host)
    return;
  host->WorkerContextDestroyed();
}

void SharedWorkerServiceImpl::WorkerReadyForInspection(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->WorkerReadyForInspection();
}

void SharedWorkerServiceImpl::WorkerScriptLoaded(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->WorkerScriptLoaded();
}

void SharedWorkerServiceImpl::WorkerScriptLoadFailed(
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  std::unique_ptr<SharedWorkerHost> host = worker_hosts_.take_and_erase(
      std::make_pair(filter->render_process_id(), worker_route_id));
  if (!host)
    return;
  host->WorkerScriptLoadFailed();
}

void SharedWorkerServiceImpl::WorkerConnected(
    int message_port_id,
    int worker_route_id,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->WorkerConnected(message_port_id);
}

void SharedWorkerServiceImpl::AllowFileSystem(
    int worker_route_id,
    const GURL& url,
    IPC::Message* reply_msg,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id)) {
    host->AllowFileSystem(url, base::WrapUnique(reply_msg));
  } else {
    filter->Send(reply_msg);
    return;
  }
}

void SharedWorkerServiceImpl::AllowIndexedDB(
    int worker_route_id,
    const GURL& url,
    const base::string16& name,
    bool* result,
    SharedWorkerMessageFilter* filter) {
  if (SharedWorkerHost* host = FindSharedWorkerHost(filter, worker_route_id))
    host->AllowIndexedDB(url, name, result);
  else
    *result = false;
}

void SharedWorkerServiceImpl::OnSharedWorkerMessageFilterClosing(
    SharedWorkerMessageFilter* filter) {
  ScopedWorkerDependencyChecker checker(this);
  std::vector<ProcessRouteIdPair> remove_list;
  for (WorkerHostMap::iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    iter->second->FilterShutdown(filter);
    if (iter->first.first == filter->render_process_id())
      remove_list.push_back(iter->first);
  }
  for (size_t i = 0; i < remove_list.size(); ++i) {
    std::unique_ptr<SharedWorkerHost> host =
        worker_hosts_.take_and_erase(remove_list[i]);
  }

  std::vector<int> remove_pending_instance_list;
  for (PendingInstanceMap::iterator iter = pending_instances_.begin();
       iter != pending_instances_.end(); ++iter) {
    iter->second->RemoveRequest(filter->render_process_id());
    if (!iter->second->requests()->size())
      remove_pending_instance_list.push_back(iter->first);
  }
  for (size_t i = 0; i < remove_pending_instance_list.size(); ++i)
    pending_instances_.take_and_erase(remove_pending_instance_list[i]);
}

void SharedWorkerServiceImpl::NotifyWorkerDestroyed(int worker_process_id,
                                                    int worker_route_id) {
  FOR_EACH_OBSERVER(WorkerServiceObserver,
                    observers_,
                    WorkerDestroyed(worker_process_id, worker_route_id));
}

void SharedWorkerServiceImpl::ReserveRenderProcessToCreateWorker(
    std::unique_ptr<SharedWorkerPendingInstance> pending_instance,
    blink::WebWorkerCreationError* creation_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!FindPendingInstance(*pending_instance->instance()));
  if (creation_error)
    *creation_error = blink::WebWorkerCreationErrorNone;
  if (!pending_instance->requests()->size())
    return;
  int worker_process_id = -1;
  int worker_route_id = MSG_ROUTING_NONE;
  bool is_new_worker = true;
  SharedWorkerHost* host = FindSharedWorkerHost(*pending_instance->instance());
  if (host) {
    if (pending_instance->instance()->url() != host->instance()->url()) {
      if (creation_error)
        *creation_error = blink::WebWorkerCreationErrorURLMismatch;
      return;
    }
    if (pending_instance->instance()->creation_context_type() !=
        host->instance()->creation_context_type()) {
      if (creation_error)
        *creation_error = blink::WebWorkerCreationErrorSecureContextMismatch;
    }
    worker_process_id = host->process_id();
    worker_route_id = host->worker_route_id();
    is_new_worker = false;
  } else {
    SharedWorkerMessageFilter* first_filter =
        (*pending_instance->requests()->begin())->filter;
    worker_process_id = first_filter->render_process_id();
    worker_route_id = first_filter->GetNextRoutingID();
  }
  const int pending_instance_id = next_pending_instance_id_++;
  scoped_refptr<SharedWorkerReserver> reserver(
      new SharedWorkerReserver(pending_instance_id,
                               worker_process_id,
                               worker_route_id,
                               is_new_worker,
                               *pending_instance->instance()));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &SharedWorkerReserver::TryReserve,
          reserver,
          base::Bind(&SharedWorkerServiceImpl::RenderProcessReservedCallback,
                     base::Unretained(this),
                     pending_instance_id,
                     worker_process_id,
                     worker_route_id,
                     is_new_worker),
          base::Bind(
              &SharedWorkerServiceImpl::RenderProcessReserveFailedCallback,
              base::Unretained(this),
              pending_instance_id,
              worker_process_id,
              worker_route_id,
              is_new_worker),
          s_try_increment_worker_ref_count_));
  pending_instances_.set(pending_instance_id, std::move(pending_instance));
}

void SharedWorkerServiceImpl::RenderProcessReservedCallback(
    int pending_instance_id,
    int worker_process_id,
    int worker_route_id,
    bool is_new_worker,
    bool pause_on_start) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // To offset the TryIncrementWorkerRefCount called for the reservation,
  // calls DecrementWorkerRefCount after CheckWorkerDependency in
  // ScopeWorkerDependencyChecker's destructor.
  ScopedWorkerDependencyChecker checker(
      this, base::Bind(&DecrementWorkerRefCount, worker_process_id));
  std::unique_ptr<SharedWorkerPendingInstance> pending_instance =
      pending_instances_.take_and_erase(pending_instance_id);
  if (!pending_instance)
    return;
  if (!is_new_worker) {
    SharedWorkerHost* existing_host =
        worker_hosts_.get(std::make_pair(worker_process_id, worker_route_id));
    if (!existing_host) {
      // Retry reserving a renderer process if the existed Shared Worker was
      // destroyed on IO thread while reserving the renderer process on UI
      // thread.
      ReserveRenderProcessToCreateWorker(std::move(pending_instance), NULL);
      return;
    }
    pending_instance->RegisterToSharedWorkerHost(existing_host);
    pending_instance->SendWorkerCreatedMessages();
    return;
  }
  SharedWorkerMessageFilter* filter =
      pending_instance->FindFilter(worker_process_id);
  if (!filter) {
    pending_instance->RemoveRequest(worker_process_id);
    // Retry reserving a renderer process if the requested renderer process was
    // destroyed on IO thread while reserving the renderer process on UI thread.
    ReserveRenderProcessToCreateWorker(std::move(pending_instance), NULL);
    return;
  }
  std::unique_ptr<SharedWorkerHost> host(new SharedWorkerHost(
      pending_instance->release_instance(), filter, worker_route_id));
  pending_instance->RegisterToSharedWorkerHost(host.get());
  const GURL url = host->instance()->url();
  const base::string16 name = host->instance()->name();
  host->Start(pause_on_start);
  worker_hosts_.set(std::make_pair(worker_process_id, worker_route_id),
                    std::move(host));
  FOR_EACH_OBSERVER(
      WorkerServiceObserver,
      observers_,
      WorkerCreated(url, name, worker_process_id, worker_route_id));
}

void SharedWorkerServiceImpl::RenderProcessReserveFailedCallback(
    int pending_instance_id,
    int worker_process_id,
    int worker_route_id,
    bool is_new_worker) {
  worker_hosts_.take_and_erase(
      std::make_pair(worker_process_id, worker_route_id));
  std::unique_ptr<SharedWorkerPendingInstance> pending_instance =
      pending_instances_.take_and_erase(pending_instance_id);
  if (!pending_instance)
    return;
  pending_instance->RemoveRequest(worker_process_id);
  // Retry reserving a renderer process.
  ReserveRenderProcessToCreateWorker(std::move(pending_instance), NULL);
}

SharedWorkerHost* SharedWorkerServiceImpl::FindSharedWorkerHost(
    SharedWorkerMessageFilter* filter,
    int worker_route_id) {
  return worker_hosts_.get(std::make_pair(filter->render_process_id(),
                                          worker_route_id));
}

SharedWorkerHost* SharedWorkerServiceImpl::FindSharedWorkerHost(
    const SharedWorkerInstance& instance) {
  for (WorkerHostMap::const_iterator iter = worker_hosts_.begin();
       iter != worker_hosts_.end();
       ++iter) {
    SharedWorkerHost* host = iter->second;
    if (host->IsAvailable() && host->instance()->Matches(instance))
      return iter->second;
  }
  return NULL;
}

SharedWorkerServiceImpl::SharedWorkerPendingInstance*
SharedWorkerServiceImpl::FindPendingInstance(
    const SharedWorkerInstance& instance) {
  for (PendingInstanceMap::iterator iter = pending_instances_.begin();
       iter != pending_instances_.end(); ++iter) {
    if (iter->second->instance()->Matches(instance))
      return iter->second;
  }
  return NULL;
}

const std::set<int>
SharedWorkerServiceImpl::GetRenderersWithWorkerDependency() {
  std::set<int> dependent_renderers;
  for (WorkerHostMap::iterator host_iter = worker_hosts_.begin();
       host_iter != worker_hosts_.end();
       ++host_iter) {
    const int process_id = host_iter->first.first;
    if (dependent_renderers.count(process_id))
      continue;
    if (host_iter->second->instance() &&
        host_iter->second->worker_document_set()->ContainsExternalRenderer(
            process_id)) {
      dependent_renderers.insert(process_id);
    }
  }
  return dependent_renderers;
}

void SharedWorkerServiceImpl::CheckWorkerDependency() {
  const std::set<int> current_worker_depended_renderers =
      GetRenderersWithWorkerDependency();
  std::vector<int> added_items = base::STLSetDifference<std::vector<int> >(
      current_worker_depended_renderers, last_worker_depended_renderers_);
  std::vector<int> removed_items = base::STLSetDifference<std::vector<int> >(
      last_worker_depended_renderers_, current_worker_depended_renderers);
  if (!added_items.empty() || !removed_items.empty()) {
    last_worker_depended_renderers_ = current_worker_depended_renderers;
    update_worker_dependency_(added_items, removed_items);
  }
}

void SharedWorkerServiceImpl::ChangeUpdateWorkerDependencyFuncForTesting(
    UpdateWorkerDependencyFunc new_func) {
  update_worker_dependency_ = new_func;
}

void SharedWorkerServiceImpl::ChangeTryIncrementWorkerRefCountFuncForTesting(
    bool (*new_func)(int)) {
  s_try_increment_worker_ref_count_ = new_func;
}

}  // namespace content
