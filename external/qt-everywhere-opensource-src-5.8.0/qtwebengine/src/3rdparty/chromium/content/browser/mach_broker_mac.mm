// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mach_broker_mac.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {
const char kBootstrapName[] = "rohitfork";
}

// static
bool MachBroker::ChildSendTaskPortToParent() {
  return base::MachPortBroker::ChildSendTaskPortToParent(kBootstrapName);
}

MachBroker* MachBroker::GetInstance() {
  return base::Singleton<MachBroker,
                         base::LeakySingletonTraits<MachBroker>>::get();
}

base::Lock& MachBroker::GetLock() {
  return broker_.GetLock();
}

void MachBroker::EnsureRunning() {
  GetLock().AssertAcquired();

  if (initialized_)
    return;

  // Do not attempt to reinitialize in the event of failure.
  initialized_ = true;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MachBroker::RegisterNotifications, base::Unretained(this)));

  if (!broker_.Init()) {
    LOG(ERROR) << "Failed to initialize the MachListenerThreadDelegate";
  }
}

void MachBroker::AddPlaceholderForPid(base::ProcessHandle pid,
                                      int child_process_id) {
  GetLock().AssertAcquired();

  broker_.AddPlaceholderForPid(pid);
  child_process_id_map_[child_process_id] = pid;
}

mach_port_t MachBroker::TaskForPid(base::ProcessHandle pid) const {
  return broker_.TaskForPid(pid);
}

void MachBroker::BrowserChildProcessHostDisconnected(
    const ChildProcessData& data) {
  InvalidateChildProcessId(data.id);
}

void MachBroker::BrowserChildProcessCrashed(const ChildProcessData& data,
    int exit_code) {
  InvalidateChildProcessId(data.id);
}

void MachBroker::Observe(int type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDERER_PROCESS_TERMINATED:
    case NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* host = Source<RenderProcessHost>(source).ptr();
      InvalidateChildProcessId(host->GetID());
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}

// static
std::string MachBroker::GetMachPortName() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const bool is_child = command_line->HasSwitch(switches::kProcessType);
  return base::MachPortBroker::GetMachPortName(kBootstrapName, is_child);
}

MachBroker::MachBroker() : initialized_(false), broker_(kBootstrapName) {
  broker_.AddObserver(this);
}

MachBroker::~MachBroker() {
  broker_.RemoveObserver(this);
}

void MachBroker::OnReceivedTaskPort(base::ProcessHandle process) {
  NotifyObservers(process);
}

void MachBroker::InvalidateChildProcessId(int child_process_id) {
  base::AutoLock lock(GetLock());
  MachBroker::ChildProcessIdMap::iterator it =
      child_process_id_map_.find(child_process_id);
  if (it == child_process_id_map_.end())
    return;

  broker_.InvalidatePid(it->second);
  child_process_id_map_.erase(it);
}

void MachBroker::RegisterNotifications() {
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());

  // No corresponding StopObservingBrowserChildProcesses,
  // we leak this singleton.
  BrowserChildProcessObserver::Add(this);
}

}  // namespace content
