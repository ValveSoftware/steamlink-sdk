// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging/push_messaging_message_filter.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/push_messaging_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/console_message_level.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/push_messaging_status.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"

namespace content {

// Service Worker database keys. If a registration ID is stored, the stored
// sender ID must be the one used to subscribe. Unfortunately, this isn't always
// true of subscriptions previously stored in the database.
const char kPushSenderIdServiceWorkerKey[] = "push_sender_id";
const char kPushRegistrationIdServiceWorkerKey[] = "push_registration_id";

namespace {

// Chrome currently does not support the Push API in incognito.
const char kIncognitoPushUnsupportedMessage[] =
    "Chrome currently does not support the Push API in incognito mode "
    "(https://crbug.com/401439). There is deliberately no way to "
    "feature-detect this, since incognito mode needs to be undetectable by "
    "websites.";

// These UMA methods are only called from IO thread, but it would be acceptable
// (even though slightly racy) to call them from UI thread as well, see
// https://groups.google.com/a/chromium.org/d/msg/chromium-dev/FNzZRJtN2aw/Aw0CWAXJJ1kJ
void RecordRegistrationStatus(PushRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.RegistrationStatus", status,
                            PUSH_REGISTRATION_STATUS_LAST + 1);
}

void RecordUnregistrationStatus(PushUnregistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UnregistrationStatus", status,
                            PUSH_UNREGISTRATION_STATUS_LAST + 1);
}

void RecordGetRegistrationStatus(PushGetRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.GetRegistrationStatus", status,
                            PUSH_GETREGISTRATION_STATUS_LAST + 1);
}

// Curries the |success| and |p256dh| parameters over to |callback| and
// posts a task to invoke |callback| on the IO thread.
void ForwardEncryptionInfoToIOThreadProxy(
    const PushMessagingService::EncryptionInfoCallback& callback,
    bool success,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, success, p256dh, auth));
}

// Returns whether |sender_info| contains a valid application server key, that
// is, a NIST P-256 public key in uncompressed format.
bool IsApplicationServerKey(const std::string& sender_info) {
  return sender_info.size() == 65 && sender_info[0] == 0x04;
}

}  // namespace

struct PushMessagingMessageFilter::RegisterData {
  RegisterData();
  RegisterData(const RegisterData& other) = default;
  bool FromDocument() const;
  int request_id;
  GURL requesting_origin;
  int64_t service_worker_registration_id;
  PushSubscriptionOptions options;
  // The following member should only be read if FromDocument() is true.
  int render_frame_id;
};

// Inner core of this message filter which lives on the UI thread.
class PushMessagingMessageFilter::Core {
 public:
  Core(const base::WeakPtr<PushMessagingMessageFilter>& io_parent,
       int render_process_id);

  // Public Register methods on UI thread --------------------------------------

  // Called via PostTask from IO thread.
  void RegisterOnUI(const RegisterData& data);

  // Public Unregister methods on UI thread ------------------------------------

  // Called via PostTask from IO thread.
  void UnregisterFromService(int request_id,
                             int64_t service_worker_registration_id,
                             const GURL& requesting_origin,
                             const std::string& sender_id);

  // Public GetPermission methods on UI thread ---------------------------------

  // Called via PostTask from IO thread.
  void GetPermissionStatusOnUI(const GURL& requesting_origin,
                               bool user_visible,
                               int request_id);

  // Public helper methods on UI thread ----------------------------------------

  // Called via PostTask from IO thread. The |io_thread_callback| callback
  // will be invoked on the IO thread.
  void GetEncryptionInfoOnUI(
      const GURL& origin,
      int64_t service_worker_registration_id,
      const PushMessagingService::EncryptionInfoCallback& io_thread_callback);

  // Called (directly) from both the UI and IO threads.
  bool is_incognito() const { return is_incognito_; }

  // Returns a push messaging service. May return null.
  PushMessagingService* service();

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<Core>;

  ~Core();

  // Private Register methods on UI thread -------------------------------------

  void DidRequestPermissionInIncognito(const RegisterData& data,
                                       blink::mojom::PermissionStatus status);

  void DidRegister(const RegisterData& data,
                   const std::string& push_registration_id,
                   const std::vector<uint8_t>& p256dh,
                   const std::vector<uint8_t>& auth,
                   PushRegistrationStatus status);

  // Private Unregister methods on UI thread -----------------------------------

  void DidUnregisterFromService(int request_id,
                                int64_t service_worker_registration_id,
                                PushUnregistrationStatus unregistration_status);

  // Private helper methods on UI thread ---------------------------------------

  void Send(IPC::Message* message);

  // Outer part of this message filter which lives on the IO thread.
  base::WeakPtr<PushMessagingMessageFilter> io_parent_;

  int render_process_id_;

  bool is_incognito_;

  base::WeakPtrFactory<Core> weak_factory_ui_to_ui_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

PushMessagingMessageFilter::RegisterData::RegisterData()
    : request_id(0),
      service_worker_registration_id(0),
      render_frame_id(ChildProcessHost::kInvalidUniqueID) {}

bool PushMessagingMessageFilter::RegisterData::FromDocument() const {
  return render_frame_id != ChildProcessHost::kInvalidUniqueID;
}

PushMessagingMessageFilter::Core::Core(
    const base::WeakPtr<PushMessagingMessageFilter>& io_parent,
    int render_process_id)
    : io_parent_(io_parent),
      render_process_id_(render_process_id),
      weak_factory_ui_to_ui_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* process_host =
      RenderProcessHost::FromID(render_process_id_);  // Can't be null yet.
  is_incognito_ = process_host->GetBrowserContext()->IsOffTheRecord();
}

PushMessagingMessageFilter::Core::~Core() {}

PushMessagingMessageFilter::PushMessagingMessageFilter(
    int render_process_id,
    ServiceWorkerContextWrapper* service_worker_context)
    : BrowserMessageFilter(PushMessagingMsgStart),
      service_worker_context_(service_worker_context),
      weak_factory_io_to_io_(this) {
  // Although this class is used only on the IO thread, it is constructed on UI.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Normally, it would be unsafe to obtain a weak pointer from the UI thread,
  // but it's ok in the constructor since we can't be destroyed before our
  // constructor finishes.
  ui_core_.reset(
      new Core(weak_factory_io_to_io_.GetWeakPtr(), render_process_id));

  PushMessagingService* service = ui_core_->service();
  service_available_ = !!service;

  if (service_available_) {
    default_endpoint_ = service->GetEndpoint(false /* standard_protocol */);
    web_push_protocol_endpoint_ =
        service->GetEndpoint(true /* standard_protocol */);
  }
}

PushMessagingMessageFilter::~PushMessagingMessageFilter() {}

void PushMessagingMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool PushMessagingMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushMessagingMessageFilter, message)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_Subscribe, OnSubscribe)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_Unsubscribe, OnUnsubscribe)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_GetSubscription, OnGetSubscription)
    IPC_MESSAGE_HANDLER(PushMessagingHostMsg_GetPermissionStatus,
                        OnGetPermissionStatus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Subscribe methods on both IO and UI threads, merged in order of use from
// PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::OnSubscribe(
    int render_frame_id,
    int request_id,
    int64_t service_worker_registration_id,
    const PushSubscriptionOptions& options) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mvanouwerkerk): Validate arguments?
  RegisterData data;

  // Will be ChildProcessHost::kInvalidUniqueID in requests from Service Worker.
  data.render_frame_id = render_frame_id;

  data.request_id = request_id;
  data.service_worker_registration_id = service_worker_registration_id;
  data.options = options;

  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          data.service_worker_registration_id);
  if (!service_worker_registration ||
      !service_worker_registration->active_version()) {
    SendSubscriptionError(data, PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER);
    return;
  }
  data.requesting_origin = service_worker_registration->pattern().GetOrigin();

  service_worker_context_->GetRegistrationUserData(
      data.service_worker_registration_id,
      {kPushRegistrationIdServiceWorkerKey},
      base::Bind(&PushMessagingMessageFilter::DidCheckForExistingRegistration,
                 weak_factory_io_to_io_.GetWeakPtr(), data));
}

void PushMessagingMessageFilter::DidCheckForExistingRegistration(
    const RegisterData& data,
    const std::vector<std::string>& push_registration_id,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status == SERVICE_WORKER_OK) {
    // TODO(johnme): Check that stored sender ID equals data.options.sender_info
    // and throw an exception if they don't match.
    DCHECK_EQ(1u, push_registration_id.size());
    auto callback = base::Bind(
        &PushMessagingMessageFilter::DidGetEncryptionKeys,
        weak_factory_io_to_io_.GetWeakPtr(), data, push_registration_id[0]);

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&Core::GetEncryptionInfoOnUI,
                   base::Unretained(ui_core_.get()), data.requesting_origin,
                   data.service_worker_registration_id, callback));
    return;
  }
  // TODO(johnme): The spec allows the register algorithm to reject with an
  // AbortError when accessing storage fails. Perhaps we should do that if
  // service_worker_status != SERVICE_WORKER_ERROR_NOT_FOUND instead of
  // attempting to do a fresh registration?
  // https://w3c.github.io/push-api/#widl-PushRegistrationManager-register-Promise-PushRegistration
  if (!data.options.sender_info.empty()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&Core::RegisterOnUI,
                                       base::Unretained(ui_core_.get()), data));
  } else {
    service_worker_context_->GetRegistrationUserData(
        data.service_worker_registration_id, {kPushSenderIdServiceWorkerKey},
        base::Bind(&PushMessagingMessageFilter::DidGetSenderIdFromStorage,
                   weak_factory_io_to_io_.GetWeakPtr(), data));
  }
}

void PushMessagingMessageFilter::DidGetEncryptionKeys(
    const RegisterData& data,
    const std::string& push_registration_id,
    bool success,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!success) {
    SendSubscriptionError(
        data, PUSH_REGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE);
    return;
  }

  SendSubscriptionSuccess(data, PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE,
                          push_registration_id, p256dh, auth);
}

void PushMessagingMessageFilter::DidGetSenderIdFromStorage(
    const RegisterData& data,
    const std::vector<std::string>& sender_id,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status != SERVICE_WORKER_OK) {
    SendSubscriptionError(data, PUSH_REGISTRATION_STATUS_NO_SENDER_ID);
    return;
  }
  DCHECK_EQ(1u, sender_id.size());
  RegisterData mutated_data = data;
  mutated_data.options.sender_info = sender_id[0];
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::RegisterOnUI, base::Unretained(ui_core_.get()),
                 mutated_data));
}

void PushMessagingMessageFilter::Core::RegisterOnUI(
    const PushMessagingMessageFilter::RegisterData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service = service();
  if (!push_service) {
    if (!is_incognito()) {
      // TODO(johnme): Might be better not to expose the API in this case.
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&PushMessagingMessageFilter::SendSubscriptionError,
                     io_parent_,
                     data, PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE));
    } else {
      // Prevent websites from detecting incognito mode, by emulating what would
      // have happened if we had a PushMessagingService available.
      if (!data.FromDocument() || !data.options.user_visible_only) {
        // Throw a permission denied error under the same circumstances.
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&PushMessagingMessageFilter::SendSubscriptionError,
                       io_parent_, data,
                       PUSH_REGISTRATION_STATUS_INCOGNITO_PERMISSION_DENIED));
      } else {
        RenderFrameHost* render_frame_host =
            RenderFrameHost::FromID(render_process_id_, data.render_frame_id);
        WebContents* web_contents =
            WebContents::FromRenderFrameHost(render_frame_host);
        if (web_contents) {
          web_contents->GetMainFrame()->AddMessageToConsole(
              CONSOLE_MESSAGE_LEVEL_ERROR, kIncognitoPushUnsupportedMessage);
          // Request push messaging permission (which will fail, since
          // notifications aren't supported in incognito), so the website can't
          // detect whether incognito is active.
          web_contents->GetBrowserContext()
              ->GetPermissionManager()
              ->RequestPermission(
                  PermissionType::PUSH_MESSAGING, render_frame_host,
                  data.requesting_origin,
                  base::Bind(&PushMessagingMessageFilter::Core::
                                 DidRequestPermissionInIncognito,
                             weak_factory_ui_to_ui_.GetWeakPtr(), data));
        }
      }
    }
    return;
  }

  if (data.FromDocument()) {
    push_service->SubscribeFromDocument(
        data.requesting_origin, data.service_worker_registration_id,
        render_process_id_, data.render_frame_id, data.options,
        base::Bind(&Core::DidRegister, weak_factory_ui_to_ui_.GetWeakPtr(),
                   data));
  } else {
    push_service->SubscribeFromWorker(
        data.requesting_origin, data.service_worker_registration_id,
        data.options, base::Bind(&Core::DidRegister,
                                 weak_factory_ui_to_ui_.GetWeakPtr(), data));
  }
}

void PushMessagingMessageFilter::Core::DidRequestPermissionInIncognito(
    const RegisterData& data,
    blink::mojom::PermissionStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Notification permission should always be denied in incognito.
  DCHECK_EQ(blink::mojom::PermissionStatus::DENIED, status);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PushMessagingMessageFilter::SendSubscriptionError, io_parent_,
                 data, PUSH_REGISTRATION_STATUS_INCOGNITO_PERMISSION_DENIED));
}

void PushMessagingMessageFilter::Core::DidRegister(
    const RegisterData& data,
    const std::string& push_registration_id,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth,
    PushRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status == PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PushMessagingMessageFilter::PersistRegistrationOnIO,
                   io_parent_, data, push_registration_id, p256dh, auth));
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PushMessagingMessageFilter::SendSubscriptionError,
                   io_parent_, data, status));
  }
}

void PushMessagingMessageFilter::PersistRegistrationOnIO(
    const RegisterData& data,
    const std::string& push_registration_id,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->StoreRegistrationUserData(
      data.service_worker_registration_id, data.requesting_origin,
      {{kPushRegistrationIdServiceWorkerKey, push_registration_id},
       {kPushSenderIdServiceWorkerKey, data.options.sender_info}},
      base::Bind(&PushMessagingMessageFilter::DidPersistRegistrationOnIO,
                 weak_factory_io_to_io_.GetWeakPtr(), data,
                 push_registration_id, p256dh, auth));
}

void PushMessagingMessageFilter::DidPersistRegistrationOnIO(
    const RegisterData& data,
    const std::string& push_registration_id,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status == SERVICE_WORKER_OK) {
    SendSubscriptionSuccess(data,
                            PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE,
                            push_registration_id, p256dh, auth);
  } else {
    // TODO(johnme): Unregister, so PushMessagingServiceImpl can decrease count.
    SendSubscriptionError(data, PUSH_REGISTRATION_STATUS_STORAGE_ERROR);
  }
}

void PushMessagingMessageFilter::SendSubscriptionError(
    const RegisterData& data, PushRegistrationStatus status) {
  // Only called from IO thread, but would be safe to call from UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (data.FromDocument()) {
    Send(new PushMessagingMsg_SubscribeFromDocumentError(
        data.render_frame_id, data.request_id, status));
  } else {
    Send(
        new PushMessagingMsg_SubscribeFromWorkerError(data.request_id, status));
  }
  RecordRegistrationStatus(status);
}

void PushMessagingMessageFilter::SendSubscriptionSuccess(
    const RegisterData& data,
    PushRegistrationStatus status,
    const std::string& push_subscription_id,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  // Only called from IO thread, but would be safe to call from UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!service_available_) {
    // This shouldn't be possible in incognito mode, since we've already checked
    // that we have an existing registration. Hence it's ok to throw an error.
    DCHECK(!ui_core_->is_incognito());
    SendSubscriptionError(data, PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE);
    return;
  }

  const GURL endpoint = CreateEndpoint(
      IsApplicationServerKey(data.options.sender_info), push_subscription_id);

  if (data.FromDocument()) {
    Send(new PushMessagingMsg_SubscribeFromDocumentSuccess(
        data.render_frame_id, data.request_id, endpoint, p256dh, auth));
  } else {
    Send(new PushMessagingMsg_SubscribeFromWorkerSuccess(
        data.request_id, endpoint, p256dh, auth));
  }
  RecordRegistrationStatus(status);
}

// Unsubscribe methods on both IO and UI threads, merged in order of use from
// PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::OnUnsubscribe(
    int request_id, int64_t service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration) {
    DidUnregister(request_id, PUSH_UNREGISTRATION_STATUS_NO_SERVICE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id,
      {kPushRegistrationIdServiceWorkerKey, kPushSenderIdServiceWorkerKey},
      base::Bind(&PushMessagingMessageFilter::UnsubscribeHavingGottenIds,
                 weak_factory_io_to_io_.GetWeakPtr(), request_id,
                 service_worker_registration_id,
                 service_worker_registration->pattern().GetOrigin()));
}

void PushMessagingMessageFilter::UnsubscribeHavingGottenIds(
    int request_id,
    int64_t service_worker_registration_id,
    const GURL& requesting_origin,
    const std::vector<std::string>& push_subscription_and_sender_ids,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Note that the subscription ID (push_subscription_and_sender_ids[0]) is
  // unused - we just needed to check if the database contained one.

  switch (service_worker_status) {
    case SERVICE_WORKER_OK:
      DCHECK_EQ(2u, push_subscription_and_sender_ids.size());
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&Core::UnregisterFromService,
                     base::Unretained(ui_core_.get()), request_id,
                     service_worker_registration_id, requesting_origin,
                     push_subscription_and_sender_ids[1]));
      break;
    case SERVICE_WORKER_ERROR_NOT_FOUND:
      // We did not find a registration, stop here and notify the renderer that
      // it was a success even though we did not unregister.
      DidUnregister(request_id,
                    PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED);
      break;
    case SERVICE_WORKER_ERROR_FAILED:
      DidUnregister(request_id, PUSH_UNREGISTRATION_STATUS_STORAGE_ERROR);
      break;
    case SERVICE_WORKER_ERROR_ABORT:
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_TIMEOUT:
    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      NOTREACHED() << "Got unexpected error code: " << service_worker_status
                   << " " << ServiceWorkerStatusToString(service_worker_status);
      DidUnregister(request_id, PUSH_UNREGISTRATION_STATUS_STORAGE_ERROR);
      break;
  }
}

void PushMessagingMessageFilter::Core::UnregisterFromService(
    int request_id,
    int64_t service_worker_registration_id,
    const GURL& requesting_origin,
    const std::string& sender_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service = service();
  if (!push_service) {
    // This shouldn't be possible in incognito mode, since we've already checked
    // that we have an existing registration. Hence it's ok to throw an error.
    DCHECK(!is_incognito());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PushMessagingMessageFilter::DidUnregister, io_parent_,
                   request_id,
                   PUSH_UNREGISTRATION_STATUS_SERVICE_NOT_AVAILABLE));
    return;
  }

  push_service->Unsubscribe(
      requesting_origin, service_worker_registration_id, sender_id,
      base::Bind(&Core::DidUnregisterFromService,
                 weak_factory_ui_to_ui_.GetWeakPtr(), request_id,
                 service_worker_registration_id));
}

void PushMessagingMessageFilter::Core::DidUnregisterFromService(
    int request_id,
    int64_t service_worker_registration_id,
    PushUnregistrationStatus unregistration_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  switch (unregistration_status) {
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED:
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED:
    case PUSH_UNREGISTRATION_STATUS_PENDING_NETWORK_ERROR:
    case PUSH_UNREGISTRATION_STATUS_PENDING_SERVICE_ERROR:
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&PushMessagingMessageFilter::ClearRegistrationData,
                     io_parent_, request_id, service_worker_registration_id,
                     unregistration_status));
      break;
    case PUSH_UNREGISTRATION_STATUS_NO_SERVICE_WORKER:
    case PUSH_UNREGISTRATION_STATUS_SERVICE_NOT_AVAILABLE:
    case PUSH_UNREGISTRATION_STATUS_STORAGE_ERROR:
    case PUSH_UNREGISTRATION_STATUS_NETWORK_ERROR:
      NOTREACHED();
      break;
  }
}

void PushMessagingMessageFilter::ClearRegistrationData(
    int request_id,
    int64_t service_worker_registration_id,
    PushUnregistrationStatus unregistration_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->ClearRegistrationUserData(
      service_worker_registration_id, {kPushRegistrationIdServiceWorkerKey},
      base::Bind(&PushMessagingMessageFilter::DidClearRegistrationData,
                 weak_factory_io_to_io_.GetWeakPtr(), request_id,
                 unregistration_status));
}

void PushMessagingMessageFilter::DidClearRegistrationData(
    int request_id,
    PushUnregistrationStatus unregistration_status,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (service_worker_status != SERVICE_WORKER_OK &&
      service_worker_status != SERVICE_WORKER_ERROR_NOT_FOUND) {
    unregistration_status = PUSH_UNREGISTRATION_STATUS_STORAGE_ERROR;
    DLOG(WARNING) << "Got unexpected error code: " << service_worker_status
                  << " " << ServiceWorkerStatusToString(service_worker_status);
  }

  DidUnregister(request_id, unregistration_status);
}

void PushMessagingMessageFilter::DidUnregister(
    int request_id,
    PushUnregistrationStatus unregistration_status) {
  // Only called from IO thread, but would be safe to call from UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (unregistration_status) {
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED:
    case PUSH_UNREGISTRATION_STATUS_PENDING_NETWORK_ERROR:
    case PUSH_UNREGISTRATION_STATUS_PENDING_SERVICE_ERROR:
      Send(new PushMessagingMsg_UnsubscribeSuccess(request_id, true));
      break;
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED:
      Send(new PushMessagingMsg_UnsubscribeSuccess(request_id, false));
      break;
    case PUSH_UNREGISTRATION_STATUS_NO_SERVICE_WORKER:
    case PUSH_UNREGISTRATION_STATUS_SERVICE_NOT_AVAILABLE:
    case PUSH_UNREGISTRATION_STATUS_STORAGE_ERROR:
      Send(new PushMessagingMsg_UnsubscribeError(
          request_id, blink::WebPushError::ErrorTypeAbort,
          PushUnregistrationStatusToString(unregistration_status)));
      break;
    case PUSH_UNREGISTRATION_STATUS_NETWORK_ERROR:
      NOTREACHED();
      break;
  }
  RecordUnregistrationStatus(unregistration_status);
}

// GetSubscription methods on both IO and UI threads, merged in order of use
// from PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::OnGetSubscription(
    int request_id,
    int64_t service_worker_registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(johnme): Validate arguments?
  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id, {kPushSenderIdServiceWorkerKey},
      base::Bind(&PushMessagingMessageFilter::DidGetSenderInfo,
                 weak_factory_io_to_io_.GetWeakPtr(), request_id,
                 service_worker_registration_id));
}

void PushMessagingMessageFilter::DidGetSenderInfo(
    int request_id,
    int64_t service_worker_registration_id,
    const std::vector<std::string>& sender_info,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK || sender_info.size() != 1) {
    DidGetSubscription(request_id, service_worker_registration_id,
                       false /* uses_standard_protocol */,
                       std::vector<std::string>() /* push_subscription_id */,
                       status);
    return;
  }

  const bool uses_standard_protocol = IsApplicationServerKey(sender_info[0]);
  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id, {kPushRegistrationIdServiceWorkerKey},
      base::Bind(&PushMessagingMessageFilter::DidGetSubscription,
                 weak_factory_io_to_io_.GetWeakPtr(), request_id,
                 service_worker_registration_id, uses_standard_protocol));
}

void PushMessagingMessageFilter::DidGetSubscription(
    int request_id,
    int64_t service_worker_registration_id,
    bool uses_standard_protocol,
    const std::vector<std::string>& push_subscription_id,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PushGetRegistrationStatus get_status =
      PUSH_GETREGISTRATION_STATUS_STORAGE_ERROR;
  switch (service_worker_status) {
    case SERVICE_WORKER_OK: {
      if (!service_available_) {
        // Return not found in incognito mode, so websites can't detect it.
        get_status =
            ui_core_->is_incognito()
                ? PUSH_GETREGISTRATION_STATUS_INCOGNITO_REGISTRATION_NOT_FOUND
                : PUSH_GETREGISTRATION_STATUS_SERVICE_NOT_AVAILABLE;
        break;
      }

      ServiceWorkerRegistration* registration =
          service_worker_context_->GetLiveRegistration(
              service_worker_registration_id);

      const GURL origin = registration->pattern().GetOrigin();
      DCHECK_EQ(1u, push_subscription_id.size());
      const GURL endpoint =
          CreateEndpoint(uses_standard_protocol, push_subscription_id[0]);

      auto callback =
          base::Bind(&PushMessagingMessageFilter::DidGetSubscriptionKeys,
                     weak_factory_io_to_io_.GetWeakPtr(), request_id, endpoint);

      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&Core::GetEncryptionInfoOnUI,
                     base::Unretained(ui_core_.get()), origin,
                     service_worker_registration_id, callback));

      return;
    }
    case SERVICE_WORKER_ERROR_NOT_FOUND: {
      get_status = PUSH_GETREGISTRATION_STATUS_REGISTRATION_NOT_FOUND;
      break;
    }
    case SERVICE_WORKER_ERROR_FAILED: {
      get_status = PUSH_GETREGISTRATION_STATUS_STORAGE_ERROR;
      break;
    }
    case SERVICE_WORKER_ERROR_ABORT:
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_TIMEOUT:
    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
    case SERVICE_WORKER_ERROR_MAX_VALUE: {
      NOTREACHED() << "Got unexpected error code: " << service_worker_status
                   << " " << ServiceWorkerStatusToString(service_worker_status);
      get_status = PUSH_GETREGISTRATION_STATUS_STORAGE_ERROR;
      break;
    }
  }
  Send(new PushMessagingMsg_GetSubscriptionError(request_id, get_status));
  RecordGetRegistrationStatus(get_status);
}

void PushMessagingMessageFilter::DidGetSubscriptionKeys(
    int request_id,
    const GURL& endpoint,
    bool success,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!success) {
    PushGetRegistrationStatus status =
        PUSH_GETREGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE;

    Send(new PushMessagingMsg_GetSubscriptionError(request_id, status));

    RecordGetRegistrationStatus(status);
    return;
  }

  Send(new PushMessagingMsg_GetSubscriptionSuccess(request_id, endpoint,
                                                   p256dh, auth));

  RecordGetRegistrationStatus(PUSH_GETREGISTRATION_STATUS_SUCCESS);
}

// GetPermission methods on both IO and UI threads, merged in order of use from
// PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::OnGetPermissionStatus(
    int request_id,
    int64_t service_worker_registration_id,
    bool user_visible) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerRegistration* service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration) {
    Send(new PushMessagingMsg_GetPermissionStatusError(
        request_id, blink::WebPushError::ErrorTypeAbort));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::GetPermissionStatusOnUI,
                 base::Unretained(ui_core_.get()),
                 service_worker_registration->pattern().GetOrigin(),
                 user_visible, request_id));
}

void PushMessagingMessageFilter::Core::GetPermissionStatusOnUI(
    const GURL& requesting_origin, bool user_visible, int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::WebPushPermissionStatus permission_status;
  PushMessagingService* push_service = service();
  if (push_service) {
    if (!user_visible && !push_service->SupportNonVisibleMessages()) {
      Send(new PushMessagingMsg_GetPermissionStatusError(
          request_id, blink::WebPushError::ErrorTypeNotSupported));
      return;
    }
    permission_status =
        push_service->GetPermissionStatus(requesting_origin, user_visible);
  } else if (is_incognito()) {
    // Return prompt, so the website can't detect incognito mode.
    permission_status = blink::WebPushPermissionStatusPrompt;
  } else {
    Send(new PushMessagingMsg_GetPermissionStatusError(
        request_id, blink::WebPushError::ErrorTypeAbort));
    return;
  }
  Send(new PushMessagingMsg_GetPermissionStatusSuccess(request_id,
                                                       permission_status));
}

// Helper methods on both IO and UI threads, merged from
// PushMessagingMessageFilter and Core.
// -----------------------------------------------------------------------------

void PushMessagingMessageFilter::Core::GetEncryptionInfoOnUI(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const PushMessagingService::EncryptionInfoCallback& io_thread_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service = service();
  if (push_service) {
    push_service->GetEncryptionInfo(
        origin, service_worker_registration_id,
        base::Bind(&ForwardEncryptionInfoToIOThreadProxy, io_thread_callback));
    return;
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(io_thread_callback, false /* success */,
                                     std::vector<uint8_t>() /* p256dh */,
                                     std::vector<uint8_t>() /* auth */));
}

void PushMessagingMessageFilter::Core::Send(IPC::Message* message) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PushMessagingMessageFilter::SendIPC, io_parent_,
                 base::Passed(base::WrapUnique(message))));
}

void PushMessagingMessageFilter::SendIPC(
    std::unique_ptr<IPC::Message> message) {
  Send(message.release());
}

GURL PushMessagingMessageFilter::CreateEndpoint(
    bool standard_protocol,
    const std::string& subscription_id) const {
  const GURL& base =
      standard_protocol ? web_push_protocol_endpoint_ : default_endpoint_;

  return GURL(base.spec() + subscription_id);
}

PushMessagingService* PushMessagingMessageFilter::Core::service() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* process_host =
      RenderProcessHost::FromID(render_process_id_);
  return process_host
             ? process_host->GetBrowserContext()->GetPushMessagingService()
             : nullptr;
}

}  // namespace content
