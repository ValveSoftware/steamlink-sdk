// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PUSH_MESSAGING_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_PUSH_MESSAGING_SERVICE_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/common/push_messaging_status.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class ServiceWorkerContext;
struct PushSubscriptionOptions;

// A push service-agnostic interface that the Push API uses for talking to
// push messaging services like GCM. Must only be used on the UI thread.
class CONTENT_EXPORT PushMessagingService {
 public:
  using RegisterCallback =
      base::Callback<void(const std::string& registration_id,
                          const std::vector<uint8_t>& p256dh,
                          const std::vector<uint8_t>& auth,
                          PushRegistrationStatus status)>;
  using UnregisterCallback = base::Callback<void(PushUnregistrationStatus)>;

  using EncryptionInfoCallback = base::Callback<void(
      bool success,
      const std::vector<uint8_t>& p256dh,
      const std::vector<uint8_t>& auth)>;

  using StringCallback = base::Callback<void(const std::string& data,
                                             bool success,
                                             bool not_found)>;

  virtual ~PushMessagingService() {}

  // Returns the absolute URL to the endpoint of the push service where messages
  // should be posted to. Should return an endpoint compatible with the Web Push
  // Protocol when |standard_protocol| is true.
  virtual GURL GetEndpoint(bool standard_protocol) const = 0;

  // Subscribe the given |options.sender_info| with the push messaging service
  // in a document context. The frame is known and a permission UI may be
  // displayed to the user.
  virtual void SubscribeFromDocument(const GURL& requesting_origin,
                                     int64_t service_worker_registration_id,
                                     int renderer_id,
                                     int render_frame_id,
                                     const PushSubscriptionOptions& options,
                                     const RegisterCallback& callback) = 0;

  // Subscribe the given |options.sender_info| with the push messaging service.
  // The frame is not known so if permission was not previously granted by the
  // user this request should fail.
  virtual void SubscribeFromWorker(const GURL& requesting_origin,
                                   int64_t service_worker_registration_id,
                                   const PushSubscriptionOptions& options,
                                   const RegisterCallback& callback) = 0;

  // Retrieves the encryption information associated with the subscription
  // associated to |origin| and |service_worker_registration_id|.
  virtual void GetEncryptionInfo(const GURL& origin,
                                 int64_t service_worker_registration_id,
                                 const EncryptionInfoCallback& callback) = 0;

  // Unsubscribe the given |sender_id| from the push messaging service. The
  // subscription will be synchronously deactivated locally, and asynchronously
  // sent to the push service, with automatic retry.
  virtual void Unsubscribe(const GURL& requesting_origin,
                           int64_t service_worker_registration_id,
                           const std::string& sender_id,
                           const UnregisterCallback& callback) = 0;

  // Checks the permission status for the |origin|. The |user_visible| boolean
  // indicates whether the permission status only has to cover push messages
  // resulting in visible effects to the user.
  virtual blink::WebPushPermissionStatus GetPermissionStatus(
      const GURL& origin,
      bool user_visible) = 0;

  // Returns whether subscriptions that do not mandate user visible UI upon
  // receiving a push message are supported. Influences permission request and
  // permission check behaviour.
  virtual bool SupportNonVisibleMessages() = 0;

 protected:
  static void GetSenderId(BrowserContext* browser_context,
                          const GURL& origin,
                          int64_t service_worker_registration_id,
                          const StringCallback& callback);

  // Clear the push subscription id stored in the service worker with the given
  // |service_worker_registration_id| for the given |origin|.
  static void ClearPushSubscriptionID(BrowserContext* browser_context,
                                      const GURL& origin,
                                      int64_t service_worker_registration_id,
                                      const base::Closure& callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PUSH_MESSAGING_SERVICE_H_
