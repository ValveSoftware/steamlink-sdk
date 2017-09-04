// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/push_messaging_status.h"

#include "base/logging.h"

namespace content {

const char* PushRegistrationStatusToString(PushRegistrationStatus status) {
  switch (status) {
    case PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE:
      return "Registration successful - from push service";

    case PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER:
      return "Registration failed - no Service Worker";

    case PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE:
      return "Registration failed - push service not available";

    case PUSH_REGISTRATION_STATUS_LIMIT_REACHED:
      return "Registration failed - registration limit has been reached";

    case PUSH_REGISTRATION_STATUS_PERMISSION_DENIED:
      return "Registration failed - permission denied";

    case PUSH_REGISTRATION_STATUS_SERVICE_ERROR:
      return "Registration failed - push service error";

    case PUSH_REGISTRATION_STATUS_NO_SENDER_ID:
      return "Registration failed - missing applicationServerKey, and "
             "gcm_sender_id not found in manifest";

    case PUSH_REGISTRATION_STATUS_STORAGE_ERROR:
      return "Registration failed - storage error";

    case PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE:
      return "Registration successful - from cache";

    case PUSH_REGISTRATION_STATUS_NETWORK_ERROR:
      return "Registration failed - could not connect to push server";

    case PUSH_REGISTRATION_STATUS_INCOGNITO_PERMISSION_DENIED:
      // We split this out for UMA, but it must be indistinguishable to JS.
      return PushRegistrationStatusToString(
          PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);

    case PUSH_REGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE:
      return "Registration failed - could not retrieve the public key";

    case PUSH_REGISTRATION_STATUS_MANIFEST_EMPTY_OR_MISSING:
      return "Registration failed - missing applicationServerKey, and manifest "
             "empty or missing";

    case PUSH_REGISTRATION_STATUS_SENDER_ID_MISMATCH:
      return "Registration failed - A subscription with a different "
             "applicationServerKey (or gcm_sender_id) already exists; to "
             "change the applicationServerKey, unsubscribe then resubscribe.";
  }
  NOTREACHED();
  return "";
}

const char* PushUnregistrationStatusToString(PushUnregistrationStatus status) {
  switch (status) {
    case PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED:
      return "Unregistration successful - from push service";

    case PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED:
      return "Unregistration successful - was not registered";

    case PUSH_UNREGISTRATION_STATUS_PENDING_NETWORK_ERROR:
      return "Unregistration pending - a network error occurred, but it will "
             "be retried until it succeeds";

    case PUSH_UNREGISTRATION_STATUS_NO_SERVICE_WORKER:
      return "Unregistration failed - no Service Worker";

    case PUSH_UNREGISTRATION_STATUS_SERVICE_NOT_AVAILABLE:
      return "Unregistration failed - push service not available";

    case PUSH_UNREGISTRATION_STATUS_PENDING_SERVICE_ERROR:
      return "Unregistration pending - a push service error occurred, but it "
             "will be retried until it succeeds";

    case PUSH_UNREGISTRATION_STATUS_STORAGE_ERROR:
      return "Unregistration failed - storage error";

    case PUSH_UNREGISTRATION_STATUS_NETWORK_ERROR:
      return "Unregistration failed - could not connect to push server";
  }
  NOTREACHED();
  return "";
}

}  // namespace content
