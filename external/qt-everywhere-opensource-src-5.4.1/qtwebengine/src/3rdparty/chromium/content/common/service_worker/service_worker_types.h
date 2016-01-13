// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerState.h"
#include "url/gurl.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

// Indicates invalid request ID (i.e. the sender does not expect it gets
// response for the message) for messaging between browser process
// and embedded worker.
const static int kInvalidServiceWorkerRequestId = -1;

// Constants for invalid identifiers.
const static int kInvalidServiceWorkerHandleId = -1;
const static int kInvalidServiceWorkerProviderId = -1;
const static int64 kInvalidServiceWorkerRegistrationId = -1;
const static int64 kInvalidServiceWorkerVersionId = -1;
const static int64 kInvalidServiceWorkerResourceId = -1;
const static int64 kInvalidServiceWorkerResponseId = -1;

// To dispatch fetch request from browser to child process.
// TODO(kinuko): This struct will definitely need more fields and
// we'll probably want to have response struct/class too.
struct CONTENT_EXPORT ServiceWorkerFetchRequest {
  ServiceWorkerFetchRequest();
  ServiceWorkerFetchRequest(
      const GURL& url,
      const std::string& method,
      const std::map<std::string, std::string>& headers);
  ~ServiceWorkerFetchRequest();

  GURL url;
  std::string method;
  std::map<std::string, std::string> headers;
};

// Indicates how the service worker handled a fetch event.
enum ServiceWorkerFetchEventResult {
  // Browser should fallback to native fetch.
  SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
  // Service worker provided a ServiceWorkerResponse.
  SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
  SERVICE_WORKER_FETCH_EVENT_LAST = SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE
};

// Represents a response to a fetch.
struct CONTENT_EXPORT ServiceWorkerResponse {
  ServiceWorkerResponse();
  ServiceWorkerResponse(int status_code,
                        const std::string& status_text,
                        const std::map<std::string, std::string>& headers,
                        const std::string& blob_uuid);
  ~ServiceWorkerResponse();

  int status_code;
  std::string status_text;
  std::map<std::string, std::string> headers;
  std::string blob_uuid;
};

// Represents initialization info for a WebServiceWorker object.
struct CONTENT_EXPORT ServiceWorkerObjectInfo {
  ServiceWorkerObjectInfo();
  int handle_id;
  GURL scope;
  GURL url;
  blink::WebServiceWorkerState state;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
