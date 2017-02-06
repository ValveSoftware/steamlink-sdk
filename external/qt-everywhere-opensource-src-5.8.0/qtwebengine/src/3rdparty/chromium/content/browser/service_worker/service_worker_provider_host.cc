// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include <utility>

#include "base/guid.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"

namespace content {

namespace {

// PlzNavigate
// Next ServiceWorkerProviderHost ID for navigations, starts at -2 and keeps
// going down.
int g_next_navigation_provider_id = -2;

}  // anonymous namespace

ServiceWorkerProviderHost::OneShotGetReadyCallback::OneShotGetReadyCallback(
    const GetRegistrationForReadyCallback& callback)
    : callback(callback),
      called(false) {
}

ServiceWorkerProviderHost::OneShotGetReadyCallback::~OneShotGetReadyCallback() {
}

// static
std::unique_ptr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::PreCreateNavigationHost(
    base::WeakPtr<ServiceWorkerContextCore> context) {
  CHECK(IsBrowserSideNavigationEnabled());
  // Generate a new browser-assigned id for the host.
  int provider_id = g_next_navigation_provider_id--;
  return std::unique_ptr<ServiceWorkerProviderHost>(
      new ServiceWorkerProviderHost(
          ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE, provider_id,
          SERVICE_WORKER_PROVIDER_FOR_WINDOW, FrameSecurityLevel::UNINITIALIZED,
          context, nullptr));
}

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int render_process_id,
    int route_id,
    int provider_id,
    ServiceWorkerProviderType provider_type,
    FrameSecurityLevel parent_frame_security_level,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host)
    : client_uuid_(base::GenerateGUID()),
      render_process_id_(render_process_id),
      route_id_(route_id),
      render_thread_id_(kDocumentMainThreadId),
      provider_id_(provider_id),
      provider_type_(provider_type),
      parent_frame_security_level_(parent_frame_security_level),
      context_(context),
      dispatcher_host_(dispatcher_host),
      allow_association_(true) {
  DCHECK_NE(SERVICE_WORKER_PROVIDER_UNKNOWN, provider_type_);

  // PlzNavigate
  CHECK(render_process_id != ChildProcessHost::kInvalidUniqueID ||
        IsBrowserSideNavigationEnabled());

  if (provider_type_ == SERVICE_WORKER_PROVIDER_FOR_CONTROLLER) {
    // Actual thread id is set when the service worker context gets started.
    render_thread_id_ = kInvalidEmbeddedWorkerThreadId;
  }
  context_->RegisterProviderHostByClientID(client_uuid_, this);
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  if (context_)
    context_->UnregisterProviderHostByClientID(client_uuid_);

  // Clear docurl so the deferred activation of a waiting worker
  // won't associate the new version with a provider being destroyed.
  document_url_ = GURL();
  if (controlling_version_.get())
    controlling_version_->RemoveControllee(this);

  RemoveAllMatchingRegistrations();

  for (const GURL& pattern : associated_patterns_)
    DecreaseProcessReference(pattern);
}

int ServiceWorkerProviderHost::frame_id() const {
  if (provider_type_ == SERVICE_WORKER_PROVIDER_FOR_WINDOW)
    return route_id_;
  return MSG_ROUTING_NONE;
}

bool ServiceWorkerProviderHost::IsContextSecureForServiceWorker() const {
  // |document_url_| may be empty if loading has not begun, or
  // ServiceWorkerRequestHandler didn't handle the load (because e.g. another
  // handler did first, or the initial request URL was such that
  // OriginCanAccessServiceWorkers returned false).
  if (!document_url_.is_valid())
    return false;
  if (!OriginCanAccessServiceWorkers(document_url_))
    return false;

  if (is_parent_frame_secure())
    return true;

  std::set<std::string> schemes;
  GetContentClient()->browser()->GetSchemesBypassingSecureContextCheckWhitelist(
      &schemes);
  return schemes.find(document_url().scheme()) != schemes.end();
}

void ServiceWorkerProviderHost::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    ChangedVersionAttributesMask changed_mask,
    const ServiceWorkerRegistrationInfo& info) {
  if (!get_ready_callback_ || get_ready_callback_->called)
    return;
  if (changed_mask.active_changed() && registration->active_version()) {
    // Wait until the state change so we don't send the get for ready
    // registration complete message before set version attributes message.
    registration->active_version()->RegisterStatusChangeCallback(base::Bind(
          &ServiceWorkerProviderHost::ReturnRegistrationForReadyIfNeeded,
          AsWeakPtr()));
  }
}

void ServiceWorkerProviderHost::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  if (associated_registration_ == registration)
    DisassociateRegistration();
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerProviderHost::OnRegistrationFinishedUninstalling(
    ServiceWorkerRegistration* registration) {
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerProviderHost::OnSkippedWaiting(
    ServiceWorkerRegistration* registration) {
  if (associated_registration_ != registration)
    return;
  // A client is "using" a registration if it is controlled by the active
  // worker of the registration. skipWaiting doesn't cause a client to start
  // using the registration.
  if (!controlling_version_)
    return;
  ServiceWorkerVersion* active_version = registration->active_version();
  DCHECK_EQ(active_version->status(), ServiceWorkerVersion::ACTIVATING);
  SetControllerVersionAttribute(active_version,
                                true /* notify_controllerchange */);
}

void ServiceWorkerProviderHost::SetDocumentUrl(const GURL& url) {
  DCHECK(!url.has_ref());
  document_url_ = url;
  if (IsProviderForClient())
    SyncMatchingRegistrations();
}

void ServiceWorkerProviderHost::SetTopmostFrameUrl(const GURL& url) {
  topmost_frame_url_ = url;
}

void ServiceWorkerProviderHost::SetControllerVersionAttribute(
    ServiceWorkerVersion* version,
    bool notify_controllerchange) {
  CHECK(!version || IsContextSecureForServiceWorker());
  if (version == controlling_version_.get())
    return;

  scoped_refptr<ServiceWorkerVersion> previous_version = controlling_version_;
  controlling_version_ = version;
  if (version)
    version->AddControllee(this);
  if (previous_version.get())
    previous_version->RemoveControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  // SetController message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  Send(new ServiceWorkerMsg_SetControllerServiceWorker(
      render_thread_id_, provider_id(), GetOrCreateServiceWorkerHandle(version),
      notify_controllerchange));
}

void ServiceWorkerProviderHost::SetHostedVersion(
    ServiceWorkerVersion* version) {
  DCHECK(!IsProviderForClient());
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, version->running_status());
  DCHECK_EQ(render_process_id_, version->embedded_worker()->process_id());
  running_hosted_version_ = version;
}

bool ServiceWorkerProviderHost::IsProviderForClient() const {
  switch (provider_type_) {
    case SERVICE_WORKER_PROVIDER_FOR_WINDOW:
    case SERVICE_WORKER_PROVIDER_FOR_WORKER:
    case SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      return true;
    case SERVICE_WORKER_PROVIDER_FOR_CONTROLLER:
      return false;
    case SERVICE_WORKER_PROVIDER_UNKNOWN:
      NOTREACHED() << provider_type_;
  }
  NOTREACHED() << provider_type_;
  return false;
}

blink::WebServiceWorkerClientType ServiceWorkerProviderHost::client_type()
    const {
  switch (provider_type_) {
    case SERVICE_WORKER_PROVIDER_FOR_WINDOW:
      return blink::WebServiceWorkerClientTypeWindow;
    case SERVICE_WORKER_PROVIDER_FOR_WORKER:
      return blink::WebServiceWorkerClientTypeWorker;
    case SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      return blink::WebServiceWorkerClientTypeSharedWorker;
    case SERVICE_WORKER_PROVIDER_FOR_CONTROLLER:
    case SERVICE_WORKER_PROVIDER_UNKNOWN:
      NOTREACHED() << provider_type_;
  }
  NOTREACHED() << provider_type_;
  return blink::WebServiceWorkerClientTypeWindow;
}

void ServiceWorkerProviderHost::AssociateRegistration(
    ServiceWorkerRegistration* registration,
    bool notify_controllerchange) {
  CHECK(IsContextSecureForServiceWorker());
  DCHECK(CanAssociateRegistration(registration));
  associated_registration_ = registration;
  AddMatchingRegistration(registration);
  SendAssociateRegistrationMessage();
  SetControllerVersionAttribute(registration->active_version(),
                                notify_controllerchange);
}

void ServiceWorkerProviderHost::DisassociateRegistration() {
  queued_events_.clear();
  if (!associated_registration_.get())
    return;
  associated_registration_ = NULL;
  SetControllerVersionAttribute(NULL, false /* notify_controllerchange */);

  if (!dispatcher_host_)
    return;

  // Disassociation message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  Send(new ServiceWorkerMsg_DisassociateRegistration(
      render_thread_id_, provider_id()));
}

void ServiceWorkerProviderHost::AddMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(ServiceWorkerUtils::ScopeMatches(
        registration->pattern(), document_url_));
  if (!IsContextSecureForServiceWorker())
    return;
  size_t key = registration->pattern().spec().size();
  if (ContainsKey(matching_registrations_, key))
    return;
  IncreaseProcessReference(registration->pattern());
  registration->AddListener(this);
  matching_registrations_[key] = registration;
  ReturnRegistrationForReadyIfNeeded();
}

void ServiceWorkerProviderHost::RemoveMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  size_t key = registration->pattern().spec().size();
  DCHECK(ContainsKey(matching_registrations_, key));
  DecreaseProcessReference(registration->pattern());
  registration->RemoveListener(this);
  matching_registrations_.erase(key);
}

ServiceWorkerRegistration*
ServiceWorkerProviderHost::MatchRegistration() const {
  ServiceWorkerRegistrationMap::const_reverse_iterator it =
      matching_registrations_.rbegin();
  for (; it != matching_registrations_.rend(); ++it) {
    if (it->second->is_uninstalled())
      continue;
    if (it->second->is_uninstalling())
      return nullptr;
    return it->second.get();
  }
  return nullptr;
}

void ServiceWorkerProviderHost::NotifyControllerLost() {
  SetControllerVersionAttribute(nullptr, true /* notify_controllerchange */);
}

std::unique_ptr<ServiceWorkerRequestHandler>
ServiceWorkerProviderHost::CreateRequestHandler(
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    FetchRedirectMode redirect_mode,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    scoped_refptr<ResourceRequestBodyImpl> body) {
  if (IsHostToRunningServiceWorker()) {
    return std::unique_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerContextRequestHandler(
            context_, AsWeakPtr(), blob_storage_context, resource_type));
  }
  if (ServiceWorkerUtils::IsMainResourceType(resource_type) ||
      controlling_version()) {
    return std::unique_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerControlleeRequestHandler(
            context_, AsWeakPtr(), blob_storage_context, request_mode,
            credentials_mode, redirect_mode, resource_type,
            request_context_type, frame_type, body));
  }
  return std::unique_ptr<ServiceWorkerRequestHandler>();
}

ServiceWorkerObjectInfo
ServiceWorkerProviderHost::GetOrCreateServiceWorkerHandle(
    ServiceWorkerVersion* version) {
  DCHECK(dispatcher_host_);
  if (!context_ || !version)
    return ServiceWorkerObjectInfo();
  ServiceWorkerHandle* handle = dispatcher_host_->FindServiceWorkerHandle(
      provider_id(), version->version_id());
  if (handle) {
    handle->IncrementRefCount();
    return handle->GetObjectInfo();
  }

  std::unique_ptr<ServiceWorkerHandle> new_handle(
      ServiceWorkerHandle::Create(context_, AsWeakPtr(), version));
  handle = new_handle.get();
  dispatcher_host_->RegisterServiceWorkerHandle(std::move(new_handle));
  return handle->GetObjectInfo();
}

bool ServiceWorkerProviderHost::CanAssociateRegistration(
    ServiceWorkerRegistration* registration) {
  if (!context_)
    return false;
  if (running_hosted_version_.get())
    return false;
  if (!registration || associated_registration_.get() || !allow_association_)
    return false;
  return true;
}

void ServiceWorkerProviderHost::PostMessageToClient(
    ServiceWorkerVersion* version,
    const base::string16& message,
    const std::vector<int>& sent_message_ports) {
  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  std::vector<int> new_routing_ids;
  dispatcher_host_->message_port_message_filter()->
      UpdateMessagePortsWithNewRoutes(sent_message_ports,
                                      &new_routing_ids);

  ServiceWorkerMsg_MessageToDocument_Params params;
  params.thread_id = kDocumentMainThreadId;
  params.provider_id = provider_id();
  params.service_worker_info = GetOrCreateServiceWorkerHandle(version);
  params.message = message;
  params.message_ports = sent_message_ports;
  params.new_routing_ids = new_routing_ids;
  Send(new ServiceWorkerMsg_MessageToDocument(params));
}

void ServiceWorkerProviderHost::AddScopedProcessReferenceToPattern(
    const GURL& pattern) {
  associated_patterns_.push_back(pattern);
  IncreaseProcessReference(pattern);
}

void ServiceWorkerProviderHost::ClaimedByRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(registration->active_version());
  if (registration == associated_registration_) {
    SetControllerVersionAttribute(registration->active_version(),
                                  true /* notify_controllerchange */);
  } else if (allow_association_) {
    DisassociateRegistration();
    AssociateRegistration(registration, true /* notify_controllerchange */);
  }
}

bool ServiceWorkerProviderHost::GetRegistrationForReady(
    const GetRegistrationForReadyCallback& callback) {
  if (get_ready_callback_)
    return false;
  get_ready_callback_.reset(new OneShotGetReadyCallback(callback));
  ReturnRegistrationForReadyIfNeeded();
  return true;
}

void ServiceWorkerProviderHost::PrepareForCrossSiteTransfer() {
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_NE(MSG_ROUTING_NONE, route_id_);
  DCHECK_EQ(kDocumentMainThreadId, render_thread_id_);
  DCHECK_NE(SERVICE_WORKER_PROVIDER_UNKNOWN, provider_type_);

  for (const GURL& pattern : associated_patterns_)
    DecreaseProcessReference(pattern);

  for (auto& key_registration : matching_registrations_)
    DecreaseProcessReference(key_registration.second->pattern());

  if (associated_registration_.get()) {
    if (dispatcher_host_) {
      Send(new ServiceWorkerMsg_DisassociateRegistration(
          render_thread_id_, provider_id()));
    }
  }

  render_process_id_ = ChildProcessHost::kInvalidUniqueID;
  route_id_ = MSG_ROUTING_NONE;
  render_thread_id_ = kInvalidEmbeddedWorkerThreadId;
  provider_id_ = kInvalidServiceWorkerProviderId;
  provider_type_ = SERVICE_WORKER_PROVIDER_UNKNOWN;
  dispatcher_host_ = nullptr;
}

void ServiceWorkerProviderHost::CompleteCrossSiteTransfer(
    int new_process_id,
    int new_frame_id,
    int new_provider_id,
    ServiceWorkerProviderType new_provider_type,
    ServiceWorkerDispatcherHost* new_dispatcher_host) {
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, new_process_id);
  DCHECK_NE(MSG_ROUTING_NONE, new_frame_id);

  render_thread_id_ = kDocumentMainThreadId;
  provider_id_ = new_provider_id;
  provider_type_ = new_provider_type;

  FinalizeInitialization(new_process_id, new_frame_id, new_dispatcher_host);
}

// PlzNavigate
void ServiceWorkerProviderHost::CompleteNavigationInitialized(
    int process_id,
    int frame_routing_id,
    ServiceWorkerDispatcherHost* dispatcher_host) {
  CHECK(IsBrowserSideNavigationEnabled());
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, render_process_id_);
  DCHECK_EQ(SERVICE_WORKER_PROVIDER_FOR_WINDOW, provider_type_);
  DCHECK_EQ(kDocumentMainThreadId, render_thread_id_);

  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);
  DCHECK_NE(MSG_ROUTING_NONE, frame_routing_id);

  FinalizeInitialization(process_id, frame_routing_id, dispatcher_host);
}

void ServiceWorkerProviderHost::SendUpdateFoundMessage(
    int registration_handle_id) {
  if (!dispatcher_host_)
    return;  // Could be nullptr in some tests.

  if (!IsReadyToSendMessages()) {
    queued_events_.push_back(
        base::Bind(&ServiceWorkerProviderHost::SendUpdateFoundMessage,
                   AsWeakPtr(), registration_handle_id));
    return;
  }

  Send(new ServiceWorkerMsg_UpdateFound(
      render_thread_id_, registration_handle_id));
}

void ServiceWorkerProviderHost::SendSetVersionAttributesMessage(
    int registration_handle_id,
    ChangedVersionAttributesMask changed_mask,
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
  if (!dispatcher_host_)
    return;  // Could be nullptr in some tests.
  if (!changed_mask.changed())
    return;

  if (!IsReadyToSendMessages()) {
    queued_events_.push_back(base::Bind(
        &ServiceWorkerProviderHost::SendSetVersionAttributesMessage,
        AsWeakPtr(), registration_handle_id, changed_mask,
        base::RetainedRef(installing_version),
        base::RetainedRef(waiting_version), base::RetainedRef(active_version)));
    return;
  }

  ServiceWorkerVersionAttributes attrs;
  if (changed_mask.installing_changed())
    attrs.installing = GetOrCreateServiceWorkerHandle(installing_version);
  if (changed_mask.waiting_changed())
    attrs.waiting = GetOrCreateServiceWorkerHandle(waiting_version);
  if (changed_mask.active_changed())
    attrs.active = GetOrCreateServiceWorkerHandle(active_version);

  Send(new ServiceWorkerMsg_SetVersionAttributes(
      render_thread_id_, registration_handle_id, changed_mask.changed(),
      attrs));
}

void ServiceWorkerProviderHost::SendServiceWorkerStateChangedMessage(
    int worker_handle_id,
    blink::WebServiceWorkerState state) {
  if (!dispatcher_host_)
    return;

  if (!IsReadyToSendMessages()) {
    queued_events_.push_back(base::Bind(
        &ServiceWorkerProviderHost::SendServiceWorkerStateChangedMessage,
        AsWeakPtr(), worker_handle_id, state));
    return;
  }

  Send(new ServiceWorkerMsg_ServiceWorkerStateChanged(
      render_thread_id_, worker_handle_id, state));
}

void ServiceWorkerProviderHost::SetReadyToSendMessagesToWorker(
    int render_thread_id) {
  DCHECK(!IsReadyToSendMessages());
  render_thread_id_ = render_thread_id;

  for (const auto& event : queued_events_)
    event.Run();
  queued_events_.clear();
}

void ServiceWorkerProviderHost::SendAssociateRegistrationMessage() {
  if (!dispatcher_host_)
    return;

  ServiceWorkerRegistrationHandle* handle =
      dispatcher_host_->GetOrCreateRegistrationHandle(
          AsWeakPtr(), associated_registration_.get());

  ServiceWorkerVersionAttributes attrs;
  attrs.installing = GetOrCreateServiceWorkerHandle(
      associated_registration_->installing_version());
  attrs.waiting = GetOrCreateServiceWorkerHandle(
      associated_registration_->waiting_version());
  attrs.active = GetOrCreateServiceWorkerHandle(
      associated_registration_->active_version());

  // Association message should be sent only for controllees.
  DCHECK(IsProviderForClient());
  dispatcher_host_->Send(new ServiceWorkerMsg_AssociateRegistration(
      render_thread_id_, provider_id(), handle->GetObjectInfo(), attrs));
}

void ServiceWorkerProviderHost::SyncMatchingRegistrations() {
  DCHECK(context_);
  RemoveAllMatchingRegistrations();
  const auto& registrations = context_->GetLiveRegistrations();
  for (const auto& key_registration : registrations) {
    ServiceWorkerRegistration* registration = key_registration.second;
    if (!registration->is_uninstalled() &&
        ServiceWorkerUtils::ScopeMatches(registration->pattern(),
                                         document_url_))
      AddMatchingRegistration(registration);
  }
}

void ServiceWorkerProviderHost::RemoveAllMatchingRegistrations() {
  for (const auto& it : matching_registrations_) {
    ServiceWorkerRegistration* registration = it.second.get();
    DecreaseProcessReference(registration->pattern());
    registration->RemoveListener(this);
  }
  matching_registrations_.clear();
}

void ServiceWorkerProviderHost::IncreaseProcessReference(
    const GURL& pattern) {
  if (context_ && context_->process_manager()) {
    context_->process_manager()->AddProcessReferenceToPattern(
        pattern, render_process_id_);
  }
}

void ServiceWorkerProviderHost::DecreaseProcessReference(
    const GURL& pattern) {
  if (context_ && context_->process_manager()) {
    context_->process_manager()->RemoveProcessReferenceFromPattern(
        pattern, render_process_id_);
  }
}

void ServiceWorkerProviderHost::ReturnRegistrationForReadyIfNeeded() {
  if (!get_ready_callback_ || get_ready_callback_->called)
    return;
  ServiceWorkerRegistration* registration = MatchRegistration();
  if (!registration)
    return;
  if (registration->active_version()) {
    get_ready_callback_->callback.Run(registration);
    get_ready_callback_->callback.Reset();
    get_ready_callback_->called = true;
    return;
  }
}

bool ServiceWorkerProviderHost::IsReadyToSendMessages() const {
  return render_thread_id_ != kInvalidEmbeddedWorkerThreadId;
}

bool ServiceWorkerProviderHost::IsContextAlive() {
  return context_ != nullptr;
}

void ServiceWorkerProviderHost::Send(IPC::Message* message) const {
  DCHECK(dispatcher_host_);
  DCHECK(IsReadyToSendMessages());
  dispatcher_host_->Send(message);
}

void ServiceWorkerProviderHost::FinalizeInitialization(
    int process_id,
    int frame_routing_id,
    ServiceWorkerDispatcherHost* dispatcher_host) {
  render_process_id_ = process_id;
  route_id_ = frame_routing_id;
  dispatcher_host_ = dispatcher_host;

  for (const GURL& pattern : associated_patterns_)
    IncreaseProcessReference(pattern);

  for (auto& key_registration : matching_registrations_)
    IncreaseProcessReference(key_registration.second->pattern());

  if (associated_registration_.get()) {
    SendAssociateRegistrationMessage();
    if (dispatcher_host_ && associated_registration_->active_version()) {
      Send(new ServiceWorkerMsg_SetControllerServiceWorker(
          render_thread_id_, provider_id(),
          GetOrCreateServiceWorkerHandle(
              associated_registration_->active_version()),
          false /* shouldNotifyControllerChange */));
    }
  }
}

}  // namespace content
