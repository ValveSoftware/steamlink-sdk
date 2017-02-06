// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SERVICE_WORKER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SERVICE_WORKER_HANDLER_H_

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace content {

class RenderFrameHostImpl;
class ServiceWorkerContextWatcher;
class ServiceWorkerContextWrapper;

namespace devtools {
namespace service_worker {

class ServiceWorkerHandler : public DevToolsAgentHostClient,
                             public ServiceWorkerDevToolsManager::Observer {
 public:
  typedef DevToolsProtocolClient::Response Response;

  ServiceWorkerHandler();
  ~ServiceWorkerHandler() override;

  void SetRenderFrameHost(RenderFrameHostImpl* render_frame_host);
  void SetClient(std::unique_ptr<Client> client);
  void UpdateHosts();
  void Detached();

  // Protocol 'service worker' domain implementation.
  Response Enable();
  Response Disable();
  Response SendMessage(const std::string& worker_id,
                       const std::string& message);
  Response Stop(const std::string& worker_id);
  Response Unregister(const std::string& scope_url);
  Response StartWorker(const std::string& scope_url);
  Response SkipWaiting(const std::string& scope_url);
  Response StopWorker(const std::string& version_id);
  Response UpdateRegistration(const std::string& scope_url);
  Response InspectWorker(const std::string& version_id);
  Response SetForceUpdateOnPageLoad(bool force_update_on_page_load);
  Response DeliverPushMessage(const std::string& origin,
                              const std::string& registration_id,
                              const std::string& data);
  Response DispatchSyncEvent(const std::string& origin,
                             const std::string& registration_id,
                             const std::string& tag,
                             bool last_chance);
  Response GetTargetInfo(const std::string& target_id,
                         scoped_refptr<TargetInfo>* target_info);
  Response ActivateTarget(const std::string& target_id);

  // WorkerDevToolsManager::Observer implementation.
  void WorkerCreated(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerReadyForInspection(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerVersionInstalled(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerVersionDoomed(ServiceWorkerDevToolsAgentHost* host) override;
  void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) override;

 private:
  // DevToolsAgentHostClient overrides.
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;
  void AgentHostClosed(DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override;

  void ReportWorkerCreated(ServiceWorkerDevToolsAgentHost* host);
  void ReportWorkerTerminated(ServiceWorkerDevToolsAgentHost* host);

  void OnWorkerRegistrationUpdated(
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);
  void OnWorkerVersionUpdated(
      const std::vector<ServiceWorkerVersionInfo>& registrations);
  void OnErrorReported(int64_t registration_id,
                       int64_t version_id,
                       const ServiceWorkerContextObserver::ErrorInfo& info);

  void OpenNewDevToolsWindow(int process_id, int devtools_agent_route_id);
  void ClearForceUpdate();

  scoped_refptr<ServiceWorkerContextWrapper> context_;
  std::unique_ptr<Client> client_;
  ServiceWorkerDevToolsAgentHost::Map attached_hosts_;
  bool enabled_;
  std::set<GURL> urls_;
  scoped_refptr<ServiceWorkerContextWatcher> context_watcher_;
  RenderFrameHostImpl* render_frame_host_;

  base::WeakPtrFactory<ServiceWorkerHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandler);
};

}  // namespace service_worker
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SERVICE_WORKER_HANDLER_H_
