// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_client_info.h"
#include "content/public/common/referrer.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerClientType.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponseType.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerState.h"
#include "url/gurl.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

// Indicates the document main thread ID in the child process. This is used for
// messaging between the browser process and the child process.
static const int kDocumentMainThreadId = 0;

// Constants for error messages.
extern const char kServiceWorkerRegisterErrorPrefix[];
extern const char kServiceWorkerUpdateErrorPrefix[];
extern const char kServiceWorkerUnregisterErrorPrefix[];
extern const char kServiceWorkerGetRegistrationErrorPrefix[];
extern const char kServiceWorkerGetRegistrationsErrorPrefix[];
extern const char kFetchScriptError[];

// Constants for invalid identifiers.
static const int kInvalidServiceWorkerHandleId = -1;
static const int kInvalidServiceWorkerRegistrationHandleId = -1;
static const int kInvalidServiceWorkerProviderId = -1;
static const int64_t kInvalidServiceWorkerRegistrationId = -1;
static const int64_t kInvalidServiceWorkerVersionId = -1;
static const int64_t kInvalidServiceWorkerResourceId = -1;
static const int kInvalidEmbeddedWorkerThreadId = -1;

// The HTTP cache is bypassed for Service Worker scripts if the last network
// fetch occurred over 24 hours ago.
static const int kServiceWorkerScriptMaxCacheAgeInHours = 24;

// ServiceWorker provider type.
enum ServiceWorkerProviderType {
  SERVICE_WORKER_PROVIDER_UNKNOWN,

  // For ServiceWorker clients.
  SERVICE_WORKER_PROVIDER_FOR_WINDOW,
  SERVICE_WORKER_PROVIDER_FOR_WORKER,
  SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER,

  // For ServiceWorkers.
  SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,

  SERVICE_WORKER_PROVIDER_TYPE_LAST =
      SERVICE_WORKER_PROVIDER_FOR_CONTROLLER
};

// The enum entries below are written to histograms and thus cannot be deleted
// or reordered.
// New entries must be added immediately before the end.
enum FetchRequestMode {
  FETCH_REQUEST_MODE_SAME_ORIGIN,
  FETCH_REQUEST_MODE_NO_CORS,
  FETCH_REQUEST_MODE_CORS,
  FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT,
  FETCH_REQUEST_MODE_NAVIGATE,
  FETCH_REQUEST_MODE_LAST = FETCH_REQUEST_MODE_NAVIGATE
};

enum FetchCredentialsMode {
  FETCH_CREDENTIALS_MODE_OMIT,
  FETCH_CREDENTIALS_MODE_SAME_ORIGIN,
  FETCH_CREDENTIALS_MODE_INCLUDE,
  FETCH_CREDENTIALS_MODE_PASSWORD,
  FETCH_CREDENTIALS_MODE_LAST = FETCH_CREDENTIALS_MODE_PASSWORD
};

enum class FetchRedirectMode {
  FOLLOW_MODE,
  ERROR_MODE,
  MANUAL_MODE,
  LAST = MANUAL_MODE
};

// Indicates which types of ServiceWorkers should skip handling a request.
enum class SkipServiceWorker {
  // Request can be handled both by a controlling same-origin worker and
  // a cross-origin foreign fetch service worker.
  NONE,
  // Request should not be handled by a same-origin controlling worker,
  // but can be intercepted by a foreign fetch service worker.
  CONTROLLING,
  // Request should skip all possible service workers.
  ALL,
  LAST = ALL
};

// Indicates how the service worker handled a fetch event.
enum ServiceWorkerFetchEventResult {
  // Browser should fallback to native fetch.
  SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
  // Service worker provided a ServiceWorkerResponse.
  SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
  SERVICE_WORKER_FETCH_EVENT_LAST = SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE
};

enum class ServiceWorkerFetchType {
  FETCH,
  FOREIGN_FETCH,
  LAST = FOREIGN_FETCH
};

struct ServiceWorkerCaseInsensitiveCompare {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return base::CompareCaseInsensitiveASCII(lhs, rhs) < 0;
  }
};

using ServiceWorkerHeaderMap =
    std::map<std::string, std::string, ServiceWorkerCaseInsensitiveCompare>;

using ServiceWorkerHeaderList = std::vector<std::string>;

// To dispatch fetch request from browser to child process.
struct CONTENT_EXPORT ServiceWorkerFetchRequest {
  ServiceWorkerFetchRequest();
  ServiceWorkerFetchRequest(const GURL& url,
                            const std::string& method,
                            const ServiceWorkerHeaderMap& headers,
                            const Referrer& referrer,
                            bool is_reload);
  ServiceWorkerFetchRequest(const ServiceWorkerFetchRequest& other);
  ~ServiceWorkerFetchRequest();

  FetchRequestMode mode;
  bool is_main_resource_load;
  RequestContextType request_context_type;
  RequestContextFrameType frame_type;
  GURL url;
  std::string method;
  ServiceWorkerHeaderMap headers;
  std::string blob_uuid;
  uint64_t blob_size;
  Referrer referrer;
  FetchCredentialsMode credentials_mode;
  FetchRedirectMode redirect_mode;
  std::string client_id;
  bool is_reload;
  ServiceWorkerFetchType fetch_type;
};

// Represents a response to a fetch.
struct CONTENT_EXPORT ServiceWorkerResponse {
  ServiceWorkerResponse();
  ServiceWorkerResponse(
      const GURL& url,
      int status_code,
      const std::string& status_text,
      blink::WebServiceWorkerResponseType response_type,
      const ServiceWorkerHeaderMap& headers,
      const std::string& blob_uuid,
      uint64_t blob_size,
      const GURL& stream_url,
      blink::WebServiceWorkerResponseError error,
      base::Time response_time,
      bool is_in_cache_storage,
      const std::string& cache_storage_cache_name,
      const ServiceWorkerHeaderList& cors_exposed_header_names);
  ServiceWorkerResponse(const ServiceWorkerResponse& other);
  ~ServiceWorkerResponse();

  GURL url;
  int status_code;
  std::string status_text;
  blink::WebServiceWorkerResponseType response_type;
  ServiceWorkerHeaderMap headers;
  std::string blob_uuid;
  uint64_t blob_size;
  GURL stream_url;
  blink::WebServiceWorkerResponseError error;
  base::Time response_time;
  bool is_in_cache_storage = false;
  std::string cache_storage_cache_name;
  ServiceWorkerHeaderList cors_exposed_header_names;
};

// Represents initialization info for a WebServiceWorker object.
struct CONTENT_EXPORT ServiceWorkerObjectInfo {
  ServiceWorkerObjectInfo();

  // Returns whether the instance is valid. A valid instance has valid
  // |handle_id| and |version_id|.
  bool IsValid() const;

  int handle_id;
  GURL url;
  blink::WebServiceWorkerState state;
  int64_t version_id;
};

struct CONTENT_EXPORT ServiceWorkerRegistrationObjectInfo {
  ServiceWorkerRegistrationObjectInfo();
  int handle_id;
  GURL scope;
  int64_t registration_id;
};

struct ServiceWorkerVersionAttributes {
  ServiceWorkerObjectInfo installing;
  ServiceWorkerObjectInfo waiting;
  ServiceWorkerObjectInfo active;
};

class ChangedVersionAttributesMask {
 public:
  enum {
    INSTALLING_VERSION = 1 << 0,
    WAITING_VERSION = 1 << 1,
    ACTIVE_VERSION = 1 << 2,
    CONTROLLING_VERSION = 1 << 3,
  };

  ChangedVersionAttributesMask() : changed_(0) {}
  explicit ChangedVersionAttributesMask(int changed) : changed_(changed) {}

  int changed() const { return changed_; }

  void add(int changed_versions) { changed_ |= changed_versions; }
  bool installing_changed() const { return !!(changed_ & INSTALLING_VERSION); }
  bool waiting_changed() const { return !!(changed_ & WAITING_VERSION); }
  bool active_changed() const { return !!(changed_ & ACTIVE_VERSION); }
  bool controller_changed() const { return !!(changed_ & CONTROLLING_VERSION); }

 private:
  int changed_;
};

struct ServiceWorkerClientQueryOptions {
  ServiceWorkerClientQueryOptions();
  blink::WebServiceWorkerClientType client_type;
  bool include_uncontrolled;
};

struct ExtendableMessageEventSource {
  ExtendableMessageEventSource();
  explicit ExtendableMessageEventSource(
      const ServiceWorkerClientInfo& client_info);
  explicit ExtendableMessageEventSource(
      const ServiceWorkerObjectInfo& service_worker_info);

  // Exactly one of these infos should be valid.
  ServiceWorkerClientInfo client_info;
  ServiceWorkerObjectInfo service_worker_info;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
