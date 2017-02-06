// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/resource_response.h"

#include "net/http/http_response_headers.h"

namespace content {

scoped_refptr<ResourceResponse> ResourceResponse::DeepCopy() const {
  scoped_refptr<ResourceResponse> new_response(new ResourceResponse);
  new_response->head.request_time = head.request_time;
  new_response->head.response_time = head.response_time;
  if (head.headers.get()) {
    new_response->head.headers =
        new net::HttpResponseHeaders(head.headers->raw_headers());
  }
  new_response->head.mime_type = head.mime_type;
  new_response->head.charset = head.charset;
  new_response->head.security_info = head.security_info;
  new_response->head.has_major_certificate_errors =
      head.has_major_certificate_errors;
  new_response->head.content_length = head.content_length;
  new_response->head.encoded_data_length = head.encoded_data_length;
  new_response->head.appcache_id = head.appcache_id;
  new_response->head.appcache_manifest_url = head.appcache_manifest_url;
  new_response->head.load_timing = head.load_timing;
  if (head.devtools_info.get()) {
    new_response->head.devtools_info = head.devtools_info->DeepCopy();
  }
  new_response->head.download_file_path = head.download_file_path;
  new_response->head.was_fetched_via_spdy = head.was_fetched_via_spdy;
  new_response->head.was_npn_negotiated = head.was_npn_negotiated;
  new_response->head.was_alternate_protocol_available =
      head.was_alternate_protocol_available;
  new_response->head.connection_info = head.connection_info;
  new_response->head.was_fetched_via_proxy = head.was_fetched_via_proxy;
  new_response->head.proxy_server = head.proxy_server;
  new_response->head.npn_negotiated_protocol = head.npn_negotiated_protocol;
  new_response->head.socket_address = head.socket_address;
  new_response->head.was_fetched_via_service_worker =
      head.was_fetched_via_service_worker;
  new_response->head.was_fallback_required_by_service_worker =
      head.was_fallback_required_by_service_worker;
  new_response->head.original_url_via_service_worker =
      head.original_url_via_service_worker;
  new_response->head.response_type_via_service_worker =
      head.response_type_via_service_worker;
  new_response->head.service_worker_start_time =
      head.service_worker_start_time;
  new_response->head.service_worker_ready_time =
      head.service_worker_ready_time;
  new_response->head.is_using_lofi = head.is_using_lofi;
  new_response->head.effective_connection_type = head.effective_connection_type;
  new_response->head.signed_certificate_timestamps =
      head.signed_certificate_timestamps;
  new_response->head.cors_exposed_header_names = head.cors_exposed_header_names;
  return new_response;
}

}  // namespace content
