// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_H_
#define CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_H_

namespace content {

// Push registration success/error codes for internal use & reporting in UMA.
enum PushRegistrationStatus {
  // New successful registration (there was not yet a registration cached in
  // Service Worker storage, so the browser successfully registered with the
  // push service. This is likely to be a new push registration, though it's
  // possible that the push service had its own cache (for example if Chrome's
  // app data was cleared, we might have forgotten about a registration that the
  // push service still stores).
  PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE = 0,

  // Registration failed because there is no Service Worker.
  PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER = 1,

  // Registration failed because the push service is not available.
  PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE = 2,

  // Registration failed because the maximum number of registratons has been
  // reached.
  PUSH_REGISTRATION_STATUS_LIMIT_REACHED = 3,

  // Registration failed because permission was denied.
  PUSH_REGISTRATION_STATUS_PERMISSION_DENIED = 4,

  // Registration failed in the push service implemented by the embedder.
  PUSH_REGISTRATION_STATUS_SERVICE_ERROR = 5,

  // Registration failed because no sender id was provided by the page.
  PUSH_REGISTRATION_STATUS_NO_SENDER_ID = 6,

  // Registration succeeded, but we failed to persist it.
  PUSH_REGISTRATION_STATUS_STORAGE_ERROR = 7,

  // A successful registration was already cached in Service Worker storage.
  PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE = 8,

  // Registration failed due to a network error.
  PUSH_REGISTRATION_STATUS_NETWORK_ERROR = 9,

  // Registration failed because the push service is not available in incognito,
  // but we tell JS that permission was denied to not reveal incognito.
  PUSH_REGISTRATION_STATUS_INCOGNITO_PERMISSION_DENIED = 10,

  // Registration failed because the public key could not be retrieved.
  PUSH_REGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE = 11,

  // Registration failed because the manifest could not be retrieved or was
  // empty.
  PUSH_REGISTRATION_STATUS_MANIFEST_EMPTY_OR_MISSING = 12,

  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync, and
  // update PUSH_REGISTRATION_STATUS_LAST below.

  PUSH_REGISTRATION_STATUS_LAST =
      PUSH_REGISTRATION_STATUS_MANIFEST_EMPTY_OR_MISSING
};

// Push unregistration success/error codes for internal use & reporting in UMA.
enum PushUnregistrationStatus {
  // The unregistration was successful.
  PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED = 0,

  // Unregistration was unnecessary, as the registration was not found.
  PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED = 1,

  // The unregistration did not happen because of a network error, but will be
  // retried until it succeeds.
  PUSH_UNREGISTRATION_STATUS_PENDING_NETWORK_ERROR = 2,

  // Unregistration failed because there is no Service Worker.
  PUSH_UNREGISTRATION_STATUS_NO_SERVICE_WORKER = 3,

  // Unregistration failed because the push service is not available.
  PUSH_UNREGISTRATION_STATUS_SERVICE_NOT_AVAILABLE = 4,

  // Unregistration failed in the push service implemented by the embedder, but
  // will be retried until it succeeds.
  PUSH_UNREGISTRATION_STATUS_PENDING_SERVICE_ERROR = 5,

  // Unregistration succeeded, but we failed to clear Service Worker storage.
  PUSH_UNREGISTRATION_STATUS_STORAGE_ERROR = 6,

  // Unregistration failed due to a network error.
  PUSH_UNREGISTRATION_STATUS_NETWORK_ERROR = 7,

  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync, and
  // update PUSH_UNREGISTRATION_STATUS_LAST below.

  PUSH_UNREGISTRATION_STATUS_LAST = PUSH_UNREGISTRATION_STATUS_NETWORK_ERROR
};

// Push getregistration success/error codes for internal use & reporting in UMA.
enum PushGetRegistrationStatus {
  // Getting the registration was successful.
  PUSH_GETREGISTRATION_STATUS_SUCCESS = 0,

  // Getting the registration failed because the push service is not available.
  PUSH_GETREGISTRATION_STATUS_SERVICE_NOT_AVAILABLE = 1,

  // Getting the registration failed because we failed to read from storage.
  PUSH_GETREGISTRATION_STATUS_STORAGE_ERROR = 2,

  // Getting the registration failed because there is no push registration.
  PUSH_GETREGISTRATION_STATUS_REGISTRATION_NOT_FOUND = 3,

  // Getting the registration failed because the push service isn't available in
  // incognito, but we tell JS registration not found to not reveal incognito.
  PUSH_GETREGISTRATION_STATUS_INCOGNITO_REGISTRATION_NOT_FOUND = 4,

  // Registration failed because the public key could not be retrieved.
  PUSH_GETREGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE = 5,

  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync, and
  // update PUSH_GETREGISTRATION_STATUS_LAST below.

  PUSH_GETREGISTRATION_STATUS_LAST =
      PUSH_GETREGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE
};

// Push message event success/error codes for internal use & reporting in UMA.
enum PushDeliveryStatus {
  // The message was successfully delivered.
  PUSH_DELIVERY_STATUS_SUCCESS = 0,

  // The message could not be delivered because the app id was unknown.
  PUSH_DELIVERY_STATUS_UNKNOWN_APP_ID = 2,

  // The message could not be delivered because origin no longer has permission.
  PUSH_DELIVERY_STATUS_PERMISSION_DENIED = 3,

  // The message could not be delivered because no service worker was found.
  PUSH_DELIVERY_STATUS_NO_SERVICE_WORKER = 4,

  // The message could not be delivered because of a service worker error.
  PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR = 5,

  // The message was delivered, but the Service Worker passed a Promise to
  // event.waitUntil that got rejected.
  PUSH_DELIVERY_STATUS_EVENT_WAITUNTIL_REJECTED = 6,

  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync, and
  // update PUSH_DELIVERY_STATUS_LAST below.

  PUSH_DELIVERY_STATUS_LAST = PUSH_DELIVERY_STATUS_EVENT_WAITUNTIL_REJECTED
};

// Push message user visible tracking for reporting in UMA.
enum PushUserVisibleStatus {
  // A notification was required and one (or more) were shown.
  PUSH_USER_VISIBLE_STATUS_REQUIRED_AND_SHOWN = 0,

  // A notification was not required, but one (or more) were shown anyway.
  PUSH_USER_VISIBLE_STATUS_NOT_REQUIRED_BUT_SHOWN = 1,

  // A notification was not required and none were shown.
  PUSH_USER_VISIBLE_STATUS_NOT_REQUIRED_AND_NOT_SHOWN = 2,

  // A notification was required, but none were shown. Fortunately, the site has
  // been well behaved recently so it was glossed over.
  PUSH_USER_VISIBLE_STATUS_REQUIRED_BUT_NOT_SHOWN_USED_GRACE = 3,

  // A notification was required, but none were shown. Unfortunately, the site
  // has run out of grace, so we had to show the user a generic notification.
  PUSH_USER_VISIBLE_STATUS_REQUIRED_BUT_NOT_SHOWN_GRACE_EXCEEDED = 4,

  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync, and
  // update PUSH_USER_VISIBLE_STATUS_LAST below.

  PUSH_USER_VISIBLE_STATUS_LAST =
      PUSH_USER_VISIBLE_STATUS_REQUIRED_BUT_NOT_SHOWN_GRACE_EXCEEDED
};

const char* PushRegistrationStatusToString(PushRegistrationStatus status);

const char* PushUnregistrationStatusToString(PushUnregistrationStatus status);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_H_
