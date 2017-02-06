// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MESSAGE_FILTER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/push_messaging_status.h"
#include "url/gurl.h"

namespace content {

class PushMessagingService;
class ServiceWorkerContextWrapper;
struct PushSubscriptionOptions;

// Documented at definition.
extern const char kPushSenderIdServiceWorkerKey[];
extern const char kPushRegistrationIdServiceWorkerKey[];

class PushMessagingMessageFilter : public BrowserMessageFilter {
 public:
  PushMessagingMessageFilter(
      int render_process_id,
      ServiceWorkerContextWrapper* service_worker_context);

 private:
  struct RegisterData;
  class Core;

  friend class BrowserThread;
  friend class base::DeleteHelper<PushMessagingMessageFilter>;

  ~PushMessagingMessageFilter() override;

  // BrowserMessageFilter implementation.
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Subscribe methods on IO thread --------------------------------------------

  void OnSubscribe(int render_frame_id,
                   int request_id,
                   int64_t service_worker_registration_id,
                   const PushSubscriptionOptions& options);

  void DidCheckForExistingRegistration(
      const RegisterData& data,
      const std::vector<std::string>& push_registration_id,
      ServiceWorkerStatusCode service_worker_status);

  void DidGetEncryptionKeys(const RegisterData& data,
                            const std::string& push_registration_id,
                            bool success,
                            const std::vector<uint8_t>& p256dh,
                            const std::vector<uint8_t>& auth);

  void DidGetSenderIdFromStorage(const RegisterData& data,
                                 const std::vector<std::string>& sender_id,
                                 ServiceWorkerStatusCode service_worker_status);

  // Called via PostTask from UI thread.
  void PersistRegistrationOnIO(const RegisterData& data,
                               const std::string& push_registration_id,
                               const std::vector<uint8_t>& p256dh,
                               const std::vector<uint8_t>& auth);

  void DidPersistRegistrationOnIO(
      const RegisterData& data,
      const std::string& push_registration_id,
      const std::vector<uint8_t>& p256dh,
      const std::vector<uint8_t>& auth,
      ServiceWorkerStatusCode service_worker_status);

  // Called both from IO thread, and via PostTask from UI thread.
  void SendSubscriptionError(const RegisterData& data,
                             PushRegistrationStatus status);
  // Called both from IO thread, and via PostTask from UI thread.
  void SendSubscriptionSuccess(const RegisterData& data,
                               PushRegistrationStatus status,
                               const std::string& push_subscription_id,
                               const std::vector<uint8_t>& p256dh,
                               const std::vector<uint8_t>& auth);

  // Unsubscribe methods on IO thread ------------------------------------------

  void OnUnsubscribe(int request_id, int64_t service_worker_registration_id);

  void UnsubscribeHavingGottenIds(
      int request_id,
      int64_t service_worker_registration_id,
      const GURL& requesting_origin,
      const std::vector<std::string>& push_subscription_and_sender_ids,
      ServiceWorkerStatusCode service_worker_status);

  // Called via PostTask from UI thread.
  void ClearRegistrationData(int request_id,
                             int64_t service_worker_registration_id,
                             PushUnregistrationStatus unregistration_status);

  void DidClearRegistrationData(int request_id,
                                PushUnregistrationStatus unregistration_status,
                                ServiceWorkerStatusCode service_worker_status);

  // Called both from IO thread, and via PostTask from UI thread.
  void DidUnregister(int request_id,
                     PushUnregistrationStatus unregistration_status);

  // GetSubscription methods on IO thread --------------------------------------

  void OnGetSubscription(int request_id,
                         int64_t service_worker_registration_id);

  void DidGetSenderInfo(int request_id,
                        int64_t service_worker_registration_id,
                        const std::vector<std::string>& sender_info,
                        ServiceWorkerStatusCode status);

  void DidGetSubscription(int request_id,
                          int64_t service_worker_registration_id,
                          bool uses_standard_protocol,
                          const std::vector<std::string>& push_subscription_id,
                          ServiceWorkerStatusCode status);

  void DidGetSubscriptionKeys(int request_id,
                              const GURL& endpoint,
                              bool success,
                              const std::vector<uint8_t>& p256dh,
                              const std::vector<uint8_t>& auth);

  // GetPermission methods on IO thread ----------------------------------------

  void OnGetPermissionStatus(int request_id,
                             int64_t service_worker_registration_id,
                             bool user_visible);

  // Helper methods on IO thread -----------------------------------------------

  // Called via PostTask from UI thread.
  void SendIPC(std::unique_ptr<IPC::Message> message);

  // Helper methods on either thread -------------------------------------------

  // Creates an endpoint for |subscription_id| with either the default protocol,
  // or the standardized Web Push Protocol, depending on |standard_protocol|.
  GURL CreateEndpoint(bool standard_protocol,
                      const std::string& subscription_id) const;

  // Inner core of this message filter which lives on the UI thread.
  std::unique_ptr<Core, BrowserThread::DeleteOnUIThread> ui_core_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // Whether the PushMessagingService was available when constructed.
  bool service_available_;

  GURL default_endpoint_;
  GURL web_push_protocol_endpoint_;

  base::WeakPtrFactory<PushMessagingMessageFilter> weak_factory_io_to_io_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_MESSAGE_FILTER_H_
