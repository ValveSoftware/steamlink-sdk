// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_response_info.h"

#include "content/public/common/resource_response_info.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

int kUserDataKey;  // Only address is used, value is not important.

}  // namespace

// static
ServiceWorkerResponseInfo* ServiceWorkerResponseInfo::ForRequest(
    net::URLRequest* request,
    bool create) {
  ServiceWorkerResponseInfo* info = static_cast<ServiceWorkerResponseInfo*>(
      request->GetUserData(&kUserDataKey));
  if (!info && create) {
    info = new ServiceWorkerResponseInfo();
    request->SetUserData(&kUserDataKey, info);
  }
  return info;
}

// static
void ServiceWorkerResponseInfo::ResetDataForRequest(net::URLRequest* request) {
  ServiceWorkerResponseInfo* info = ForRequest(request);
  if (info)
    info->ResetData();
}

ServiceWorkerResponseInfo::~ServiceWorkerResponseInfo() {}

void ServiceWorkerResponseInfo::GetExtraResponseInfo(
    ResourceResponseInfo* response_info) const {
  response_info->was_fetched_via_service_worker =
      was_fetched_via_service_worker_;
  response_info->was_fallback_required_by_service_worker =
      was_fallback_required_;
  response_info->original_url_via_service_worker =
      original_url_via_service_worker_;
  response_info->response_type_via_service_worker =
      response_type_via_service_worker_;
  response_info->service_worker_start_time = service_worker_start_time_;
  response_info->service_worker_ready_time = service_worker_ready_time_;
  response_info->is_in_cache_storage = response_is_in_cache_storage_;
  response_info->cache_storage_cache_name = response_cache_storage_cache_name_;
  response_info->cors_exposed_header_names = cors_exposed_header_names_;
}

void ServiceWorkerResponseInfo::OnPrepareToRestart(
    base::TimeTicks service_worker_start_time,
    base::TimeTicks service_worker_ready_time) {
  ResetData();

  // Update times, if not already set by a previous Job.
  if (service_worker_start_time_.is_null()) {
    service_worker_start_time_ = service_worker_start_time;
    service_worker_ready_time_ = service_worker_ready_time;
  }
}

void ServiceWorkerResponseInfo::OnStartCompleted(
    bool was_fetched_via_service_worker,
    bool was_fallback_required,
    const GURL& original_url_via_service_worker,
    blink::WebServiceWorkerResponseType response_type_via_service_worker,
    base::TimeTicks service_worker_start_time,
    base::TimeTicks service_worker_ready_time,
    bool response_is_in_cache_storage,
    const std::string& response_cache_storage_cache_name,
    const ServiceWorkerHeaderList& cors_exposed_header_names) {
  was_fetched_via_service_worker_ = was_fetched_via_service_worker;
  was_fallback_required_ = was_fallback_required;
  original_url_via_service_worker_ = original_url_via_service_worker;
  response_type_via_service_worker_ = response_type_via_service_worker;
  response_is_in_cache_storage_ = response_is_in_cache_storage;
  response_cache_storage_cache_name_ = response_cache_storage_cache_name;
  cors_exposed_header_names_ = cors_exposed_header_names;

  // Update times, if not already set by a previous Job.
  if (service_worker_start_time_.is_null()) {
    service_worker_start_time_ = service_worker_start_time;
    service_worker_ready_time_ = service_worker_ready_time;
  }
}

void ServiceWorkerResponseInfo::ResetData() {
  was_fetched_via_service_worker_ = false;
  was_fallback_required_ = false;
  original_url_via_service_worker_ = GURL();
  response_type_via_service_worker_ =
      blink::WebServiceWorkerResponseTypeDefault;
  response_is_in_cache_storage_ = false;
  response_cache_storage_cache_name_ = std::string();
  cors_exposed_header_names_.clear();
}

ServiceWorkerResponseInfo::ServiceWorkerResponseInfo() {}

}  // namespace content
