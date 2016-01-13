// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "url/gurl.h"

namespace content {

static bool IncrementWorkerRefCountByPid(int process_id) {
  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (!rph || rph->FastShutdownStarted())
    return false;

  static_cast<RenderProcessHostImpl*>(rph)->IncrementWorkerRefCount();
  return true;
}

ServiceWorkerProcessManager::ProcessInfo::ProcessInfo(
    const scoped_refptr<SiteInstance>& site_instance)
    : site_instance(site_instance),
      process_id(site_instance->GetProcess()->GetID()) {
}

ServiceWorkerProcessManager::ProcessInfo::ProcessInfo(int process_id)
    : process_id(process_id) {
}

ServiceWorkerProcessManager::ProcessInfo::~ProcessInfo() {
}

ServiceWorkerProcessManager::ServiceWorkerProcessManager(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      process_id_for_test_(-1),
      weak_this_factory_(this),
      weak_this_(weak_this_factory_.GetWeakPtr()) {
}

ServiceWorkerProcessManager::~ServiceWorkerProcessManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context_ == NULL)
      << "Call Shutdown() before destroying |this|, so that racing method "
      << "invocations don't use a destroyed BrowserContext.";
}

void ServiceWorkerProcessManager::Shutdown() {
  browser_context_ = NULL;
  for (std::map<int, ProcessInfo>::const_iterator it = instance_info_.begin();
       it != instance_info_.end();
       ++it) {
    RenderProcessHost* rph = RenderProcessHost::FromID(it->second.process_id);
    DCHECK(rph);
    static_cast<RenderProcessHostImpl*>(rph)->DecrementWorkerRefCount();
  }
  instance_info_.clear();
}

void ServiceWorkerProcessManager::AllocateWorkerProcess(
    int embedded_worker_id,
    const std::vector<int>& process_ids,
    const GURL& script_url,
    const base::Callback<void(ServiceWorkerStatusCode, int process_id)>&
        callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ServiceWorkerProcessManager::AllocateWorkerProcess,
                   weak_this_,
                   embedded_worker_id,
                   process_ids,
                   script_url,
                   callback));
    return;
  }

  if (process_id_for_test_ != -1) {
    // Let tests specify the returned process ID. Note: We may need to be able
    // to specify the error code too.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, SERVICE_WORKER_OK, process_id_for_test_));
    return;
  }

  DCHECK(!ContainsKey(instance_info_, embedded_worker_id))
      << embedded_worker_id << " already has a process allocated";

  for (std::vector<int>::const_iterator it = process_ids.begin();
       it != process_ids.end();
       ++it) {
    if (IncrementWorkerRefCountByPid(*it)) {
      instance_info_.insert(
          std::make_pair(embedded_worker_id, ProcessInfo(*it)));
      BrowserThread::PostTask(BrowserThread::IO,
                              FROM_HERE,
                              base::Bind(callback, SERVICE_WORKER_OK, *it));
      return;
    }
  }

  if (!browser_context_) {
    // Shutdown has started.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED, -1));
    return;
  }
  // No existing processes available; start a new one.
  scoped_refptr<SiteInstance> site_instance =
      SiteInstance::CreateForURL(browser_context_, script_url);
  RenderProcessHost* rph = site_instance->GetProcess();
  // This Init() call posts a task to the IO thread that adds the RPH's
  // ServiceWorkerDispatcherHost to the
  // EmbeddedWorkerRegistry::process_sender_map_.
  if (!rph->Init()) {
    LOG(ERROR) << "Couldn't start a new process!";
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED, -1));
    return;
  }

  instance_info_.insert(
      std::make_pair(embedded_worker_id, ProcessInfo(site_instance)));

  static_cast<RenderProcessHostImpl*>(rph)->IncrementWorkerRefCount();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, SERVICE_WORKER_OK, rph->GetID()));
}

void ServiceWorkerProcessManager::ReleaseWorkerProcess(int embedded_worker_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ServiceWorkerProcessManager::ReleaseWorkerProcess,
                   weak_this_,
                   embedded_worker_id));
    return;
  }
  if (process_id_for_test_ != -1) {
    // Unittests don't increment or decrement the worker refcount of a
    // RenderProcessHost.
    return;
  }
  if (browser_context_ == NULL) {
    // Shutdown already released all instances.
    DCHECK(instance_info_.empty());
    return;
  }
  std::map<int, ProcessInfo>::iterator info =
      instance_info_.find(embedded_worker_id);
  DCHECK(info != instance_info_.end());
  RenderProcessHost* rph = NULL;
  if (info->second.site_instance) {
    rph = info->second.site_instance->GetProcess();
    DCHECK_EQ(info->second.process_id, rph->GetID())
        << "A SiteInstance's process shouldn't get destroyed while we're "
           "holding a reference to it. Was the reference actually held?";
  } else {
    rph = RenderProcessHost::FromID(info->second.process_id);
    DCHECK(rph)
        << "Process " << info->second.process_id
        << " was destroyed unexpectedly. Did we actually hold a reference?";
  }
  static_cast<RenderProcessHostImpl*>(rph)->DecrementWorkerRefCount();
  instance_info_.erase(info);
}

}  // namespace content

namespace base {
// Destroying ServiceWorkerProcessManagers only on the UI thread allows the
// member WeakPtr to safely guard the object's lifetime when used on that
// thread.
void DefaultDeleter<content::ServiceWorkerProcessManager>::operator()(
    content::ServiceWorkerProcessManager* ptr) const {
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::UI, FROM_HERE, ptr);
}
}  // namespace base
