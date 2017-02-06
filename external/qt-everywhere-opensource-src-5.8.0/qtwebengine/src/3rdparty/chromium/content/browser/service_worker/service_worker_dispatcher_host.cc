// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_client_utils.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "url/gurl.h"

using blink::WebServiceWorkerError;

namespace content {

namespace {

const char kNoDocumentURLErrorMessage[] =
    "No URL is associated with the caller's document.";
const char kShutdownErrorMessage[] =
    "The Service Worker system has shutdown.";
const char kUserDeniedPermissionMessage[] =
    "The user denied permission to use Service Worker.";
const char kInvalidStateErrorMessage[] = "The object is in an invalid state.";

const uint32_t kFilteredMessageClasses[] = {
    ServiceWorkerMsgStart, EmbeddedWorkerMsgStart,
};

void RunSoon(const base::Closure& callback) {
  if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

bool CanUnregisterServiceWorker(const GURL& document_url,
                                const GURL& pattern) {
  DCHECK(document_url.is_valid());
  DCHECK(pattern.is_valid());
  return document_url.GetOrigin() == pattern.GetOrigin() &&
         OriginCanAccessServiceWorkers(document_url) &&
         OriginCanAccessServiceWorkers(pattern);
}

bool CanUpdateServiceWorker(const GURL& document_url, const GURL& pattern) {
  DCHECK(document_url.is_valid());
  DCHECK(pattern.is_valid());
  DCHECK(OriginCanAccessServiceWorkers(document_url));
  DCHECK(OriginCanAccessServiceWorkers(pattern));
  return document_url.GetOrigin() == pattern.GetOrigin();
}

bool CanGetRegistration(const GURL& document_url,
                        const GURL& given_document_url) {
  DCHECK(document_url.is_valid());
  DCHECK(given_document_url.is_valid());
  return document_url.GetOrigin() == given_document_url.GetOrigin() &&
         OriginCanAccessServiceWorkers(document_url) &&
         OriginCanAccessServiceWorkers(given_document_url);
}

}  // namespace

ServiceWorkerDispatcherHost::ServiceWorkerDispatcherHost(
    int render_process_id,
    MessagePortMessageFilter* message_port_message_filter,
    ResourceContext* resource_context)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      message_port_message_filter_(message_port_message_filter),
      resource_context_(resource_context),
      channel_ready_(false) {
}

ServiceWorkerDispatcherHost::~ServiceWorkerDispatcherHost() {
  if (GetContext()) {
    GetContext()->RemoveAllProviderHostsForProcess(render_process_id_);
    GetContext()->embedded_worker_registry()->RemoveChildProcessSender(
        render_process_id_);
  }
}

void ServiceWorkerDispatcherHost::Init(
    ServiceWorkerContextWrapper* context_wrapper) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&ServiceWorkerDispatcherHost::Init, this,
                                       base::RetainedRef(context_wrapper)));
    return;
  }

  context_wrapper_ = context_wrapper;
  if (!GetContext())
    return;
  GetContext()->embedded_worker_registry()->AddChildProcessSender(
      render_process_id_, this, message_port_message_filter_);
}

void ServiceWorkerDispatcherHost::OnFilterAdded(IPC::Sender* sender) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnFilterAdded");
  channel_ready_ = true;
  std::vector<std::unique_ptr<IPC::Message>> messages;
  messages.swap(pending_messages_);
  for (auto& message : messages) {
    BrowserMessageFilter::Send(message.release());
  }
}

void ServiceWorkerDispatcherHost::OnFilterRemoved() {
  // Don't wait until the destructor to teardown since a new dispatcher host
  // for this process might be created before then.
  if (GetContext()) {
    GetContext()->RemoveAllProviderHostsForProcess(render_process_id_);
    GetContext()->embedded_worker_registry()->RemoveChildProcessSender(
        render_process_id_);
  }
  context_wrapper_ = nullptr;
  channel_ready_ = false;
}

void ServiceWorkerDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool ServiceWorkerDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcherHost, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_RegisterServiceWorker,
                        OnRegisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_UpdateServiceWorker,
                        OnUpdateServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_UnregisterServiceWorker,
                        OnUnregisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetRegistration,
                        OnGetRegistration)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetRegistrations,
                        OnGetRegistrations)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_GetRegistrationForReady,
                        OnGetRegistrationForReady)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ProviderCreated,
                        OnProviderCreated)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_ProviderDestroyed,
                        OnProviderDestroyed)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_SetVersionId,
                        OnSetHostedVersionId)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_PostMessageToWorker,
                        OnPostMessageToWorker)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerReadyForInspection,
                        OnWorkerReadyForInspection)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerScriptLoaded,
                        OnWorkerScriptLoaded)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerThreadStarted,
                        OnWorkerThreadStarted)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerScriptLoadFailed,
                        OnWorkerScriptLoadFailed)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerScriptEvaluated,
                        OnWorkerScriptEvaluated)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerStarted,
                        OnWorkerStarted)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_WorkerStopped,
                        OnWorkerStopped)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_ReportException,
                        OnReportException)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerHostMsg_ReportConsoleMessage,
                        OnReportConsoleMessage)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount,
                        OnIncrementServiceWorkerRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount,
                        OnDecrementServiceWorkerRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_IncrementRegistrationRefCount,
                        OnIncrementRegistrationRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_DecrementRegistrationRefCount,
                        OnDecrementRegistrationRefCount)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_TerminateWorker, OnTerminateWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled && GetContext()) {
    handled = GetContext()->embedded_worker_registry()->OnMessageReceived(
        message, render_process_id_);
    if (!handled)
      bad_message::ReceivedBadMessage(this, bad_message::SWDH_NOT_HANDLED);
  }

  return handled;
}

bool ServiceWorkerDispatcherHost::Send(IPC::Message* message) {
  if (channel_ready_) {
    BrowserMessageFilter::Send(message);
    // Don't bother passing through Send()'s result: it's not reliable.
    return true;
  }

  pending_messages_.push_back(base::WrapUnique(message));
  return true;
}

void ServiceWorkerDispatcherHost::RegisterServiceWorkerHandle(
    std::unique_ptr<ServiceWorkerHandle> handle) {
  int handle_id = handle->handle_id();
  handles_.AddWithID(handle.release(), handle_id);
}

void ServiceWorkerDispatcherHost::RegisterServiceWorkerRegistrationHandle(
    std::unique_ptr<ServiceWorkerRegistrationHandle> handle) {
  int handle_id = handle->handle_id();
  registration_handles_.AddWithID(handle.release(), handle_id);
}

ServiceWorkerHandle* ServiceWorkerDispatcherHost::FindServiceWorkerHandle(
    int provider_id,
    int64_t version_id) {
  for (IDMap<ServiceWorkerHandle, IDMapOwnPointer>::iterator iter(&handles_);
       !iter.IsAtEnd(); iter.Advance()) {
    ServiceWorkerHandle* handle = iter.GetCurrentValue();
    DCHECK(handle);
    DCHECK(handle->version());
    if (handle->provider_id() == provider_id &&
        handle->version()->version_id() == version_id) {
      return handle;
    }
  }
  return nullptr;
}

ServiceWorkerRegistrationHandle*
ServiceWorkerDispatcherHost::GetOrCreateRegistrationHandle(
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration) {
  DCHECK(provider_host);
  ServiceWorkerRegistrationHandle* existing_handle =
      FindRegistrationHandle(provider_host->provider_id(), registration->id());
  if (existing_handle) {
    existing_handle->IncrementRefCount();
    return existing_handle;
  }

  std::unique_ptr<ServiceWorkerRegistrationHandle> new_handle(
      new ServiceWorkerRegistrationHandle(GetContext()->AsWeakPtr(),
                                          provider_host, registration));
  ServiceWorkerRegistrationHandle* new_handle_ptr = new_handle.get();
  RegisterServiceWorkerRegistrationHandle(std::move(new_handle));
  return new_handle_ptr;
}

void ServiceWorkerDispatcherHost::OnRegisterServiceWorker(
    int thread_id,
    int request_id,
    int provider_id,
    const GURL& pattern,
    const GURL& script_url) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnRegisterServiceWorker");
  if (!GetContext()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kServiceWorkerRegisterErrorPrefix) +
            base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }
  if (!pattern.is_valid() || !script_url.is_valid()) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_REGISTER_BAD_URL);
    return;
  }

  ServiceWorkerProviderHost* provider_host = GetContext()->GetProviderHost(
      render_process_id_, provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_REGISTER_NO_HOST);
    return;
  }
  if (!provider_host->IsContextAlive()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kServiceWorkerRegisterErrorPrefix) +
            base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  // TODO(ksakamoto): Currently, document_url is empty if the document is in an
  // IFRAME using frame.contentDocument.write(...). We can remove this check
  // once crbug.com/439697 is fixed.
  if (provider_host->document_url().is_empty()) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeSecurity,
        base::ASCIIToUTF16(kServiceWorkerRegisterErrorPrefix) +
            base::ASCIIToUTF16(kNoDocumentURLErrorMessage)));
    return;
  }

  if (!ServiceWorkerUtils::CanRegisterServiceWorker(
          provider_host->document_url(), pattern, script_url)) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_REGISTER_CANNOT);
    return;
  }

  std::string error_message;
  if (ServiceWorkerUtils::ContainsDisallowedCharacter(pattern, script_url,
                                                      &error_message)) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_REGISTER_CANNOT);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          pattern, provider_host->topmost_frame_url(), resource_context_,
          render_process_id_, provider_host->frame_id())) {
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeUnknown,
        base::ASCIIToUTF16(kServiceWorkerRegisterErrorPrefix) +
            base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerDispatcherHost::RegisterServiceWorker",
      request_id, "Scope", pattern.spec(), "Script URL", script_url.spec());
  GetContext()->RegisterServiceWorker(
      pattern,
      script_url,
      provider_host,
      base::Bind(&ServiceWorkerDispatcherHost::RegistrationComplete,
                 this,
                 thread_id,
                 provider_id,
                 request_id));
}

void ServiceWorkerDispatcherHost::OnUpdateServiceWorker(
    int thread_id,
    int request_id,
    int provider_id,
    int64_t registration_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnUpdateServiceWorker");
  if (!GetContext()) {
    Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
            base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_UPDATE_NO_HOST);
    return;
  }
  if (!provider_host->IsContextAlive()) {
    Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
            base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  // TODO(jungkees): This check can be removed once crbug.com/439697 is fixed.
  if (provider_host->document_url().is_empty()) {
    Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeSecurity,
        base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
            base::ASCIIToUTF16(kNoDocumentURLErrorMessage)));
    return;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  if (!registration) {
    // |registration| must be alive because a renderer retains a registration
    // reference at this point.
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_UPDATE_BAD_REGISTRATION_ID);
    return;
  }

  if (!CanUpdateServiceWorker(provider_host->document_url(),
                              registration->pattern())) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_UPDATE_CANNOT);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(), provider_host->topmost_frame_url(),
          resource_context_, render_process_id_, provider_host->frame_id())) {
    Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeUnknown,
        base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
            base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  if (!registration->GetNewestVersion()) {
    // This can happen if update() is called during initial script evaluation.
    // Abort the following steps according to the spec.
    Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeState,
        base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) +
            base::ASCIIToUTF16(kInvalidStateErrorMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcherHost::UpdateServiceWorker",
                           request_id, "Scope", registration->pattern().spec());
  GetContext()->UpdateServiceWorker(
      registration, false /* force_bypass_cache */,
      false /* skip_script_comparison */, provider_host,
      base::Bind(&ServiceWorkerDispatcherHost::UpdateComplete, this, thread_id,
                 provider_id, request_id));
}

void ServiceWorkerDispatcherHost::OnUnregisterServiceWorker(
    int thread_id,
    int request_id,
    int provider_id,
    int64_t registration_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnUnregisterServiceWorker");
  if (!GetContext()) {
    Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
        thread_id, request_id, blink::WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_UNREGISTER_NO_HOST);
    return;
  }
  if (!provider_host->IsContextAlive()) {
    Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
        thread_id, request_id, blink::WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  // TODO(ksakamoto): This check can be removed once crbug.com/439697 is fixed.
  if (provider_host->document_url().is_empty()) {
    Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeSecurity,
        base::ASCIIToUTF16(kNoDocumentURLErrorMessage)));
    return;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  if (!registration) {
    // |registration| must be alive because a renderer retains a registration
    // reference at this point.
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_UNREGISTER_BAD_REGISTRATION_ID);
    return;
  }

  if (!CanUnregisterServiceWorker(provider_host->document_url(),
                                  registration->pattern())) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_UNREGISTER_CANNOT);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          registration->pattern(), provider_host->topmost_frame_url(),
          resource_context_, render_process_id_, provider_host->frame_id())) {
    Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeUnknown,
        base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1(
      "ServiceWorker", "ServiceWorkerDispatcherHost::UnregisterServiceWorker",
      request_id, "Scope", registration->pattern().spec());
  GetContext()->UnregisterServiceWorker(
      registration->pattern(),
      base::Bind(&ServiceWorkerDispatcherHost::UnregistrationComplete, this,
                 thread_id, request_id));
}

void ServiceWorkerDispatcherHost::OnGetRegistration(
    int thread_id,
    int request_id,
    int provider_id,
    const GURL& document_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnGetRegistration");

  if (!GetContext()) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationError(
        thread_id, request_id, blink::WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
            base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }
  if (!document_url.is_valid()) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::SWDH_GET_REGISTRATION_BAD_URL);
    return;
  }

  ServiceWorkerProviderHost* provider_host = GetContext()->GetProviderHost(
      render_process_id_, provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::SWDH_GET_REGISTRATION_NO_HOST);
    return;
  }
  if (!provider_host->IsContextAlive()) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationError(
        thread_id, request_id, blink::WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
            base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  // TODO(ksakamoto): This check can be removed once crbug.com/439697 is fixed.
  if (provider_host->document_url().is_empty()) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeSecurity,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
            base::ASCIIToUTF16(kNoDocumentURLErrorMessage)));
    return;
  }

  if (!CanGetRegistration(provider_host->document_url(), document_url)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::SWDH_GET_REGISTRATION_CANNOT);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          provider_host->document_url(), provider_host->topmost_frame_url(),
          resource_context_, render_process_id_, provider_host->frame_id())) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeUnknown,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
            base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerDispatcherHost::GetRegistration",
                           request_id, "Document URL", document_url.spec());
  GetContext()->storage()->FindRegistrationForDocument(
      document_url,
      base::Bind(&ServiceWorkerDispatcherHost::GetRegistrationComplete,
                 this,
                 thread_id,
                 provider_id,
                 request_id));
}

void ServiceWorkerDispatcherHost::OnGetRegistrations(int thread_id,
                                                     int request_id,
                                                     int provider_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnGetRegistrations");

  if (!GetContext()) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationsError(
        thread_id, request_id, blink::WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationsErrorPrefix) +
            base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_GET_REGISTRATIONS_NO_HOST);
    return;
  }
  if (!provider_host->IsContextAlive()) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationsError(
        thread_id, request_id, blink::WebServiceWorkerError::ErrorTypeAbort,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationsErrorPrefix) +
            base::ASCIIToUTF16(kShutdownErrorMessage)));
    return;
  }

  // TODO(jungkees): This check can be removed once crbug.com/439697 is fixed.
  if (provider_host->document_url().is_empty()) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationsError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeSecurity,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationsErrorPrefix) +
            base::ASCIIToUTF16(kNoDocumentURLErrorMessage)));
    return;
  }

  if (!OriginCanAccessServiceWorkers(provider_host->document_url())) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_GET_REGISTRATIONS_INVALID_ORIGIN);
    return;
  }

  if (!GetContentClient()->browser()->AllowServiceWorker(
          provider_host->document_url(), provider_host->topmost_frame_url(),
          resource_context_, render_process_id_, provider_host->frame_id())) {
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationsError(
        thread_id, request_id, WebServiceWorkerError::ErrorTypeUnknown,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationsErrorPrefix) +
            base::ASCIIToUTF16(kUserDeniedPermissionMessage)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerDispatcherHost::GetRegistrations",
                           request_id);
  GetContext()->storage()->GetRegistrationsForOrigin(
      provider_host->document_url().GetOrigin(),
      base::Bind(&ServiceWorkerDispatcherHost::GetRegistrationsComplete, this,
                 thread_id, provider_id, request_id));
}

void ServiceWorkerDispatcherHost::OnGetRegistrationForReady(
    int thread_id,
    int request_id,
    int provider_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnGetRegistrationForReady");
  if (!GetContext())
    return;
  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_GET_REGISTRATION_FOR_READY_NO_HOST);
    return;
  }
  if (!provider_host->IsContextAlive())
    return;

  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerDispatcherHost::GetRegistrationForReady",
      request_id);
  if (!provider_host->GetRegistrationForReady(base::Bind(
          &ServiceWorkerDispatcherHost::GetRegistrationForReadyComplete,
          this, thread_id, request_id, provider_host->AsWeakPtr()))) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_GET_REGISTRATION_FOR_READY_ALREADY_IN_PROGRESS);
  }
}

void ServiceWorkerDispatcherHost::OnPostMessageToWorker(
    int handle_id,
    int provider_id,
    const base::string16& message,
    const url::Origin& source_origin,
    const std::vector<int>& sent_message_ports) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnPostMessageToWorker");
  if (!GetContext())
    return;

  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(this, bad_message::SWDH_POST_MESSAGE);
    return;
  }

  ServiceWorkerProviderHost* sender_provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!sender_provider_host) {
    // This may occur when destruction of the sender provider overtakes
    // postMessage() because of thread hopping on WebServiceWorkerImpl.
    return;
  }

  DispatchExtendableMessageEvent(
      make_scoped_refptr(handle->version()), message, source_origin,
      sent_message_ports, sender_provider_host,
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

void ServiceWorkerDispatcherHost::DispatchExtendableMessageEvent(
    scoped_refptr<ServiceWorkerVersion> worker,
    const base::string16& message,
    const url::Origin& source_origin,
    const std::vector<int>& sent_message_ports,
    ServiceWorkerProviderHost* sender_provider_host,
    const StatusCallback& callback) {
  for (int port : sent_message_ports)
    MessagePortService::GetInstance()->HoldMessages(port);

  switch (sender_provider_host->provider_type()) {
    case SERVICE_WORKER_PROVIDER_FOR_WINDOW:
    case SERVICE_WORKER_PROVIDER_FOR_WORKER:
    case SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      service_worker_client_utils::GetClient(
          sender_provider_host,
          base::Bind(&ServiceWorkerDispatcherHost::
                         DispatchExtendableMessageEventInternal<
                             ServiceWorkerClientInfo>,
                     this, worker, message, source_origin, sent_message_ports,
                     callback));
      break;
    case SERVICE_WORKER_PROVIDER_FOR_CONTROLLER:
      RunSoon(base::Bind(
          &ServiceWorkerDispatcherHost::DispatchExtendableMessageEventInternal<
              ServiceWorkerObjectInfo>,
          this, worker, message, source_origin, sent_message_ports, callback,
          sender_provider_host->GetOrCreateServiceWorkerHandle(
              sender_provider_host->running_hosted_version())));
      break;
    case SERVICE_WORKER_PROVIDER_UNKNOWN:
    default:
      NOTREACHED() << sender_provider_host->provider_type();
      break;
  }
}

void ServiceWorkerDispatcherHost::OnProviderCreated(
    int provider_id,
    int route_id,
    ServiceWorkerProviderType provider_type,
    bool is_parent_frame_secure) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "477117 ServiceWorkerDispatcherHost::OnProviderCreated"));
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnProviderCreated");
  if (!GetContext())
    return;
  if (GetContext()->GetProviderHost(render_process_id_, provider_id)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::SWDH_PROVIDER_CREATED_NO_HOST);
    return;
  }

  ServiceWorkerProviderHost::FrameSecurityLevel parent_frame_security_level =
      is_parent_frame_secure
          ? ServiceWorkerProviderHost::FrameSecurityLevel::SECURE
          : ServiceWorkerProviderHost::FrameSecurityLevel::INSECURE;
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      std::unique_ptr<ServiceWorkerProviderHost>(new ServiceWorkerProviderHost(
          render_process_id_, route_id, provider_id, provider_type,
          parent_frame_security_level, GetContext()->AsWeakPtr(), this));
  GetContext()->AddProviderHost(std::move(provider_host));
}

void ServiceWorkerDispatcherHost::OnProviderDestroyed(int provider_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnProviderDestroyed");
  if (!GetContext())
    return;
  if (!GetContext()->GetProviderHost(render_process_id_, provider_id)) {
    // PlzNavigate: in some cancellation of navigation cases, it is possible
    // for the pre-created hoist to have been destroyed before being claimed by
    // the renderer. The provider is then destroyed in the renderer, and no
    // matching host will be found.
    if (!IsBrowserSideNavigationEnabled() ||
        !ServiceWorkerUtils::IsBrowserAssignedProviderId(provider_id)) {
      bad_message::ReceivedBadMessage(
          this, bad_message::SWDH_PROVIDER_DESTROYED_NO_HOST);
    }
    return;
  }
  GetContext()->RemoveProviderHost(render_process_id_, provider_id);
}

void ServiceWorkerDispatcherHost::OnSetHostedVersionId(int provider_id,
                                                       int64_t version_id,
                                                       int embedded_worker_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnSetHostedVersionId");
  if (!GetContext())
    return;
  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_SET_HOSTED_VERSION_NO_HOST);
    return;
  }

  // This provider host must be specialized for a controller.
  if (provider_host->IsProviderForClient()) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_SET_HOSTED_VERSION_INVALID_HOST);
    return;
  }

  // A service worker context associated with this provider host was destroyed
  // due to restarting the service worker system etc.
  if (!provider_host->IsContextAlive())
    return;

  // We might not be STARTING if the stop sequence was entered (STOPPING) or
  // ended up being detached (STOPPED).
  ServiceWorkerVersion* version = GetContext()->GetLiveVersion(version_id);
  if (!version || version->running_status() != EmbeddedWorkerStatus::STARTING)
    return;

  // If the version has a different embedded worker, assume the message is about
  // a detached worker and ignore.
  if (version->embedded_worker()->embedded_worker_id() != embedded_worker_id)
    return;

  // A process for the worker must be equal to a process for the provider host.
  if (version->embedded_worker()->process_id() != provider_host->process_id()) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_SET_HOSTED_VERSION_PROCESS_MISMATCH);
    return;
  }

  provider_host->SetHostedVersion(version);

  // Retrieve the registration associated with |version|. The registration
  // must be alive because the version keeps it during starting worker.
  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(version->registration_id());
  DCHECK(registration);

  // Set the document URL to the script url in order to allow
  // register/unregister/getRegistration on ServiceWorkerGlobalScope.
  provider_host->SetDocumentUrl(version->script_url());

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  GetRegistrationObjectInfoAndVersionAttributes(
      provider_host->AsWeakPtr(), registration, &info, &attrs);

  Send(new ServiceWorkerMsg_AssociateRegistration(kDocumentMainThreadId,
                                                  provider_id, info, attrs));
}

template <typename SourceInfo>
void ServiceWorkerDispatcherHost::DispatchExtendableMessageEventInternal(
    scoped_refptr<ServiceWorkerVersion> worker,
    const base::string16& message,
    const url::Origin& source_origin,
    const std::vector<int>& sent_message_ports,
    const StatusCallback& callback,
    const SourceInfo& source_info) {
  if (!source_info.IsValid()) {
    DidFailToDispatchExtendableMessageEvent<SourceInfo>(
        sent_message_ports, source_info, callback, SERVICE_WORKER_ERROR_FAILED);
    return;
  }
  worker->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::MESSAGE,
      base::Bind(&ServiceWorkerDispatcherHost::
                     DispatchExtendableMessageEventAfterStartWorker,
                 this, worker, message, source_origin, sent_message_ports,
                 ExtendableMessageEventSource(source_info), callback),
      base::Bind(
          &ServiceWorkerDispatcherHost::DidFailToDispatchExtendableMessageEvent<
              SourceInfo>,
          this, sent_message_ports, source_info, callback));
}

void ServiceWorkerDispatcherHost::
    DispatchExtendableMessageEventAfterStartWorker(
        scoped_refptr<ServiceWorkerVersion> worker,
        const base::string16& message,
        const url::Origin& source_origin,
        const std::vector<int>& sent_message_ports,
        const ExtendableMessageEventSource& source,
        const StatusCallback& callback) {
  int request_id =
      worker->StartRequest(ServiceWorkerMetrics::EventType::MESSAGE, callback);

  MessagePortMessageFilter* filter =
      worker->embedded_worker()->message_port_message_filter();
  std::vector<int> new_routing_ids;
  filter->UpdateMessagePortsWithNewRoutes(sent_message_ports, &new_routing_ids);

  ServiceWorkerMsg_ExtendableMessageEvent_Params params;
  params.message = message;
  params.source_origin = source_origin;
  params.message_ports = sent_message_ports;
  params.new_routing_ids = new_routing_ids;
  params.source = source;

  // Hide the client url if the client has a unique origin.
  if (source_origin.unique()) {
    if (params.source.client_info.IsValid())
      params.source.client_info.url = GURL();
    else
      params.source.service_worker_info.url = GURL();
  }

  worker->DispatchSimpleEvent<
      ServiceWorkerHostMsg_ExtendableMessageEventFinished>(
      request_id, ServiceWorkerMsg_ExtendableMessageEvent(request_id, params));
}

template <typename SourceInfo>
void ServiceWorkerDispatcherHost::DidFailToDispatchExtendableMessageEvent(
    const std::vector<int>& sent_message_ports,
    const SourceInfo& source_info,
    const StatusCallback& callback,
    ServiceWorkerStatusCode status) {
  // Transfering the message ports failed, so destroy the ports.
  for (int port : sent_message_ports)
    MessagePortService::GetInstance()->ClosePort(port);
  if (source_info.IsValid())
    ReleaseSourceInfo(source_info);
  callback.Run(status);
}

void ServiceWorkerDispatcherHost::ReleaseSourceInfo(
    const ServiceWorkerClientInfo& source_info) {
  // ServiceWorkerClientInfo is just a snapshot of the client. There is no need
  // to do anything for it.
}

void ServiceWorkerDispatcherHost::ReleaseSourceInfo(
    const ServiceWorkerObjectInfo& source_info) {
  ServiceWorkerHandle* handle = handles_.Lookup(source_info.handle_id);
  DCHECK(handle);
  handle->DecrementRefCount();
  if (handle->HasNoRefCount())
    handles_.Remove(source_info.handle_id);
}

ServiceWorkerRegistrationHandle*
ServiceWorkerDispatcherHost::FindRegistrationHandle(int provider_id,
                                                    int64_t registration_id) {
  for (RegistrationHandleMap::iterator iter(&registration_handles_);
       !iter.IsAtEnd(); iter.Advance()) {
    ServiceWorkerRegistrationHandle* handle = iter.GetCurrentValue();
    if (handle->provider_id() == provider_id &&
        handle->registration()->id() == registration_id) {
      return handle;
    }
  }
  return nullptr;
}

void ServiceWorkerDispatcherHost::GetRegistrationObjectInfoAndVersionAttributes(
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration,
    ServiceWorkerRegistrationObjectInfo* info,
    ServiceWorkerVersionAttributes* attrs) {
  ServiceWorkerRegistrationHandle* handle =
      GetOrCreateRegistrationHandle(provider_host, registration);
  *info = handle->GetObjectInfo();

  attrs->installing = provider_host->GetOrCreateServiceWorkerHandle(
      registration->installing_version());
  attrs->waiting = provider_host->GetOrCreateServiceWorkerHandle(
      registration->waiting_version());
  attrs->active = provider_host->GetOrCreateServiceWorkerHandle(
      registration->active_version());
}

void ServiceWorkerDispatcherHost::RegistrationComplete(
    int thread_id,
    int provider_id,
    int request_id,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerDispatcherHost::RegisterServiceWorker",
      request_id, "Status", status, "Registration ID", registration_id);
  if (!GetContext())
    return;

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;  // The provider has already been destroyed.

  if (status != SERVICE_WORKER_OK) {
    base::string16 error_message;
    blink::WebServiceWorkerError::ErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, status_message,
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerRegistrationError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerRegisterErrorPrefix) + error_message));
    return;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  DCHECK(registration);

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  GetRegistrationObjectInfoAndVersionAttributes(
      provider_host->AsWeakPtr(), registration, &info, &attrs);

  Send(new ServiceWorkerMsg_ServiceWorkerRegistered(
      thread_id, request_id, info, attrs));
}

void ServiceWorkerDispatcherHost::UpdateComplete(
    int thread_id,
    int provider_id,
    int request_id,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerDispatcherHost::UpdateServiceWorker",
      request_id, "Status", status, "Registration ID", registration_id);
  if (!GetContext())
    return;

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;  // The provider has already been destroyed.

  if (status != SERVICE_WORKER_OK) {
    base::string16 error_message;
    blink::WebServiceWorkerError::ErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, status_message,
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerUpdateError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerUpdateErrorPrefix) + error_message));
    return;
  }

  ServiceWorkerRegistration* registration =
      GetContext()->GetLiveRegistration(registration_id);
  DCHECK(registration);

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  GetRegistrationObjectInfoAndVersionAttributes(provider_host->AsWeakPtr(),
                                                registration, &info, &attrs);

  Send(new ServiceWorkerMsg_ServiceWorkerUpdated(thread_id, request_id));
}

void ServiceWorkerDispatcherHost::OnWorkerReadyForInspection(
    int embedded_worker_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnWorkerReadyForInspection");
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerReadyForInspection(render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerScriptLoaded(int embedded_worker_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnWorkerScriptLoaded");
  if (!GetContext())
    return;

  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerScriptLoaded(render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerThreadStarted(int embedded_worker_id,
                                                        int thread_id,
                                                        int provider_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnWorkerThreadStarted");
  if (!GetContext())
    return;

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_WORKER_SCRIPT_LOAD_NO_HOST);
    return;
  }

  provider_host->SetReadyToSendMessagesToWorker(thread_id);

  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerThreadStarted(render_process_id_, thread_id,
                                  embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerScriptLoadFailed(
    int embedded_worker_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnWorkerScriptLoadFailed");
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerScriptLoadFailed(render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerScriptEvaluated(
    int embedded_worker_id,
    bool success) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnWorkerScriptEvaluated");
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerScriptEvaluated(
      render_process_id_, embedded_worker_id, success);
}

void ServiceWorkerDispatcherHost::OnWorkerStarted(int embedded_worker_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnWorkerStarted");
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerStarted(render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnWorkerStopped(int embedded_worker_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnWorkerStopped");
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnWorkerStopped(render_process_id_, embedded_worker_id);
}

void ServiceWorkerDispatcherHost::OnReportException(
    int embedded_worker_id,
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnReportException");
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnReportException(embedded_worker_id,
                              error_message,
                              line_number,
                              column_number,
                              source_url);
}

void ServiceWorkerDispatcherHost::OnReportConsoleMessage(
    int embedded_worker_id,
    const EmbeddedWorkerHostMsg_ReportConsoleMessage_Params& params) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnReportConsoleMessage");
  if (!GetContext())
    return;
  EmbeddedWorkerRegistry* registry = GetContext()->embedded_worker_registry();
  if (!registry->CanHandle(embedded_worker_id))
    return;
  registry->OnReportConsoleMessage(embedded_worker_id,
                                   params.source_identifier,
                                   params.message_level,
                                   params.message,
                                   params.line_number,
                                   params.source_url);
}

void ServiceWorkerDispatcherHost::OnIncrementServiceWorkerRefCount(
    int handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnIncrementServiceWorkerRefCount");
  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_INCREMENT_WORKER_BAD_HANDLE);
    return;
  }
  handle->IncrementRefCount();
}

void ServiceWorkerDispatcherHost::OnDecrementServiceWorkerRefCount(
    int handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnDecrementServiceWorkerRefCount");
  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_DECREMENT_WORKER_BAD_HANDLE);
    return;
  }
  handle->DecrementRefCount();
  if (handle->HasNoRefCount())
    handles_.Remove(handle_id);
}

void ServiceWorkerDispatcherHost::OnIncrementRegistrationRefCount(
    int registration_handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnIncrementRegistrationRefCount");
  ServiceWorkerRegistrationHandle* handle =
      registration_handles_.Lookup(registration_handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_INCREMENT_REGISTRATION_BAD_HANDLE);
    return;
  }
  handle->IncrementRefCount();
}

void ServiceWorkerDispatcherHost::OnDecrementRegistrationRefCount(
    int registration_handle_id) {
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerDispatcherHost::OnDecrementRegistrationRefCount");
  ServiceWorkerRegistrationHandle* handle =
      registration_handles_.Lookup(registration_handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(
        this, bad_message::SWDH_DECREMENT_REGISTRATION_BAD_HANDLE);
    return;
  }
  handle->DecrementRefCount();
  if (handle->HasNoRefCount())
    registration_handles_.Remove(registration_handle_id);
}

void ServiceWorkerDispatcherHost::UnregistrationComplete(
    int thread_id,
    int request_id,
    ServiceWorkerStatusCode status) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerDispatcherHost::UnregisterServiceWorker",
                         request_id, "Status", status);
  if (status != SERVICE_WORKER_OK && status != SERVICE_WORKER_ERROR_NOT_FOUND) {
    base::string16 error_message;
    blink::WebServiceWorkerError::ErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, std::string(),
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerUnregistrationError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerUnregisterErrorPrefix) +
            error_message));
    return;
  }
  const bool is_success = (status == SERVICE_WORKER_OK);
  Send(new ServiceWorkerMsg_ServiceWorkerUnregistered(thread_id,
                                                      request_id,
                                                      is_success));
}

void ServiceWorkerDispatcherHost::GetRegistrationComplete(
    int thread_id,
    int provider_id,
    int request_id,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerDispatcherHost::GetRegistration",
      request_id, "Status", status, "Registration ID",
      registration ? registration->id() : kInvalidServiceWorkerRegistrationId);
  if (!GetContext())
    return;

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;  // The provider has already been destroyed.

  if (status != SERVICE_WORKER_OK && status != SERVICE_WORKER_ERROR_NOT_FOUND) {
    base::string16 error_message;
    blink::WebServiceWorkerError::ErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, std::string(),
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
            error_message));

    return;
  }

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  if (status == SERVICE_WORKER_OK) {
    DCHECK(registration.get());
    if (!registration->is_uninstalling()) {
      GetRegistrationObjectInfoAndVersionAttributes(
          provider_host->AsWeakPtr(), registration.get(), &info, &attrs);
    }
  }

  Send(new ServiceWorkerMsg_DidGetRegistration(
      thread_id, request_id, info, attrs));
}

void ServiceWorkerDispatcherHost::GetRegistrationsComplete(
    int thread_id,
    int provider_id,
    int request_id,
    ServiceWorkerStatusCode status,
    const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
        registrations) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerDispatcherHost::GetRegistrations",
                         request_id, "Status", status);
  if (!GetContext())
    return;

  ServiceWorkerProviderHost* provider_host =
      GetContext()->GetProviderHost(render_process_id_, provider_id);
  if (!provider_host)
    return;  // The provider has already been destroyed.

  if (status != SERVICE_WORKER_OK) {
    base::string16 error_message;
    blink::WebServiceWorkerError::ErrorType error_type;
    GetServiceWorkerRegistrationStatusResponse(status, std::string(),
                                               &error_type, &error_message);
    Send(new ServiceWorkerMsg_ServiceWorkerGetRegistrationsError(
        thread_id, request_id, error_type,
        base::ASCIIToUTF16(kServiceWorkerGetRegistrationErrorPrefix) +
            error_message));
    return;
  }

  std::vector<ServiceWorkerRegistrationObjectInfo> object_infos;
  std::vector<ServiceWorkerVersionAttributes> version_attrs;

  for (const auto& registration : registrations) {
    DCHECK(registration.get());
    if (!registration->is_uninstalling()) {
      ServiceWorkerRegistrationObjectInfo object_info;
      ServiceWorkerVersionAttributes version_attr;
      GetRegistrationObjectInfoAndVersionAttributes(
          provider_host->AsWeakPtr(), registration.get(), &object_info,
          &version_attr);
      object_infos.push_back(object_info);
      version_attrs.push_back(version_attr);
    }
  }

  Send(new ServiceWorkerMsg_DidGetRegistrations(thread_id, request_id,
                                                object_infos, version_attrs));
}

void ServiceWorkerDispatcherHost::GetRegistrationForReadyComplete(
    int thread_id,
    int request_id,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    ServiceWorkerRegistration* registration) {
  DCHECK(registration);
  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerDispatcherHost::GetRegistrationForReady",
      request_id, "Registration ID",
      registration ? registration->id() : kInvalidServiceWorkerRegistrationId);
  if (!GetContext())
    return;

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  GetRegistrationObjectInfoAndVersionAttributes(
      provider_host, registration, &info, &attrs);
  Send(new ServiceWorkerMsg_DidGetRegistrationForReady(
        thread_id, request_id, info, attrs));
}

ServiceWorkerContextCore* ServiceWorkerDispatcherHost::GetContext() {
  if (!context_wrapper_.get())
    return nullptr;
  return context_wrapper_->context();
}

void ServiceWorkerDispatcherHost::OnTerminateWorker(int handle_id) {
  ServiceWorkerHandle* handle = handles_.Lookup(handle_id);
  if (!handle) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::SWDH_TERMINATE_BAD_HANDLE);
    return;
  }
  handle->version()->StopWorker(
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

}  // namespace content
