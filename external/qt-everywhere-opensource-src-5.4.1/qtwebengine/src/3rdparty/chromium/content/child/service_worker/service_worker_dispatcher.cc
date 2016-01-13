// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_dispatcher.h"

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "content/child/child_thread.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/url_utils.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProviderClient.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

using blink::WebServiceWorkerError;
using blink::WebServiceWorkerProvider;
using base::ThreadLocalPointer;

namespace content {

namespace {

base::LazyInstance<ThreadLocalPointer<ServiceWorkerDispatcher> >::Leaky
    g_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

ServiceWorkerDispatcher* const kHasBeenDeleted =
    reinterpret_cast<ServiceWorkerDispatcher*>(0x1);

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

}  // namespace

ServiceWorkerDispatcher::ServiceWorkerDispatcher(
    ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender) {
  g_dispatcher_tls.Pointer()->Set(this);
}

ServiceWorkerDispatcher::~ServiceWorkerDispatcher() {
  g_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

void ServiceWorkerDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcher, msg)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistered, OnRegistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerUnregistered,
                        OnUnregistered)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerRegistrationError,
                        OnRegistrationError)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerStateChanged,
                        OnServiceWorkerStateChanged)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SetWaitingServiceWorker,
                        OnSetWaitingServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SetCurrentServiceWorker,
                        OnSetCurrentServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_MessageToDocument,
                        OnPostMessage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

bool ServiceWorkerDispatcher::Send(IPC::Message* msg) {
  return thread_safe_sender_->Send(msg);
}

void ServiceWorkerDispatcher::RegisterServiceWorker(
    int provider_id,
    const GURL& pattern,
    const GURL& script_url,
    WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks) {
  DCHECK(callbacks);

  if (pattern.possibly_invalid_spec().size() > GetMaxURLChars() ||
      script_url.possibly_invalid_spec().size() > GetMaxURLChars()) {
    scoped_ptr<WebServiceWorkerProvider::WebServiceWorkerCallbacks>
        owned_callbacks(callbacks);
    scoped_ptr<WebServiceWorkerError> error(new WebServiceWorkerError(
        WebServiceWorkerError::ErrorTypeSecurity, "URL too long"));
    callbacks->onError(error.release());
    return;
  }

  int request_id = pending_callbacks_.Add(callbacks);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_RegisterServiceWorker(
      CurrentWorkerId(), request_id, provider_id, pattern, script_url));
}

void ServiceWorkerDispatcher::UnregisterServiceWorker(
    int provider_id,
    const GURL& pattern,
    WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks) {
  DCHECK(callbacks);

  if (pattern.possibly_invalid_spec().size() > GetMaxURLChars()) {
    scoped_ptr<WebServiceWorkerProvider::WebServiceWorkerCallbacks>
        owned_callbacks(callbacks);
    scoped_ptr<WebServiceWorkerError> error(new WebServiceWorkerError(
        WebServiceWorkerError::ErrorTypeSecurity, "URL too long"));
    callbacks->onError(error.release());
    return;
  }

  int request_id = pending_callbacks_.Add(callbacks);
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_UnregisterServiceWorker(
      CurrentWorkerId(), request_id, provider_id, pattern));
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
  worker_to_provider_.erase(provider_context->waiting_handle_id());
  worker_to_provider_.erase(provider_context->current_handle_id());
}

void ServiceWorkerDispatcher::AddScriptClient(
    int provider_id,
    blink::WebServiceWorkerProviderClient* client) {
  DCHECK(client);
  DCHECK(!ContainsKey(script_clients_, provider_id));
  script_clients_[provider_id] = client;
}

void ServiceWorkerDispatcher::RemoveScriptClient(int provider_id) {
  // This could be possibly called multiple times to ensure termination.
  if (ContainsKey(script_clients_, provider_id))
    script_clients_.erase(provider_id);
}

ServiceWorkerDispatcher*
ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender) {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS ServiceWorkerDispatcher.";
    g_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_dispatcher_tls.Pointer()->Get())
    return g_dispatcher_tls.Pointer()->Get();

  ServiceWorkerDispatcher* dispatcher =
      new ServiceWorkerDispatcher(thread_safe_sender);
  if (WorkerTaskRunner::Instance()->CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(dispatcher);
  return dispatcher;
}

ServiceWorkerDispatcher* ServiceWorkerDispatcher::GetThreadSpecificInstance() {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted)
    return NULL;
  return g_dispatcher_tls.Pointer()->Get();
}

void ServiceWorkerDispatcher::OnWorkerRunLoopStopped() {
  delete this;
}

WebServiceWorkerImpl* ServiceWorkerDispatcher::GetServiceWorker(
    const ServiceWorkerObjectInfo& info,
    bool adopt_handle) {
  if (info.handle_id == kInvalidServiceWorkerHandleId)
    return NULL;

  WorkerObjectMap::iterator existing_worker =
      service_workers_.find(info.handle_id);

  if (existing_worker != service_workers_.end()) {
    if (adopt_handle) {
      // We are instructed to adopt a handle but we already have one, so
      // adopt and destroy a handle ref.
      ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_);
    }
    return existing_worker->second;
  }

  scoped_ptr<ServiceWorkerHandleReference> handle_ref =
      adopt_handle
          ? ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_)
          : ServiceWorkerHandleReference::Create(info, thread_safe_sender_);
  // WebServiceWorkerImpl constructor calls AddServiceWorker.
  return new WebServiceWorkerImpl(handle_ref.Pass(), thread_safe_sender_);
}

void ServiceWorkerDispatcher::OnRegistered(
    int thread_id,
    int request_id,
    const ServiceWorkerObjectInfo& info) {
  WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks =
      pending_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onSuccess(GetServiceWorker(info, true));
  pending_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnUnregistered(
    int thread_id,
    int request_id) {
  WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks =
      pending_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  callbacks->onSuccess(NULL);
  pending_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnRegistrationError(
    int thread_id,
    int request_id,
    WebServiceWorkerError::ErrorType error_type,
    const base::string16& message) {
  WebServiceWorkerProvider::WebServiceWorkerCallbacks* callbacks =
      pending_callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  scoped_ptr<WebServiceWorkerError> error(
      new WebServiceWorkerError(error_type, message));
  callbacks->onError(error.release());
  pending_callbacks_.Remove(request_id);
}

void ServiceWorkerDispatcher::OnServiceWorkerStateChanged(
    int thread_id,
    int handle_id,
    blink::WebServiceWorkerState state) {
  WorkerObjectMap::iterator worker = service_workers_.find(handle_id);
  if (worker != service_workers_.end())
    worker->second->OnStateChanged(state);

  WorkerToProviderMap::iterator provider = worker_to_provider_.find(handle_id);
  if (provider != worker_to_provider_.end())
    provider->second->OnServiceWorkerStateChanged(handle_id, state);
}

void ServiceWorkerDispatcher::OnSetWaitingServiceWorker(
    int thread_id,
    int provider_id,
    const ServiceWorkerObjectInfo& info) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider != provider_contexts_.end()) {
    int existing_waiting_id = provider->second->waiting_handle_id();
    if (existing_waiting_id != info.handle_id &&
        existing_waiting_id != kInvalidServiceWorkerHandleId) {
      WorkerToProviderMap::iterator associated_provider =
          worker_to_provider_.find(existing_waiting_id);
      DCHECK(associated_provider != worker_to_provider_.end());
      DCHECK(associated_provider->second->provider_id() == provider_id);
      worker_to_provider_.erase(associated_provider);
    }
    provider->second->OnSetWaitingServiceWorker(provider_id, info);
    if (info.handle_id != kInvalidServiceWorkerHandleId)
      worker_to_provider_[info.handle_id] = provider->second;
  }

  ScriptClientMap::iterator found = script_clients_.find(provider_id);
  if (found != script_clients_.end()) {
    // Populate the .waiting field with the new worker object.
    found->second->setWaiting(GetServiceWorker(info, false));
  }
}

void ServiceWorkerDispatcher::OnSetCurrentServiceWorker(
    int thread_id,
    int provider_id,
    const ServiceWorkerObjectInfo& info) {
  ProviderContextMap::iterator provider = provider_contexts_.find(provider_id);
  if (provider != provider_contexts_.end()) {
    provider->second->OnSetCurrentServiceWorker(provider_id, info);
    worker_to_provider_[info.handle_id] = provider->second;
  }

  ScriptClientMap::iterator found = script_clients_.find(provider_id);
  if (found != script_clients_.end()) {
    // Populate the .controller field with the new worker object.
    found->second->setController(GetServiceWorker(info, false));
  }
}

void ServiceWorkerDispatcher::OnPostMessage(
    int thread_id,
    int provider_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  // Make sure we're on the main document thread. (That must be the only
  // thread we get this message)
  DCHECK(ChildThread::current());

  ScriptClientMap::iterator found = script_clients_.find(provider_id);
  if (found == script_clients_.end()) {
    // For now we do no queueing for messages sent to nonexistent / unattached
    // client.
    return;
  }

  std::vector<WebMessagePortChannelImpl*> ports;
  if (!sent_message_port_ids.empty()) {
    ports.resize(sent_message_port_ids.size());
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      ports[i] = new WebMessagePortChannelImpl(
          new_routing_ids[i], sent_message_port_ids[i],
          base::MessageLoopProxy::current());
    }
  }

  found->second->dispatchMessageEvent(message, ports);
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

}  // namespace content
