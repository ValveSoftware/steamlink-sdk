// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_dispatcher.h"

#include <stddef.h>
#include <utility>

#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/content_constants.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "url/url_constants.h"

using blink::WebServiceWorkerError;
using blink::WebServiceWorkerProvider;
using base::ThreadLocalPointer;

namespace content {

namespace {

base::LazyInstance<ThreadLocalPointer<void>>::Leaky g_dispatcher_tls =
    LAZY_INSTANCE_INITIALIZER;

void* const kHasBeenDeleted = reinterpret_cast<void*>(0x1);

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
}

}  // namespace

ServiceWorkerDispatcher::ServiceWorkerDispatcher(
    ThreadSafeSender* thread_safe_sender,
    base::SingleThreadTaskRunner* main_thread_task_runner)
    : thread_safe_sender_(thread_safe_sender),
      main_thread_task_runner_(main_thread_task_runner) {
  g_dispatcher_tls.Pointer()->Set(static_cast<void*>(this));
}

ServiceWorkerDispatcher::~ServiceWorkerDispatcher() {
  g_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

void ServiceWorkerDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;

  // When you add a new message handler, you should consider adding a similar
  // handler in ServiceWorkerMessageFilter to release references passed from
  // the browser process in case we fail to post task to the thread.
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcher, msg)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_AssociateRegistration,
                        OnAssociateRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DisassociateRegistration,
                        OnDisassociateRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistered, OnRegistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUpdated, OnUpdated)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistered,
                        OnUnregistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetRegistration,
                        OnDidGetRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetRegistrations,
                        OnDidGetRegistrations)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetRegistrationForReady,
                        OnDidGetRegistrationForReady)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistrationError,
                        OnRegistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUpdateError,
                        OnUpdateError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistrationError,
                        OnUnregistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerGetRegistrationError,
                        OnGetRegistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerGetRegistrationsError,
                        OnGetRegistrationsError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerStateChanged,
                        OnServiceWorkerStateChanged)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SetVersionAttributes,
                        OnSetVersionAttributes)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_UpdateFound,
                        OnUpdateFound)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SetControllerServiceWorker,
                        OnSetControllerServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_MessageToDocument,
                        OnPostMessage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

void ServiceWorkerDispatcher::RegisterServiceWorker(
    int provider_id,
    const GURL& pattern,
    const GURL& script_url,
    WebServiceWorkerRegistrationCallbacks* callbacks) {
  DCHECK(callbacks);

  if (pattern.possibly_invalid_spec().size() > url::kMaxURLChars ||
      script_url.possibly_invalid_spec().size() > url::kMaxURLChars) {
    std::unique_ptr<WebServiceWorkerRegistrationCallbacks> owned_callbacks(
        callbacks);
    std::string error_message(kServiceWorkerRegisterErrorPrefix);
    error_message += "The provided scriptURL or scope is too long.";
    callbacks->onError(
        WebServiceWorkerError(WebServiceWorkerError::ErrorTypeSecurity,
                              blink::WebString::fromUTF8(error_message)));
    return;
  }

  int request_id = pending_registration_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN2("ServiceWorker",
                           "ServiceWorkerDispatcher::RegisterServiceWorker",
                           request_id,
                           "Scope", pattern.spec(),
                           "Script URL", script_url.spec());
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_RegisterServiceWorker(
      CurrentWorkerId(), request_id, provider_id, pattern, script_url));
}

void ServiceWorkerDispatcher::UpdateServiceWorker(
    int provider_id,
    int64_t registration_id,
    WebServiceWorkerUpdateCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_update_callbacks_.Add(callbacks);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_UpdateServiceWorker(
      CurrentWorkerId(), request_id, provider_id, registration_id));
}

void ServiceWorkerDispatcher::UnregisterServiceWorker(
    int provider_id,
    int64_t registration_id,
    WebServiceWorkerUnregistrationCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_unregistration_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcher::UnregisterServiceWorker",
                           request_id, "Registration ID", registration_id);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_UnregisterServiceWorker(
      CurrentWorkerId(), request_id, provider_id, registration_id));
}

void ServiceWorkerDispatcher::GetRegistration(
    int provider_id,
    const GURL& document_url,
    WebServiceWorkerGetRegistrationCallbacks* callbacks) {
  DCHECK(callbacks);

  if (document_url.possibly_invalid_spec().size() > url::kMaxURLChars) {
    std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> owned_callbacks(
        callbacks);
    std::string error_message(kServiceWorkerGetRegistrationErrorPrefix);
    error_message += "The provided documentURL is too long.";
    callbacks->onError(
        WebServiceWorkerError(WebServiceWorkerError::ErrorTypeSecurity,
                              blink::WebString::fromUTF8(error_message)));
    return;
  }

  int request_id = pending_get_registration_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcher::GetRegistration",
                           request_id,
                           "Document URL", document_url.spec());
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_GetRegistration(
      CurrentWorkerId(), request_id, provider_id, document_url));
}

void ServiceWorkerDispatcher::GetRegistrations(
    int provider_id,
    WebServiceWorkerGetRegistrationsCallbacks* callbacks) {
  DCHECK(callbacks);

  int request_id = pending_get_registrations_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerDispatcher::GetRegistrations",
                           request_id);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_GetRegistrations(
      CurrentWorkerId(), request_id, provider_id));
}

void ServiceWorkerDispatcher::GetRegistrationForReady(
    int provider_id,
    WebServiceWorkerGetRegistrationForReadyCallbacks* callbacks) {
  int request_id = get_for_ready_callbacks_.Add(callbacks);
  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerDispatcher::GetRegistrationForReady",
                           request_id);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_GetRegistrationForReady(
      CurrentWorkerId(), request_id, provider_id));
}

void ServiceWorkerDispatcher::AddProviderContext(
    ServiceWorkerProviderContext* provider_context) {
  DCHECK(provider_context);
  int provider_id = provider_context->provider_id();
  DCHECK(!ContainsKey(provider_contexts_, provider_id));
  provider_contexts_[provider_id] = provider_context;
}

void ServiceWorkerDispatcher::RemoveProviderContext(
    ServiceWorkerProviderContext* provider_context) {
  DCHECK(provider_context);
  DCHECK(ContainsKey(provider_contexts_, provider_context->provider_id()));
  provider_contexts_.erase(provider_context->provider_id());
}

void ServiceWorkerDispatcher::AddProviderClient(
    int provider_id,
    blink::WebServiceWorkerProviderClient* client) {
  DCHECK(client);
  DCHECK(!ContainsKey(provider_clients_, provider_id));
  provider_clients_[provider_id] = client;
}

void ServiceWorkerDispatcher::RemoveProviderClient(int provider_id) {
  // This could be possibly called multiple times to ensure termination.
  if (ContainsKey(provider_clients_, provider_id))
    provider_clients_.erase(provider_id);
}

ServiceWorkerDispatcher*
ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    base::SingleThreadTaskRunner* main_thread_task_runner) {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS ServiceWorkerDispatcher.";
    g_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_dispatcher_tls.Pointer()->Get())
    return static_cast<ServiceWorkerDispatcher*>(
        g_dispatcher_tls.Pointer()->Get());

  ServiceWorkerDispatcher* dispatcher =
      new ServiceWorkerDispatcher(thread_safe_sender, main_thread_task_runner);
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(dispatcher);
  return dispatcher;
}

ServiceWorkerDispatcher* ServiceWorkerDispatcher::GetThreadSpecificInstance() {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted)
    return NULL;
  return static_cast<ServiceWorkerDispatcher*>(
      g_dispatcher_tls.Pointer()->Get());
}

void ServiceWorkerDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

scoped_refptr<WebServiceWorkerImpl>
ServiceWorkerDispatcher::GetOrCreateServiceWorker(
    std::unique_ptr<ServiceWorkerHandleReference> handle_ref) {
  if (!handle_ref)
    return nullptr;

  WorkerObjectMap::iterator found =
      service_workers_.find(handle_ref->handle_id());
  if (found != service_workers_.end())
    return found->second;

  // WebServiceWorkerImpl constructor calls AddServiceWorker.
  return new WebServiceWorkerImpl(std::move(handle_ref),
                                  thread_safe_sender_.get());
}

scoped_refptr<WebServiceWorkerRegistrationImpl>
ServiceWorkerDispatcher::GetOrCreateRegistration(
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  RegistrationObjectMap::iterator found = registrations_.find(info.handle_id);
  if (found != registrations_.end())
    return found->second;

  // WebServiceWorkerRegistrationImpl constructor calls
  // AddServiceWorkerRegistration.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration(
      new WebServiceWorkerRegistrationImpl(
          ServiceWorkerRegistrationHandleReference::Create(
              info, thread_safe_sender_.get())));

  registration->SetInstalling(
      GetOrCreateServiceWorker(ServiceWorkerHandleReference::Create(
          attrs.installing, thread_safe_sender_.get())));
  registration->SetWaiting(
      GetOrCreateServiceWorker(ServiceWorkerHandleReference::Create(
          attrs.waiting, thread_safe_sender_.get())));
  registration->SetActive(
      GetOrCreateServiceWorker(ServiceWorkerHandleReference::Create(
          attrs.active, thread_safe_sender_.get())));
  return registration;
}

scoped_refptr<WebServiceWorkerRegistrationImpl>
ServiceWorkerDispatcher::GetOrAdoptRegistration(
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration_ref =
      Adopt(info);
  std::unique_ptr<ServiceWorkerHandleReference> installing_ref =
      Adopt(attrs.installing);
  std::unique_ptr<ServiceWorkerHandleReference> waiting_ref =
      Adopt(attrs.waiting);
  std::unique_ptr<ServiceWorkerHandleReference> active_ref =
      Adopt(attrs.active);

  RegistrationObjectMap::iterator found = registrations_.find(info.handle_id);
  if (found != registrations_.end())
    return found->second;

  // WebServiceWorkerRegistrationImpl constructor calls
  // AddServiceWorkerRegistration.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration(
      new WebServiceWorkerRegistrationImpl(std::move(registration_ref)));
  registration->SetInstalling(
      GetOrCreateServiceWorker(std::move(installing_ref)));
  registration->SetWaiting(GetOrCreateServiceWorker(std::move(waiting_ref)));
  registration->SetActive(GetOrCreateServiceWorker(std::move(active_ref)));
  return registration;
}

void ServiceWorkerDispatcher::OnAssociateRegistration(
    int thread_id,
    int provider_id,
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  // Adopt the references sent from the browser process and pass them to the
  // provider context if it exists.
  std::unique_ptr<ServiceWorkerRegistrationHandleReference> registration =
      Adopt(info);
  std::unique_ptr<ServiceWorkerHandleReference> installing =
      Adopt(attrs.installing);
  std::unique_ptr<ServiceWorkerHandleReference> waiting = Adopt(attrs.waiting);
  std::unique_ptr<ServiceWorkerHandleReference> active = Adopt(attrs.active);
  ProviderContextMap::iterator context = provider_contexts_.find(provider_id);
  if (context != provider_contexts_.end()) {
    context->second->OnAssociateRegistration(
        std::move(registration), std::move(installing), std::move(waiting),
        std::move(active));
  }
}

void ServiceWorkerDispatcher::OnDisassociateRegistration(
    int thread_id,
    int provider_id) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider == provider_contexts_.end())
    return;
  provider->second->OnDisassociateRegistration();
}

void ServiceWorkerDispatcher::OnRegistered(
    int thread_id,
    int request_id,
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerDispatcher::RegisterServiceWorker",
                               request_id,
                               "OnRegistered");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::RegisterServiceWorker",
                         request_id);
  WebServiceWorkerRegistrationCallbacks* callbacks =
      pending_registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onSuccess(WebServiceWorkerRegistrationImpl::CreateHandle(
      GetOrAdoptRegistration(info, attrs)));
  pending_registration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUpdated(int thread_id, int request_id) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerDispatcher::UpdateServiceWorker",
                               request_id, "OnUpdated");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::UpdateServiceWorker",
                         request_id);
  WebServiceWorkerUpdateCallbacks* callbacks =
      pending_update_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onSuccess();
  pending_update_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUnregistered(int thread_id,
                                             int request_id,
                                             bool is_success) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::UnregisterServiceWorker",
      request_id,
      "OnUnregistered");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::UnregisterServiceWorker",
                         request_id);
  WebServiceWorkerUnregistrationCallbacks* callbacks =
      pending_unregistration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;
  callbacks->onSuccess(is_success);
  pending_unregistration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnDidGetRegistration(
    int thread_id,
    int request_id,
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistration",
      request_id,
      "OnDidGetRegistration");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistration",
                         request_id);
  WebServiceWorkerGetRegistrationCallbacks* callbacks =
      pending_get_registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  scoped_refptr<WebServiceWorkerRegistrationImpl> registration;
  if (info.handle_id != kInvalidServiceWorkerRegistrationHandleId)
    registration = GetOrAdoptRegistration(info, attrs);

  callbacks->onSuccess(
      WebServiceWorkerRegistrationImpl::CreateHandle(registration));
  pending_get_registration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnDidGetRegistrations(
    int thread_id,
    int request_id,
    const std::vector<ServiceWorkerRegistrationObjectInfo>& infos,
    const std::vector<ServiceWorkerVersionAttributes>& attrs) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistrations",
      request_id,
      "OnDidGetRegistrations");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistrations",
                         request_id);

  WebServiceWorkerGetRegistrationsCallbacks* callbacks =
      pending_get_registrations_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  typedef blink::WebVector<blink::WebServiceWorkerRegistration::Handle*>
      WebServiceWorkerRegistrationArray;
  std::unique_ptr<WebServiceWorkerRegistrationArray> registrations(
      new WebServiceWorkerRegistrationArray(infos.size()));
  for (size_t i = 0; i < infos.size(); ++i) {
    if (infos[i].handle_id != kInvalidServiceWorkerHandleId) {
      ServiceWorkerRegistrationObjectInfo info(infos[i]);
      ServiceWorkerVersionAttributes attr(attrs[i]);

      // WebServiceWorkerGetRegistrationsCallbacks cannot receive an array of
      // std::unique_ptr<WebServiceWorkerRegistration::Handle>, so create leaky
      // handles instead.
      (*registrations)[i] = WebServiceWorkerRegistrationImpl::CreateLeakyHandle(
          GetOrAdoptRegistration(info, attr));
    }
  }

  callbacks->onSuccess(std::move(registrations));
  pending_get_registrations_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnDidGetRegistrationForReady(
    int thread_id,
    int request_id,
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistrationForReady",
      request_id,
      "OnDidGetRegistrationForReady");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistrationForReady",
                         request_id);
  WebServiceWorkerGetRegistrationForReadyCallbacks* callbacks =
      get_for_ready_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onSuccess(WebServiceWorkerRegistrationImpl::CreateHandle(
      GetOrAdoptRegistration(info, attrs)));
  get_for_ready_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnRegistrationError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerDispatcher::RegisterServiceWorker",
                               request_id,
                               "OnRegistrationError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::RegisterServiceWorker",
                         request_id);
  WebServiceWorkerRegistrationCallbacks* callbacks =
      pending_registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_registration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUpdateError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerDispatcher::UpdateServiceWorker",
                               request_id, "OnUpdateError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::UpdateServiceWorker",
                         request_id);
  WebServiceWorkerUpdateCallbacks* callbacks =
      pending_update_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_update_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUnregistrationError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::UnregisterServiceWorker",
      request_id,
      "OnUnregistrationError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::UnregisterServiceWorker",
                         request_id);
  WebServiceWorkerUnregistrationCallbacks* callbacks =
      pending_unregistration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_unregistration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnGetRegistrationError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistration",
      request_id,
      "OnGetRegistrationError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistration",
                         request_id);
  WebServiceWorkerGetRegistrationCallbacks* callbacks =
      pending_get_registration_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_get_registration_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnGetRegistrationsError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "ServiceWorker",
      "ServiceWorkerDispatcher::GetRegistrations",
      request_id,
      "OnGetRegistrationsError");
  TRACE_EVENT_ASYNC_END0("ServiceWorker",
                         "ServiceWorkerDispatcher::GetRegistrations",
                         request_id);
  WebServiceWorkerGetRegistrationsCallbacks* callbacks =
      pending_get_registrations_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onError(WebServiceWorkerError(error_type, message));
  pending_get_registrations_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnServiceWorkerStateChanged(
    int thread_id,
    int handle_id,
    blink::WebServiceWorkerState state) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerDispatcher::OnServiceWorkerStateChanged",
               "Thread ID", thread_id,
               "State", state);
  WorkerObjectMap::iterator worker = service_workers_.find(handle_id);
  if (worker != service_workers_.end())
    worker->second->OnStateChanged(state);
}

void ServiceWorkerDispatcher::OnSetVersionAttributes(
    int thread_id,
    int registration_handle_id,
    int changed_mask,
    const ServiceWorkerVersionAttributes& attrs) {
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerDispatcher::OnSetVersionAttributes",
               "Thread ID", thread_id);

  // Adopt the references sent from the browser process and pass it to the
  // registration if it exists.
  std::unique_ptr<ServiceWorkerHandleReference> installing =
      Adopt(attrs.installing);
  std::unique_ptr<ServiceWorkerHandleReference> waiting = Adopt(attrs.waiting);
  std::unique_ptr<ServiceWorkerHandleReference> active = Adopt(attrs.active);

  RegistrationObjectMap::iterator found =
      registrations_.find(registration_handle_id);
  if (found != registrations_.end()) {
    // Populate the version fields (eg. .installing) with worker objects.
    ChangedVersionAttributesMask mask(changed_mask);
    if (mask.installing_changed())
      found->second->SetInstalling(
          GetOrCreateServiceWorker(std::move(installing)));
    if (mask.waiting_changed())
      found->second->SetWaiting(GetOrCreateServiceWorker(std::move(waiting)));
    if (mask.active_changed())
      found->second->SetActive(GetOrCreateServiceWorker(std::move(active)));
  }
}

void ServiceWorkerDispatcher::OnUpdateFound(
    int thread_id,
    int registration_handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcher::OnUpdateFound");
  RegistrationObjectMap::iterator found =
      registrations_.find(registration_handle_id);
  if (found != registrations_.end())
    found->second->OnUpdateFound();
}

void ServiceWorkerDispatcher::OnSetControllerServiceWorker(
    int thread_id,
    int provider_id,
    const ServiceWorkerObjectInfo& info,
    bool should_notify_controllerchange) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerDispatcher::OnSetControllerServiceWorker",
               "Thread ID", thread_id,
               "Provider ID", provider_id);

  // Adopt the reference sent from the browser process and pass it to the
  // provider context if it exists.
  std::unique_ptr<ServiceWorkerHandleReference> handle_ref = Adopt(info);
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider != provider_contexts_.end())
    provider->second->OnSetControllerServiceWorker(std::move(handle_ref));

  ProviderClientMap::iterator found = provider_clients_.find(provider_id);
  if (found != provider_clients_.end()) {
    // Get the existing worker object or create a new one with a new reference
    // to populate the .controller field.
    scoped_refptr<WebServiceWorkerImpl> worker = GetOrCreateServiceWorker(
        ServiceWorkerHandleReference::Create(info, thread_safe_sender_.get()));
    found->second->setController(WebServiceWorkerImpl::CreateHandle(worker),
                                 should_notify_controllerchange);
  }
}

void ServiceWorkerDispatcher::OnPostMessage(
    const ServiceWorkerMsg_MessageToDocument_Params& params) {
  // Make sure we're on the main document thread. (That must be the only
  // thread we get this message)
  DCHECK_EQ(kDocumentMainThreadId, params.thread_id);
  TRACE_EVENT1("ServiceWorker", "ServiceWorkerDispatcher::OnPostMessage",
               "Thread ID", params.thread_id);

  // Adopt the reference sent from the browser process and get the corresponding
  // worker object.
  scoped_refptr<WebServiceWorkerImpl> worker =
      GetOrCreateServiceWorker(Adopt(params.service_worker_info));

  ProviderClientMap::iterator found =
      provider_clients_.find(params.provider_id);
  if (found == provider_clients_.end()) {
    // For now we do no queueing for messages sent to nonexistent / unattached
    // client.
    return;
  }

  blink::WebMessagePortChannelArray ports =
      WebMessagePortChannelImpl::CreatePorts(
          params.message_ports, params.new_routing_ids,
          base::ThreadTaskRunnerHandle::Get());

  found->second->dispatchMessageEvent(
      WebServiceWorkerImpl::CreateHandle(worker), params.message, ports);
}

void ServiceWorkerDispatcher::AddServiceWorker(
    int handle_id, WebServiceWorkerImpl* worker) {
  DCHECK(!ContainsKey(service_workers_, handle_id));
  service_workers_[handle_id] = worker;
}

void ServiceWorkerDispatcher::RemoveServiceWorker(int handle_id) {
  DCHECK(ContainsKey(service_workers_, handle_id));
  service_workers_.erase(handle_id);
}

void ServiceWorkerDispatcher::AddServiceWorkerRegistration(
    int registration_handle_id,
    WebServiceWorkerRegistrationImpl* registration) {
  DCHECK(!ContainsKey(registrations_, registration_handle_id));
  registrations_[registration_handle_id] = registration;
}

void ServiceWorkerDispatcher::RemoveServiceWorkerRegistration(
    int registration_handle_id) {
  DCHECK(ContainsKey(registrations_, registration_handle_id));
  registrations_.erase(registration_handle_id);
}

std::unique_ptr<ServiceWorkerRegistrationHandleReference>
ServiceWorkerDispatcher::Adopt(
    const ServiceWorkerRegistrationObjectInfo& info) {
  return ServiceWorkerRegistrationHandleReference::Adopt(
      info, thread_safe_sender_.get());
}

std::unique_ptr<ServiceWorkerHandleReference> ServiceWorkerDispatcher::Adopt(
    const ServiceWorkerObjectInfo& info) {
  return ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get());
}

}  // namespace content
