// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_agent_host.h"

#include "base/strings/stringprintf.h"
#include "content/browser/devtools/devtools_protocol_handler.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

void StatusNoOp(ServiceWorkerStatusCode status) {}

void TerminateServiceWorkerOnIO(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id))
      version->StopWorker(base::Bind(&StatusNoOp));
  }
}

void UnregisterServiceWorkerOnIO(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id)) {
        version->StopWorker(base::Bind(&StatusNoOp));
        context->UnregisterServiceWorker(
            version->scope(), base::Bind(&StatusNoOp));
    }
  }
}

void SetDevToolsAttachedOnIO(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id,
    bool attached) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id))
      version->SetDevToolsAttached(attached);
  }
}

}  // namespace

ServiceWorkerDevToolsAgentHost::ServiceWorkerDevToolsAgentHost(
    WorkerId worker_id,
    const ServiceWorkerIdentifier& service_worker,
    bool is_installed_version)
    : WorkerDevToolsAgentHost(worker_id),
      service_worker_(new ServiceWorkerIdentifier(service_worker)),
      network_handler_(new devtools::network::NetworkHandler()),
      version_installed_time_(is_installed_version ? base::Time::Now()
                                                   : base::Time()) {
  DevToolsProtocolDispatcher* dispatcher = protocol_handler()->dispatcher();
  dispatcher->SetNetworkHandler(network_handler_.get());
}

DevToolsAgentHost::Type ServiceWorkerDevToolsAgentHost::GetType() {
  return TYPE_SERVICE_WORKER;
}

std::string ServiceWorkerDevToolsAgentHost::GetTitle() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id().first)) {
    return base::StringPrintf("Worker pid:%d",
                              base::GetProcId(host->GetHandle()));
  }
  return "";
}

GURL ServiceWorkerDevToolsAgentHost::GetURL() {
  return service_worker_->url();
}

bool ServiceWorkerDevToolsAgentHost::Activate() {
  return false;
}

bool ServiceWorkerDevToolsAgentHost::Close() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&TerminateServiceWorkerOnIO,
                  service_worker_->context_weak(),
                  service_worker_->version_id()));
  return true;
}

void ServiceWorkerDevToolsAgentHost::UnregisterWorker() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&UnregisterServiceWorkerOnIO,
                  service_worker_->context_weak(),
                  service_worker_->version_id()));
}

void ServiceWorkerDevToolsAgentHost::OnAttachedStateChanged(bool attached) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SetDevToolsAttachedOnIO,
                  service_worker_->context_weak(),
                  service_worker_->version_id(),
                  attached));
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionInstalled() {
  version_installed_time_ = base::Time::Now();
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionDoomed() {
  version_doomed_time_ = base::Time::Now();
}

int64_t ServiceWorkerDevToolsAgentHost::service_worker_version_id() const {
  return service_worker_->version_id();
}

GURL ServiceWorkerDevToolsAgentHost::scope() const {
  return service_worker_->scope();
}

bool ServiceWorkerDevToolsAgentHost::Matches(
    const ServiceWorkerIdentifier& other) {
  return service_worker_->Matches(other);
}

ServiceWorkerDevToolsAgentHost::~ServiceWorkerDevToolsAgentHost() {
  ServiceWorkerDevToolsManager::GetInstance()->RemoveInspectedWorkerData(
      worker_id());
}

}  // namespace content
