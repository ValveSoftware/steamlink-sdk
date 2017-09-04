// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_request_details.h"

#include "content/public/browser/resource_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace content {

ResourceRequestDetails::ResourceRequestDetails(const net::URLRequest* request,
                                               bool has_certificate)
    : url(request->url()),
      original_url(request->original_url()),
      method(request->method()),
      referrer(request->referrer()),
      has_upload(request->has_upload()),
      load_flags(request->load_flags()),
      status(request->status()),
      has_certificate(has_certificate),
      ssl_cert_status(request->ssl_info().cert_status),
      socket_address(request->GetSocketAddress()) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  resource_type = info->GetResourceType();
  http_response_code =
      request->response_info().headers.get() ?
          request->response_info().headers.get()->response_code() : -1;
}

ResourceRequestDetails::~ResourceRequestDetails() {}

ResourceRedirectDetails::ResourceRedirectDetails(const net::URLRequest* request,
                                                 bool has_certificate,
                                                 const GURL& new_url)
    : ResourceRequestDetails(request, has_certificate),
      new_url(new_url) {
}

ResourceRedirectDetails::~ResourceRedirectDetails() {}

}  // namespace content
