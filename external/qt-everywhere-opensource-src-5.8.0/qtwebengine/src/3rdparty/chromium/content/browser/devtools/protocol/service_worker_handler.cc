// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/service_worker_handler.h"

#include "base/bind.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/background_sync/background_sync_context.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_watcher.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/common/push_messaging_status.h"
#include "url/gurl.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace content {
namespace devtools {
namespace service_worker {

using Response = DevToolsProtocolClient::Response;

namespace {

using ScopeAgentsMap =
    std::map<GURL, std::unique_ptr<ServiceWorkerDevToolsAgentHost::List>>;

void ResultNoOp(bool success) {
}

void StatusNoOp(ServiceWorkerStatusCode status) {
}

void StatusNoOpKeepingRegistration(
    scoped_refptr<content::ServiceWorkerRegistration> protect,
    ServiceWorkerStatusCode status) {
}

void PushDeliveryNoOp(PushDeliveryStatus status) {
}

const std::string GetVersionRunningStatusString(
    EmbeddedWorkerStatus running_status) {
  switch (running_status) {
    case EmbeddedWorkerStatus::STOPPED:
      return kServiceWorkerVersionRunningStatusStopped;
    case EmbeddedWorkerStatus::STARTING:
      return kServiceWorkerVersionRunningStatusStarting;
    case EmbeddedWorkerStatus::RUNNING:
      return kServiceWorkerVersionRunningStatusRunning;
    case EmbeddedWorkerStatus::STOPPING:
      return kServiceWorkerVersionRunningStatusStopping;
  }
  return std::string();
}

const std::string GetVersionStatusString(
    content::ServiceWorkerVersion::Status status) {
  switch (status) {
    case content::ServiceWorkerVersion::NEW:
      return kServiceWorkerVersionStatusNew;
    case content::ServiceWorkerVersion::INSTALLING:
      return kServiceWorkerVersionStatusInstalling;
    case content::ServiceWorkerVersion::INSTALLED:
      return kServiceWorkerVersionStatusInstalled;
    case content::ServiceWorkerVersion::ACTIVATING:
      return kServiceWorkerVersionStatusActivating;
    case content::ServiceWorkerVersion::ACTIVATED:
      return kServiceWorkerVersionStatusActivated;
    case content::ServiceWorkerVersion::REDUNDANT:
      return kServiceWorkerVersionStatusRedundant;
  }
  return std::string();
}

scoped_refptr<ServiceWorkerVersion> CreateVersionDictionaryValue(
    const ServiceWorkerVersionInfo& version_info) {
  std::vector<std::string> clients;
  for (const auto& client : version_info.clients) {
    if (client.second.type == SERVICE_WORKER_PROVIDER_FOR_WINDOW) {
      RenderFrameHostImpl* render_frame_host = RenderFrameHostImpl::FromID(
          client.second.process_id, client.second.route_id);
      WebContents* web_contents =
          WebContents::FromRenderFrameHost(render_frame_host);
      // There is a possibility that the frame is already deleted because of the
      // thread hopping.
      if (!web_contents)
        continue;
      scoped_refptr<DevToolsAgentHost> agent_host(
          DevToolsAgentHost::GetOrCreateFor(web_contents));
      if (agent_host)
        clients.push_back(agent_host->GetId());
    } else if (client.second.type ==
               SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER) {
      scoped_refptr<DevToolsAgentHost> agent_host(
          DevToolsAgentHost::GetForWorker(client.second.process_id,
                                          client.second.route_id));
      if (agent_host)
        clients.push_back(agent_host->GetId());
    }
  }
  scoped_refptr<ServiceWorkerVersion> version(
      ServiceWorkerVersion::Create()
          ->set_version_id(base::Int64ToString(version_info.version_id))
          ->set_registration_id(
                base::Int64ToString(version_info.registration_id))
          ->set_script_url(version_info.script_url.spec())
          ->set_running_status(
                GetVersionRunningStatusString(version_info.running_status))
          ->set_status(GetVersionStatusString(version_info.status))
          ->set_script_last_modified(
                version_info.script_last_modified.ToDoubleT())
          ->set_script_response_time(
                version_info.script_response_time.ToDoubleT())
          ->set_controlled_clients(clients));
  return version;
}

scoped_refptr<ServiceWorkerRegistration> CreateRegistrationDictionaryValue(
    const ServiceWorkerRegistrationInfo& registration_info) {
  scoped_refptr<ServiceWorkerRegistration> registration(
      ServiceWorkerRegistration::Create()
          ->set_registration_id(
              base::Int64ToString(registration_info.registration_id))
          ->set_scope_url(registration_info.pattern.spec())
          ->set_is_deleted(registration_info.delete_flag ==
                           ServiceWorkerRegistrationInfo::IS_DELETED));
  return registration;
}

void GetMatchingHostsByScopeMap(
    const ServiceWorkerDevToolsAgentHost::List& agent_hosts,
    const std::set<GURL>& urls,
    ScopeAgentsMap* scope_agents_map) {
  std::set<base::StringPiece> host_name_set;
  for (const GURL& url : urls)
    host_name_set.insert(url.host_piece());
  for (const auto& host : agent_hosts) {
    if (host_name_set.find(host->scope().host_piece()) == host_name_set.end())
      continue;
    const auto& it = scope_agents_map->find(host->scope());
    if (it == scope_agents_map->end()) {
      std::unique_ptr<ServiceWorkerDevToolsAgentHost::List> new_list(
          new ServiceWorkerDevToolsAgentHost::List());
      new_list->push_back(host);
      (*scope_agents_map)[host->scope()] = std::move(new_list);
    } else {
      it->second->push_back(host);
    }
  }
}

void AddEligibleHosts(const ServiceWorkerDevToolsAgentHost::List& list,
                      ServiceWorkerDevToolsAgentHost::Map* result) {
  base::Time last_installed_time;
  base::Time last_doomed_time;
  for (const auto& host : list) {
    if (host->version_installed_time() > last_installed_time)
      last_installed_time = host->version_installed_time();
    if (host->version_doomed_time() > last_doomed_time)
      last_doomed_time = host->version_doomed_time();
  }
  for (const auto& host : list) {
    // We don't attech old redundant Service Workers when there is newer
    // installed Service Worker.
    if (host->version_doomed_time().is_null() ||
        (last_installed_time < last_doomed_time &&
         last_doomed_time == host->version_doomed_time())) {
      (*result)[host->GetId()] = host;
    }
  }
}

ServiceWorkerDevToolsAgentHost::Map GetMatchingServiceWorkers(
    BrowserContext* browser_context,
    const std::set<GURL>& urls) {
  ServiceWorkerDevToolsAgentHost::Map result;
  if (!browser_context)
    return result;

  ServiceWorkerDevToolsAgentHost::List agent_hosts;
  ServiceWorkerDevToolsManager::GetInstance()
      ->AddAllAgentHostsForBrowserContext(browser_context, &agent_hosts);

  ScopeAgentsMap scope_agents_map;
  GetMatchingHostsByScopeMap(agent_hosts, urls, &scope_agents_map);

  for (const auto& it : scope_agents_map)
    AddEligibleHosts(*it.second.get(), &result);

  return result;
}

void StopServiceWorkerOnIO(scoped_refptr<ServiceWorkerContextWrapper> context,
                           int64_t version_id) {
  if (content::ServiceWorkerVersion* version =
          context->GetLiveVersion(version_id)) {
    version->StopWorker(base::Bind(&StatusNoOp));
  }
}

void GetDevToolsRouteInfoOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    int64_t version_id,
    const base::Callback<void(int, int)>& callback) {
  if (content::ServiceWorkerVersion* version =
          context->GetLiveVersion(version_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            callback, version->embedded_worker()->process_id(),
            version->embedded_worker()->worker_devtools_agent_route_id()));
  }
}

Response CreateContextErrorResponse() {
  return Response::InternalError("Could not connect to the context");
}

Response CreateInvalidVersionIdErrorResponse() {
  return Response::InternalError("Invalid version ID");
}

const std::string GetDevToolsAgentHostTypeString(
    content::DevToolsAgentHost::Type type) {
  switch (type) {
    case DevToolsAgentHost::TYPE_WEB_CONTENTS:
      return "web_contents";
    case DevToolsAgentHost::TYPE_FRAME:
      return "frame";
    case DevToolsAgentHost::TYPE_SHARED_WORKER:
      return "shared_worker";
    case DevToolsAgentHost::TYPE_SERVICE_WORKER:
      return "service_worker";
    case DevToolsAgentHost::TYPE_EXTERNAL:
      return "external";
    case DevToolsAgentHost::TYPE_BROWSER:
      return "browser";
  }
  NOTREACHED() << type;
  return std::string();
}

void DidFindRegistrationForDispatchSyncEventOnIO(
    scoped_refptr<BackgroundSyncContext> sync_context,
    const std::string& tag,
    bool last_chance,
    ServiceWorkerStatusCode status,
    const scoped_refptr<content::ServiceWorkerRegistration>& registration) {
  if (status != SERVICE_WORKER_OK || !registration->active_version())
    return;
  BackgroundSyncManager* background_sync_manager =
      sync_context->background_sync_manager();
  scoped_refptr<content::ServiceWorkerVersion> version(
      registration->active_version());
  // Keep the registration while dispatching the sync event.
  background_sync_manager->EmulateDispatchSyncEvent(
      tag, std::move(version), last_chance,
      base::Bind(&StatusNoOpKeepingRegistration, registration));
}

void DispatchSyncEventOnIO(scoped_refptr<ServiceWorkerContextWrapper> context,
                           scoped_refptr<BackgroundSyncContext> sync_context,
                           const GURL& origin,
                           int64_t registration_id,
                           const std::string& tag,
                           bool last_chance) {
  context->FindReadyRegistrationForId(
      registration_id, origin,
      base::Bind(&DidFindRegistrationForDispatchSyncEventOnIO, sync_context,
                 tag, last_chance));
}

}  // namespace

ServiceWorkerHandler::ServiceWorkerHandler()
    : enabled_(false), render_frame_host_(nullptr), weak_factory_(this) {
}

ServiceWorkerHandler::~ServiceWorkerHandler() {
  Disable();
}

void ServiceWorkerHandler::SetRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  render_frame_host_ = render_frame_host;
  // Do not call UpdateHosts yet, wait for load to commit.
  if (!render_frame_host) {
    ClearForceUpdate();
    context_ = nullptr;
    return;
  }
  StoragePartition* partition = BrowserContext::GetStoragePartition(
      render_frame_host->GetProcess()->GetBrowserContext(),
      render_frame_host->GetSiteInstance());
  DCHECK(partition);
  context_ = static_cast<ServiceWorkerContextWrapper*>(
      partition->GetServiceWorkerContext());
}

void ServiceWorkerHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

void ServiceWorkerHandler::UpdateHosts() {
  if (!enabled_)
    return;

  urls_.clear();
  BrowserContext* browser_context = nullptr;
  if (render_frame_host_) {
    for (FrameTreeNode* node :
         render_frame_host_->frame_tree_node()->frame_tree()->Nodes())
      urls_.insert(node->current_url());

    browser_context = render_frame_host_->GetProcess()->GetBrowserContext();
  }

  ServiceWorkerDevToolsAgentHost::Map old_hosts = attached_hosts_;
  ServiceWorkerDevToolsAgentHost::Map new_hosts =
      GetMatchingServiceWorkers(browser_context, urls_);

  for (const auto& pair : old_hosts) {
    if (new_hosts.find(pair.first) == new_hosts.end())
      ReportWorkerTerminated(pair.second.get());
  }

  for (const auto& pair : new_hosts) {
    if (old_hosts.find(pair.first) == old_hosts.end())
      ReportWorkerCreated(pair.second.get());
  }
}

void ServiceWorkerHandler::Detached() {
  Disable();
}

Response ServiceWorkerHandler::Enable() {
  if (enabled_)
    return Response::OK();
  if (!context_)
    return Response::InternalError("Could not connect to the context");
  enabled_ = true;

  ServiceWorkerDevToolsManager::GetInstance()->AddObserver(this);

  context_watcher_ = new ServiceWorkerContextWatcher(
      context_, base::Bind(&ServiceWorkerHandler::OnWorkerRegistrationUpdated,
                           weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerHandler::OnWorkerVersionUpdated,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ServiceWorkerHandler::OnErrorReported,
                 weak_factory_.GetWeakPtr()));
  context_watcher_->Start();

  UpdateHosts();
  return Response::OK();
}

Response ServiceWorkerHandler::Disable() {
  if (!enabled_)
    return Response::OK();
  enabled_ = false;

  ServiceWorkerDevToolsManager::GetInstance()->RemoveObserver(this);
  ClearForceUpdate();
  for (const auto& pair : attached_hosts_)
    pair.second->DetachClient(this);
  attached_hosts_.clear();
  DCHECK(context_watcher_);
  context_watcher_->Stop();
  context_watcher_ = nullptr;
  return Response::OK();
}

Response ServiceWorkerHandler::SendMessage(
    const std::string& worker_id,
    const std::string& message) {
  auto it = attached_hosts_.find(worker_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not connected to the worker");
  it->second->DispatchProtocolMessage(message);
  return Response::OK();
}

Response ServiceWorkerHandler::Stop(
    const std::string& worker_id) {
  auto it = attached_hosts_.find(worker_id);
  if (it == attached_hosts_.end())
    return Response::InternalError("Not connected to the worker");
  it->second->UnregisterWorker();
  return Response::OK();
}

Response ServiceWorkerHandler::Unregister(const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  context_->UnregisterServiceWorker(GURL(scope_url), base::Bind(&ResultNoOp));
  return Response::OK();
}

Response ServiceWorkerHandler::StartWorker(const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  context_->StartServiceWorker(GURL(scope_url), base::Bind(&StatusNoOp));
  return Response::OK();
}

Response ServiceWorkerHandler::SkipWaiting(const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  context_->SkipWaitingWorker(GURL(scope_url));
  return Response::OK();
}

Response ServiceWorkerHandler::StopWorker(const std::string& version_id) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(version_id, &id))
    return CreateInvalidVersionIdErrorResponse();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&StopServiceWorkerOnIO, context_, id));
  return Response::OK();
}

Response ServiceWorkerHandler::UpdateRegistration(
    const std::string& scope_url) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();
  context_->UpdateRegistration(GURL(scope_url));
  return Response::OK();
}

Response ServiceWorkerHandler::InspectWorker(const std::string& version_id) {
  if (!enabled_)
    return Response::OK();
  if (!context_)
    return CreateContextErrorResponse();

  int64_t id = kInvalidServiceWorkerVersionId;
  if (!base::StringToInt64(version_id, &id))
    return CreateInvalidVersionIdErrorResponse();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetDevToolsRouteInfoOnIO, context_, id,
                 base::Bind(&ServiceWorkerHandler::OpenNewDevToolsWindow,
                            weak_factory_.GetWeakPtr())));
  return Response::OK();
}

Response ServiceWorkerHandler::SetForceUpdateOnPageLoad(
    bool force_update_on_page_load) {
  if (!context_)
    return CreateContextErrorResponse();
  context_->SetForceUpdateOnPageLoad(force_update_on_page_load);
  return Response::OK();
}

Response ServiceWorkerHandler::DeliverPushMessage(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& data) {
  if (!enabled_)
    return Response::OK();
  if (!render_frame_host_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();
  PushEventPayload payload;
  if (data.size() > 0)
    payload.setData(data);
  BrowserContext::DeliverPushMessage(
      render_frame_host_->GetProcess()->GetBrowserContext(), GURL(origin), id,
      payload, base::Bind(&PushDeliveryNoOp));
  return Response::OK();
}

Response ServiceWorkerHandler::DispatchSyncEvent(
    const std::string& origin,
    const std::string& registration_id,
    const std::string& tag,
    bool last_chance) {
  if (!enabled_)
    return Response::OK();
  if (!render_frame_host_)
    return CreateContextErrorResponse();
  int64_t id = 0;
  if (!base::StringToInt64(registration_id, &id))
    return CreateInvalidVersionIdErrorResponse();

  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(BrowserContext::GetStoragePartition(
          render_frame_host_->GetProcess()->GetBrowserContext(),
          render_frame_host_->GetSiteInstance()));
  BackgroundSyncContext* sync_context = partition->GetBackgroundSyncContext();

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&DispatchSyncEventOnIO, context_,
                                     make_scoped_refptr(sync_context),
                                     GURL(origin), id, tag, last_chance));
  return Response::OK();
}

Response ServiceWorkerHandler::GetTargetInfo(
    const std::string& target_id,
    scoped_refptr<TargetInfo>* target_info) {
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("targetId");
  *target_info =
      TargetInfo::Create()
          ->set_id(agent_host->GetId())
          ->set_type(GetDevToolsAgentHostTypeString(agent_host->GetType()))
          ->set_title(agent_host->GetTitle())
          ->set_url(agent_host->GetURL().spec());
  return Response::OK();
}

Response ServiceWorkerHandler::ActivateTarget(const std::string& target_id) {
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("targetId");
  agent_host->Activate();
  return Response::OK();
}

void ServiceWorkerHandler::OpenNewDevToolsWindow(int process_id,
                                                 int devtools_agent_route_id) {
  scoped_refptr<DevToolsAgentHostImpl> agent_host(
      ServiceWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(process_id, devtools_agent_route_id));
  if (!agent_host.get())
    return;
  agent_host->Inspect(render_frame_host_->GetProcess()->GetBrowserContext());
}

void ServiceWorkerHandler::OnWorkerRegistrationUpdated(
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registration_values;
  for (const auto& registration : registrations) {
    registration_values.push_back(
        CreateRegistrationDictionaryValue(registration));
  }
  client_->WorkerRegistrationUpdated(
      WorkerRegistrationUpdatedParams::Create()->set_registrations(
          registration_values));
}

void ServiceWorkerHandler::OnWorkerVersionUpdated(
    const std::vector<ServiceWorkerVersionInfo>& versions) {
  std::vector<scoped_refptr<ServiceWorkerVersion>> version_values;
  for (const auto& version : versions) {
    version_values.push_back(CreateVersionDictionaryValue(version));
  }
  client_->WorkerVersionUpdated(
      WorkerVersionUpdatedParams::Create()->set_versions(version_values));
}

void ServiceWorkerHandler::OnErrorReported(
    int64_t registration_id,
    int64_t version_id,
    const ServiceWorkerContextObserver::ErrorInfo& info) {
  client_->WorkerErrorReported(
      WorkerErrorReportedParams::Create()->set_error_message(
          ServiceWorkerErrorMessage::Create()
              ->set_error_message(base::UTF16ToUTF8(info.error_message))
              ->set_registration_id(base::Int64ToString(registration_id))
              ->set_version_id(base::Int64ToString(version_id))
              ->set_source_url(info.source_url.spec())
              ->set_line_number(info.line_number)
              ->set_column_number(info.column_number)));
}

void ServiceWorkerHandler::DispatchProtocolMessage(
    DevToolsAgentHost* host,
    const std::string& message) {

  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;  // Already disconnected.

  client_->DispatchMessage(
      DispatchMessageParams::Create()->
          set_worker_id(host->GetId())->
          set_message(message));
}

void ServiceWorkerHandler::AgentHostClosed(
    DevToolsAgentHost* host,
    bool replaced_with_another_client) {
  client_->WorkerTerminated(WorkerTerminatedParams::Create()->
      set_worker_id(host->GetId()));
  attached_hosts_.erase(host->GetId());
}

void ServiceWorkerHandler::WorkerCreated(
    ServiceWorkerDevToolsAgentHost* host) {
  BrowserContext* browser_context = nullptr;
  if (render_frame_host_)
    browser_context = render_frame_host_->GetProcess()->GetBrowserContext();

  auto hosts = GetMatchingServiceWorkers(browser_context, urls_);
  if (hosts.find(host->GetId()) != hosts.end() && !host->IsAttached() &&
      !host->IsPausedForDebugOnStart())
    host->PauseForDebugOnStart();
}

void ServiceWorkerHandler::WorkerReadyForInspection(
    ServiceWorkerDevToolsAgentHost* host) {
  if (ServiceWorkerDevToolsManager::GetInstance()
          ->debug_service_worker_on_start()) {
    // When debug_service_worker_on_start is true, a new DevTools window will
    // be opend in ServiceWorkerDevToolsManager::WorkerReadyForInspection.
    return;
  }
  UpdateHosts();
}

void ServiceWorkerHandler::WorkerVersionInstalled(
    ServiceWorkerDevToolsAgentHost* host) {
  UpdateHosts();
}
void ServiceWorkerHandler::WorkerVersionDoomed(
    ServiceWorkerDevToolsAgentHost* host) {
  UpdateHosts();
}

void ServiceWorkerHandler::WorkerDestroyed(
    ServiceWorkerDevToolsAgentHost* host) {
  UpdateHosts();
}

void ServiceWorkerHandler::ReportWorkerCreated(
    ServiceWorkerDevToolsAgentHost* host) {
  if (host->IsAttached())
    return;
  attached_hosts_[host->GetId()] = host;
  host->AttachClient(this);
  client_->WorkerCreated(WorkerCreatedParams::Create()
                             ->set_worker_id(host->GetId())
                             ->set_url(host->GetURL().spec())
                             ->set_version_id(base::Int64ToString(
                                 host->service_worker_version_id())));
}

void ServiceWorkerHandler::ReportWorkerTerminated(
    ServiceWorkerDevToolsAgentHost* host) {
  auto it = attached_hosts_.find(host->GetId());
  if (it == attached_hosts_.end())
    return;
  host->DetachClient(this);
  client_->WorkerTerminated(WorkerTerminatedParams::Create()->
      set_worker_id(host->GetId()));
  attached_hosts_.erase(it);
}

void ServiceWorkerHandler::ClearForceUpdate() {
  if (context_)
    context_->SetForceUpdateOnPageLoad(false);
}

}  // namespace service_worker
}  // namespace devtools
}  // namespace content
