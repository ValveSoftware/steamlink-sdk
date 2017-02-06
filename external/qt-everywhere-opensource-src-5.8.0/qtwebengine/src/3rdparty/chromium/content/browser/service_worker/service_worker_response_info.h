// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESPONSE_INFO_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESPONSE_INFO_H_

#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponseType.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {

struct ResourceResponseInfo;

class CONTENT_EXPORT ServiceWorkerResponseInfo
    : public base::SupportsUserData::Data {
 public:
  static ServiceWorkerResponseInfo* ForRequest(net::URLRequest* request,
                                               bool create = false);
  static void ResetDataForRequest(net::URLRequest* request);

  ~ServiceWorkerResponseInfo() override;

  void GetExtraResponseInfo(ResourceResponseInfo* response_info) const;
  void OnPrepareToRestart(base::TimeTicks service_worker_start_time,
                          base::TimeTicks service_worker_ready_time);
  void OnStartCompleted(
      bool was_fetched_via_service_worker,
      bool was_fallback_required,
      const GURL& original_url_via_service_worker,
      blink::WebServiceWorkerResponseType response_type_via_service_worker,
      base::TimeTicks service_worker_start_time,
      base::TimeTicks service_worker_ready_time,
      bool response_is_in_cache_storage,
      const std::string& response_cache_storage_cache_name,
      const ServiceWorkerHeaderList& cors_exposed_header_names);
  void ResetData();

  bool was_fetched_via_service_worker() const {
    return was_fetched_via_service_worker_;
  }
  bool was_fallback_required() const { return was_fallback_required_; }
  const GURL& original_url_via_service_worker() const {
    return original_url_via_service_worker_;
  }
  blink::WebServiceWorkerResponseType response_type_via_service_worker() const {
    return response_type_via_service_worker_;
  }
  base::TimeTicks service_worker_start_time() const {
    return service_worker_start_time_;
  }
  base::TimeTicks service_worker_ready_time() const {
    return service_worker_ready_time_;
  }
  bool response_is_in_cache_storage() const {
    return response_is_in_cache_storage_;
  }
  const std::string& response_cache_storage_cache_name() const {
    return response_cache_storage_cache_name_;
  }

 private:
  ServiceWorkerResponseInfo();

  bool was_fetched_via_service_worker_ = false;
  bool was_fallback_required_ = false;
  GURL original_url_via_service_worker_;
  blink::WebServiceWorkerResponseType response_type_via_service_worker_ =
      blink::WebServiceWorkerResponseTypeDefault;
  base::TimeTicks service_worker_start_time_;
  base::TimeTicks service_worker_ready_time_;
  bool response_is_in_cache_storage_ = false;
  std::string response_cache_storage_cache_name_;
  ServiceWorkerHeaderList cors_exposed_header_names_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESPONSE_INFO_H_
