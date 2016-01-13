// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_service_impl.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/browser/worker_host/worker_message_filter.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/worker_service_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"

namespace content {

namespace {
void AddRenderFrameID(std::set<std::pair<int, int> >* visible_frame_ids,
                      RenderFrameHost* rfh) {
  visible_frame_ids->insert(
      std::pair<int, int>(rfh->GetProcess()->GetID(),
                          rfh->GetRoutingID()));
}
}

const int WorkerServiceImpl::kMaxWorkersWhenSeparate = 64;
const int WorkerServiceImpl::kMaxWorkersPerFrameWhenSeparate = 16;

class WorkerPrioritySetter
    : public NotificationObserver,
      public base::RefCountedThreadSafe<WorkerPrioritySetter,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  WorkerPrioritySetter();

  // Posts a task to the UI thread to register to receive notifications.
  void Initialize();

  // Invoked by WorkerServiceImpl when a worker process is created.
  void NotifyWorkerProcessCreated();

 private:
  friend class base::RefCountedThreadSafe<WorkerPrioritySetter>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<WorkerPrioritySetter>;
  virtual ~WorkerPrioritySetter();

  // Posts a task to perform a worker priority update.
  void PostTaskToGatherAndUpdateWorkerPriorities();

  // Gathers up a list of the visible tabs and then updates priorities for
  // all the shared workers.
  void GatherVisibleIDsAndUpdateWorkerPriorities();

  // Registers as an observer to receive notifications about
  // widgets being shown.
  void RegisterObserver();

  // Sets priorities for shared workers given a set of visible frames (as a
  // std::set of std::pair<render_process, render_frame> ids.
  void UpdateWorkerPrioritiesFromVisibleSet(
      const std::set<std::pair<int, int> >* visible);

  // Called to refresh worker priorities when focus changes between tabs.
  void OnRenderWidgetVisibilityChanged(std::pair<int, int>);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;
};

WorkerPrioritySetter::WorkerPrioritySetter() {
}

WorkerPrioritySetter::~WorkerPrioritySetter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void WorkerPrioritySetter::Initialize() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WorkerPrioritySetter::RegisterObserver, this));
}

void WorkerPrioritySetter::NotifyWorkerProcessCreated() {
  PostTaskToGatherAndUpdateWorkerPriorities();
}

void WorkerPrioritySetter::PostTaskToGatherAndUpdateWorkerPriorities() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &WorkerPrioritySetter::GatherVisibleIDsAndUpdateWorkerPriorities,
          this));
}

void WorkerPrioritySetter::GatherVisibleIDsAndUpdateWorkerPriorities() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<std::pair<int, int> >* visible_frame_ids =
      new std::set<std::pair<int, int> >();

  // Gather up all the visible renderer process/view pairs
  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (widget->GetProcess()->VisibleWidgetCount() == 0)
      continue;
    if (!widget->IsRenderView())
      continue;

    RenderWidgetHostView* widget_view = widget->GetView();
    if (!widget_view || !widget_view->IsShowing())
      continue;
    RenderViewHost* rvh = RenderViewHost::From(widget);
    WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
    if (!web_contents)
      continue;
    web_contents->ForEachFrame(
        base::Bind(&AddRenderFrameID, visible_frame_ids));
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&WorkerPrioritySetter::UpdateWorkerPrioritiesFromVisibleSet,
                 this, base::Owned(visible_frame_ids)));
}

void WorkerPrioritySetter::UpdateWorkerPrioritiesFromVisibleSet(
    const std::set<std::pair<int, int> >* visible_frame_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (!iter->process_launched())
      continue;
    bool throttle = true;

    for (WorkerProcessHost::Instances::const_iterator instance =
        iter->instances().begin(); instance != iter->instances().end();
        ++instance) {

      // This code assumes one worker per process
      WorkerProcessHost::Instances::const_iterator first_instance =
          iter->instances().begin();
      if (first_instance == iter->instances().end())
        continue;

      WorkerDocumentSet::DocumentInfoSet::const_iterator info =
          first_instance->worker_document_set()->documents().begin();

      for (; info != first_instance->worker_document_set()->documents().end();
          ++info) {
        std::pair<int, int> id(
            info->render_process_id(), info->render_frame_id());
        if (visible_frame_ids->find(id) != visible_frame_ids->end()) {
          throttle = false;
          break;
        }
      }

      if (!throttle ) {
        break;
      }
    }

    iter->SetBackgrounded(throttle);
  }
}

void WorkerPrioritySetter::OnRenderWidgetVisibilityChanged(
    std::pair<int, int> id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::set<std::pair<int, int> > visible_frame_ids;

  visible_frame_ids.insert(id);

  UpdateWorkerPrioritiesFromVisibleSet(&visible_frame_ids);
}

void WorkerPrioritySetter::RegisterObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this, NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllBrowserContextsAndSources());
}

void WorkerPrioritySetter::Observe(int type,
    const NotificationSource& source, const NotificationDetails& details) {
  if (type == NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED) {
    bool visible = *Details<bool>(details).ptr();

    if (visible) {
      int render_widget_id =
          Source<RenderWidgetHost>(source).ptr()->GetRoutingID();
      int render_process_pid =
          Source<RenderWidgetHost>(source).ptr()->GetProcess()->GetID();

      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&WorkerPrioritySetter::OnRenderWidgetVisibilityChanged,
              this, std::pair<int, int>(render_process_pid, render_widget_id)));
    }
  }
  else if (type == NOTIFICATION_RENDERER_PROCESS_CREATED) {
    PostTaskToGatherAndUpdateWorkerPriorities();
  }
}

WorkerService* WorkerService::GetInstance() {
  if (EmbeddedSharedWorkerEnabled())
    return SharedWorkerServiceImpl::GetInstance();
  else
    return WorkerServiceImpl::GetInstance();
}

bool WorkerService::EmbeddedSharedWorkerEnabled() {
  static bool disabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableEmbeddedSharedWorker);
  return !disabled;
}

WorkerServiceImpl* WorkerServiceImpl::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<WorkerServiceImpl>::get();
}

WorkerServiceImpl::WorkerServiceImpl()
    : priority_setter_(new WorkerPrioritySetter()),
      next_worker_route_id_(0) {
  priority_setter_->Initialize();
}

WorkerServiceImpl::~WorkerServiceImpl() {
  // The observers in observers_ can't be used here because they might be
  // gone already.
}

void WorkerServiceImpl::PerformTeardownForTesting() {
  priority_setter_ = NULL;
}

void WorkerServiceImpl::OnWorkerMessageFilterClosing(
    WorkerMessageFilter* filter) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    iter->FilterShutdown(filter);
  }

  // See if that process had any queued workers.
  for (WorkerProcessHost::Instances::iterator i = queued_workers_.begin();
       i != queued_workers_.end();) {
    i->RemoveFilters(filter);
    if (i->NumFilters() == 0) {
      i = queued_workers_.erase(i);
    } else {
      ++i;
    }
  }

  // Either a worker proceess has shut down, in which case we can start one of
  // the queued workers, or a renderer has shut down, in which case it doesn't
  // affect anything.  We call this function in both scenarios because then we
  // don't have to keep track which filters are from worker processes.
  TryStartingQueuedWorker();
}

void WorkerServiceImpl::CreateWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    int route_id,
    WorkerMessageFilter* filter,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition,
    bool* url_mismatch) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  *url_mismatch = false;
  WorkerProcessHost::WorkerInstance* existing_instance =
      FindSharedWorkerInstance(
          params.url, params.name, partition, resource_context);
  if (existing_instance) {
    if (params.url != existing_instance->url()) {
      *url_mismatch = true;
      return;
    }
    if (existing_instance->load_failed()) {
      filter->Send(new ViewMsg_WorkerScriptLoadFailed(route_id));
      return;
    }
    existing_instance->AddFilter(filter, route_id);
    existing_instance->worker_document_set()->Add(
        filter, params.document_id, filter->render_process_id(),
        params.render_frame_route_id);
    filter->Send(new ViewMsg_WorkerCreated(route_id));
    return;
  }
  for (WorkerProcessHost::Instances::iterator i = queued_workers_.begin();
       i != queued_workers_.end(); ++i) {
    if (i->Matches(params.url, params.name, partition, resource_context) &&
        params.url != i->url()) {
      *url_mismatch = true;
      return;
    }
  }

  // Generate a unique route id for the browser-worker communication that's
  // unique among all worker processes.  That way when the worker process sends
  // a wrapped IPC message through us, we know which WorkerProcessHost to give
  // it to.
  WorkerProcessHost::WorkerInstance instance(
      params.url,
      params.name,
      params.content_security_policy,
      params.security_policy_type,
      next_worker_route_id(),
      params.render_frame_route_id,
      resource_context,
      partition);
  instance.AddFilter(filter, route_id);
  instance.worker_document_set()->Add(
      filter, params.document_id, filter->render_process_id(),
      params.render_frame_route_id);

  CreateWorkerFromInstance(instance);
}

void WorkerServiceImpl::ForwardToWorker(const IPC::Message& message,
                                        WorkerMessageFilter* filter) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->FilterMessage(message, filter))
      return;
  }

  // TODO(jabdelmalek): tell filter that callee is gone
}

void WorkerServiceImpl::DocumentDetached(unsigned long long document_id,
                                         WorkerMessageFilter* filter) {
  // Any associated shared workers can be shut down.
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter)
    iter->DocumentDetached(filter, document_id);

  // Remove any queued shared workers for this document.
  for (WorkerProcessHost::Instances::iterator iter = queued_workers_.begin();
       iter != queued_workers_.end();) {

    iter->worker_document_set()->Remove(filter, document_id);
    if (iter->worker_document_set()->IsEmpty()) {
      iter = queued_workers_.erase(iter);
      continue;
    }
    ++iter;
  }
}

bool WorkerServiceImpl::CreateWorkerFromInstance(
    WorkerProcessHost::WorkerInstance instance) {
  if (!CanCreateWorkerProcess(instance)) {
    queued_workers_.push_back(instance);
    return true;
  }

  // Remove any queued instances of this worker and copy over the filter to
  // this instance.
  for (WorkerProcessHost::Instances::iterator iter = queued_workers_.begin();
       iter != queued_workers_.end();) {
    if (iter->Matches(instance.url(), instance.name(),
                      instance.partition(), instance.resource_context())) {
      DCHECK(iter->NumFilters() == 1);
      DCHECK_EQ(instance.url(), iter->url());
      WorkerProcessHost::WorkerInstance::FilterInfo filter_info =
          iter->GetFilter();
      instance.AddFilter(filter_info.filter(), filter_info.route_id());
      iter = queued_workers_.erase(iter);
    } else {
      ++iter;
    }
  }

  WorkerMessageFilter* first_filter = instance.filters().begin()->filter();
  WorkerProcessHost* worker = new WorkerProcessHost(
      instance.resource_context(), instance.partition());
  // TODO(atwilson): This won't work if the message is from a worker process.
  // We don't support that yet though (this message is only sent from
  // renderers) but when we do, we'll need to add code to pass in the current
  // worker's document set for nested workers.
  if (!worker->Init(first_filter->render_process_id(),
                    instance.render_frame_id())) {
    delete worker;
    return false;
  }

  worker->CreateWorker(
      instance,
      WorkerDevToolsManager::GetInstance()->WorkerCreated(worker, instance));
  FOR_EACH_OBSERVER(
      WorkerServiceObserver, observers_,
      WorkerCreated(instance.url(), instance.name(), worker->GetData().id,
                    instance.worker_route_id()));
  return true;
}

bool WorkerServiceImpl::CanCreateWorkerProcess(
    const WorkerProcessHost::WorkerInstance& instance) {
  // Worker can be fired off if *any* parent has room.
  const WorkerDocumentSet::DocumentInfoSet& parents =
        instance.worker_document_set()->documents();

  for (WorkerDocumentSet::DocumentInfoSet::const_iterator parent_iter =
           parents.begin();
       parent_iter != parents.end(); ++parent_iter) {
    bool hit_total_worker_limit = false;
    if (FrameCanCreateWorkerProcess(parent_iter->render_process_id(),
                                    parent_iter->render_frame_id(),
                                    &hit_total_worker_limit)) {
      return true;
    }
    // Return false if already at the global worker limit (no need to continue
    // checking parent tabs).
    if (hit_total_worker_limit)
      return false;
  }
  // If we've reached here, none of the parent tabs is allowed to create an
  // instance.
  return false;
}

bool WorkerServiceImpl::FrameCanCreateWorkerProcess(
    int render_process_id,
    int render_frame_id,
    bool* hit_total_worker_limit) {
  int total_workers = 0;
  int workers_per_tab = 0;
  *hit_total_worker_limit = false;
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    for (WorkerProcessHost::Instances::const_iterator cur_instance =
             iter->instances().begin();
         cur_instance != iter->instances().end(); ++cur_instance) {
      total_workers++;
      if (total_workers >= kMaxWorkersWhenSeparate) {
        *hit_total_worker_limit = true;
        return false;
      }
      if (cur_instance->FrameIsParent(render_process_id, render_frame_id)) {
        workers_per_tab++;
        if (workers_per_tab >= kMaxWorkersPerFrameWhenSeparate)
          return false;
      }
    }
  }

  return true;
}

void WorkerServiceImpl::TryStartingQueuedWorker() {
  if (queued_workers_.empty())
    return;

  for (WorkerProcessHost::Instances::iterator i = queued_workers_.begin();
       i != queued_workers_.end();) {
    if (CanCreateWorkerProcess(*i)) {
      WorkerProcessHost::WorkerInstance instance = *i;
      queued_workers_.erase(i);
      CreateWorkerFromInstance(instance);

      // CreateWorkerFromInstance can modify the queued_workers_ list when it
      // coalesces queued instances after starting a shared worker, so we
      // have to rescan the list from the beginning (our iterator is now
      // invalid). This is not a big deal as having any queued workers will be
      // rare in practice so the list will be small.
      i = queued_workers_.begin();
    } else {
      ++i;
    }
  }
}

bool WorkerServiceImpl::GetRendererForWorker(int worker_process_id,
                                             int* render_process_id,
                                             int* render_frame_id) const {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter.GetData().id != worker_process_id)
      continue;

    // This code assumes one worker per process, see function comment in header!
    WorkerProcessHost::Instances::const_iterator first_instance =
        iter->instances().begin();
    if (first_instance == iter->instances().end())
      return false;

    WorkerDocumentSet::DocumentInfoSet::const_iterator info =
        first_instance->worker_document_set()->documents().begin();
    *render_process_id = info->render_process_id();
    *render_frame_id = info->render_frame_id();
    return true;
  }
  return false;
}

const WorkerProcessHost::WorkerInstance* WorkerServiceImpl::FindWorkerInstance(
      int worker_process_id) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter.GetData().id != worker_process_id)
        continue;

    WorkerProcessHost::Instances::const_iterator instance =
        iter->instances().begin();
    return instance == iter->instances().end() ? NULL : &*instance;
  }
  return NULL;
}

bool WorkerServiceImpl::TerminateWorker(int process_id, int route_id) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter.GetData().id == process_id) {
      iter->TerminateWorker(route_id);
      return true;
    }
  }
  return false;
}

std::vector<WorkerService::WorkerInfo> WorkerServiceImpl::GetWorkers() {
  std::vector<WorkerService::WorkerInfo> results;
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    const WorkerProcessHost::Instances& instances = (*iter)->instances();
    for (WorkerProcessHost::Instances::const_iterator i = instances.begin();
         i != instances.end(); ++i) {
      WorkerService::WorkerInfo info;
      info.url = i->url();
      info.name = i->name();
      info.route_id = i->worker_route_id();
      info.process_id = iter.GetData().id;
      info.handle = iter.GetData().handle;
      results.push_back(info);
    }
  }
  return results;
}

void WorkerServiceImpl::AddObserver(WorkerServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.AddObserver(observer);
}

void WorkerServiceImpl::RemoveObserver(WorkerServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.RemoveObserver(observer);
}

void WorkerServiceImpl::NotifyWorkerDestroyed(
    WorkerProcessHost* process,
    int worker_route_id) {
  WorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      process, worker_route_id);
  FOR_EACH_OBSERVER(WorkerServiceObserver, observers_,
                    WorkerDestroyed(process->GetData().id, worker_route_id));
}

void WorkerServiceImpl::NotifyWorkerProcessCreated() {
  priority_setter_->NotifyWorkerProcessCreated();
}

WorkerProcessHost::WorkerInstance* WorkerServiceImpl::FindSharedWorkerInstance(
    const GURL& url,
    const base::string16& name,
    const WorkerStoragePartition& partition,
    ResourceContext* resource_context) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    for (WorkerProcessHost::Instances::iterator instance_iter =
             iter->mutable_instances().begin();
         instance_iter != iter->mutable_instances().end();
         ++instance_iter) {
      if (instance_iter->Matches(url, name, partition, resource_context))
        return &(*instance_iter);
    }
  }
  return NULL;
}

}  // namespace content
