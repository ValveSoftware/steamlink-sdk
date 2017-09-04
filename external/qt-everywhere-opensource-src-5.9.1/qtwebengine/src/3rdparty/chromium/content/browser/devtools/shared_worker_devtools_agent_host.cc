// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_agent_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

void TerminateSharedWorkerOnIO(
    WorkerDevToolsAgentHost::WorkerId worker_id) {
  SharedWorkerServiceImpl::GetInstance()->TerminateWorker(
      worker_id.first, worker_id.second);
}

}  // namespace

SharedWorkerDevToolsAgentHost::SharedWorkerDevToolsAgentHost(
    WorkerId worker_id,
    const SharedWorkerInstance& shared_worker)
    : WorkerDevToolsAgentHost(worker_id),
      shared_worker_(new SharedWorkerInstance(shared_worker)) {
  NotifyCreated();
}

std::string SharedWorkerDevToolsAgentHost::GetType() {
  return kTypeSharedWorker;
}

std::string SharedWorkerDevToolsAgentHost::GetTitle() {
  return base::UTF16ToUTF8(shared_worker_->name());
}

GURL SharedWorkerDevToolsAgentHost::GetURL() {
  return shared_worker_->url();
}

bool SharedWorkerDevToolsAgentHost::Activate() {
  return false;
}

void SharedWorkerDevToolsAgentHost::Reload() {
}

bool SharedWorkerDevToolsAgentHost::Close() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&TerminateSharedWorkerOnIO, worker_id()));
  return true;
}

bool SharedWorkerDevToolsAgentHost::Matches(
    const SharedWorkerInstance& other) {
  return shared_worker_->Matches(other);
}

SharedWorkerDevToolsAgentHost::~SharedWorkerDevToolsAgentHost() {
  SharedWorkerDevToolsManager::GetInstance()->RemoveInspectedWorkerData(
      worker_id());
}

}  // namespace content
