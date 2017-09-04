// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"

namespace content {

namespace {

// Functor to sort by the .second element of a struct.
struct SecondGreater {
  template <typename Value>
  bool operator()(const Value& lhs, const Value& rhs) {
    return lhs.second > rhs.second;
  }
};

}  // namespace

ServiceWorkerProcessManager::ProcessInfo::ProcessInfo(
    const scoped_refptr<SiteInstance>& site_instance)
    : site_instance(site_instance),
      process_id(site_instance->GetProcess()->GetID()) {
}

ServiceWorkerProcessManager::ProcessInfo::ProcessInfo(int process_id)
    : process_id(process_id) {
}

ServiceWorkerProcessManager::ProcessInfo::ProcessInfo(
    const ProcessInfo& other) = default;

ServiceWorkerProcessManager::ProcessInfo::~ProcessInfo() {
}

ServiceWorkerProcessManager::ServiceWorkerProcessManager(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      process_id_for_test_(ChildProcessHost::kInvalidUniqueID),
      new_process_id_for_test_(ChildProcessHost::kInvalidUniqueID),
      weak_this_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

ServiceWorkerProcessManager::~ServiceWorkerProcessManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsShutdown())
      << "Call Shutdown() before destroying |this|, so that racing method "
      << "invocations don't use a destroyed BrowserContext.";
  // TODO(horo): Remove after collecting crash data.
  // Temporary checks to verify that ServiceWorkerProcessManager doesn't prevent
  // render process hosts from shutting down: crbug.com/639193
  CHECK(instance_info_.empty());
}

void ServiceWorkerProcessManager::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  {
    base::AutoLock lock(browser_context_lock_);
    browser_context_ = nullptr;
  }

  for (std::map<int, ProcessInfo>::const_iterator it = instance_info_.begin();
       it != instance_info_.end();
       ++it) {
    RenderProcessHost::FromID(it->second.process_id)
        ->DecrementServiceWorkerRefCount();
  }
  instance_info_.clear();
}

bool ServiceWorkerProcessManager::IsShutdown() {
  base::AutoLock lock(browser_context_lock_);
  return !browser_context_;
}

void ServiceWorkerProcessManager::AddProcessReferenceToPattern(
    const GURL& pattern, int process_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ServiceWorkerProcessManager::AddProcessReferenceToPattern,
                   weak_this_,
                   pattern,
                   process_id));
    return;
  }

  ProcessRefMap& process_refs = pattern_processes_[pattern];
  ++process_refs[process_id];
}

void ServiceWorkerProcessManager::RemoveProcessReferenceFromPattern(
    const GURL& pattern, int process_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(
          &ServiceWorkerProcessManager::RemoveProcessReferenceFromPattern,
          weak_this_,
          pattern,
          process_id));
    return;
  }

  PatternProcessRefMap::iterator it = pattern_processes_.find(pattern);
  if (it == pattern_processes_.end()) {
    NOTREACHED() << "process references not found for pattern: " << pattern;
    return;
  }
  ProcessRefMap& process_refs = it->second;
  ProcessRefMap::iterator found = process_refs.find(process_id);
  if (found == process_refs.end()) {
    NOTREACHED() << "Releasing unknown process ref " << process_id;
    return;
  }
  if (--found->second == 0) {
    process_refs.erase(found);
    if (process_refs.empty())
      pattern_processes_.erase(it);
  }
}

bool ServiceWorkerProcessManager::PatternHasProcessToRun(
    const GURL& pattern) const {
  PatternProcessRefMap::const_iterator it = pattern_processes_.find(pattern);
  if (it == pattern_processes_.end())
    return false;
  return !it->second.empty();
}

void ServiceWorkerProcessManager::AllocateWorkerProcess(
    int embedded_worker_id,
    const GURL& pattern,
    const GURL& script_url,
    bool can_use_existing_process,
    const base::Callback<void(ServiceWorkerStatusCode,
                              int process_id,
                              bool is_new_process,
                              const EmbeddedWorkerSettings&)>& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ServiceWorkerProcessManager::AllocateWorkerProcess,
                   weak_this_, embedded_worker_id, pattern, script_url,
                   can_use_existing_process, callback));
    return;
  }

  // This |EmbeddedWorkerSettings| only populates |data_saver_enabled|,
  // but in general, this function will populate settings from prefs, while
  // the caller will be responsible for populating settings from other sources,
  // such as command line switches.
  EmbeddedWorkerSettings settings;
  settings.data_saver_enabled =
      GetContentClient()->browser()->IsDataSaverEnabled(browser_context_);

  if (process_id_for_test_ != ChildProcessHost::kInvalidUniqueID) {
    // Let tests specify the returned process ID. Note: We may need to be able
    // to specify the error code too.
    int result = can_use_existing_process ? process_id_for_test_
                                          : new_process_id_for_test_;
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback, SERVICE_WORKER_OK, result,
                                       false /* is_new_process */, settings));
    return;
  }

  if (IsShutdown()) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback, SERVICE_WORKER_ERROR_ABORT,
                                       ChildProcessHost::kInvalidUniqueID,
                                       false /* is_new_process */, settings));
    return;
  }

  DCHECK(!base::ContainsKey(instance_info_, embedded_worker_id))
      << embedded_worker_id << " already has a process allocated";

  if (can_use_existing_process) {
    int process_id = FindAvailableProcess(pattern);
    if (process_id != ChildProcessHost::kInvalidUniqueID) {
      RenderProcessHost::FromID(process_id)->IncrementServiceWorkerRefCount();
      instance_info_.insert(
          std::make_pair(embedded_worker_id, ProcessInfo(process_id)));
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(callback, SERVICE_WORKER_OK, process_id,
                     false /* is_new_process */, settings));
      return;
    }
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
        BrowserThread::IO, FROM_HERE,
        base::Bind(callback, SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND,
                   ChildProcessHost::kInvalidUniqueID,
                   false /* is_new_process */, settings));
    return;
  }

  instance_info_.insert(
      std::make_pair(embedded_worker_id, ProcessInfo(site_instance)));

  rph->IncrementServiceWorkerRefCount();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, SERVICE_WORKER_OK, rph->GetID(),
                                     true /* is_new_process */, settings));
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

  if (process_id_for_test_ != ChildProcessHost::kInvalidUniqueID) {
    // Unittests don't increment or decrement the worker refcount of a
    // RenderProcessHost.
    return;
  }

  if (IsShutdown()) {
    // Shutdown already released all instances.
    DCHECK(instance_info_.empty());
    return;
  }

  std::map<int, ProcessInfo>::iterator info =
      instance_info_.find(embedded_worker_id);
  // ReleaseWorkerProcess could be called for a nonexistent worker id, for
  // example, when request to start a worker is aborted on the IO thread during
  // process allocation that is failed on the UI thread.
  if (info == instance_info_.end())
    return;

  RenderProcessHost* rph = NULL;
  if (info->second.site_instance.get()) {
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
  rph->DecrementServiceWorkerRefCount();
  instance_info_.erase(info);
}

std::vector<int> ServiceWorkerProcessManager::SortProcessesForPattern(
    const GURL& pattern) const {
  PatternProcessRefMap::const_iterator it = pattern_processes_.find(pattern);
  if (it == pattern_processes_.end())
    return std::vector<int>();

  // Prioritize higher refcount processes to choose the process which has more
  // tabs and is less likely to be backgrounded by user action like tab close.
  std::vector<std::pair<int, int> > counted(
      it->second.begin(), it->second.end());
  std::sort(counted.begin(), counted.end(), SecondGreater());

  std::vector<int> result(counted.size());
  for (size_t i = 0; i < counted.size(); ++i)
    result[i] = counted[i].first;
  return result;
}

int ServiceWorkerProcessManager::FindAvailableProcess(const GURL& pattern) {
  RenderProcessHost* backgrounded_candidate = nullptr;

  // Try to find an available foreground process.
  std::vector<int> sorted_candidates = SortProcessesForPattern(pattern);
  for (int process_id : sorted_candidates) {
    RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
    if (!rph || rph->FastShutdownStarted())
      continue;

    // Keep a backgrounded process for a suboptimal choice.
    if (rph->IsProcessBackgrounded()) {
      if (!backgrounded_candidate)
        backgrounded_candidate = rph;
      continue;
    }

    return process_id;
  }

  // No foreground processes available; choose a backgrounded one.
  if (backgrounded_candidate)
    return backgrounded_candidate->GetID();

  return ChildProcessHost::kInvalidUniqueID;
}

}  // namespace content

namespace std {
// Destroying ServiceWorkerProcessManagers only on the UI thread allows the
// member WeakPtr to safely guard the object's lifetime when used on that
// thread.
void default_delete<content::ServiceWorkerProcessManager>::operator()(
    content::ServiceWorkerProcessManager* ptr) const {
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::UI, FROM_HERE, ptr);
}
}  // namespace std
