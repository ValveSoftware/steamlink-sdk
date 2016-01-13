// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_request_details.h"

#include "content/browser/worker_host/worker_service_impl.h"
#include "content/public/browser/resource_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace content {

ResourceRequestDetails::ResourceRequestDetails(const net::URLRequest* request,
                                               int cert_id)
    : url(request->url()),
      original_url(request->original_url()),
      method(request->method()),
      referrer(request->referrer()),
      has_upload(request->has_upload()),
      load_flags(request->load_flags()),
      status(request->status()),
      ssl_cert_id(cert_id),
      ssl_cert_status(request->ssl_info().cert_status),
      socket_address(request->GetSocketAddress()) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  resource_type = info->GetResourceType();
  render_frame_id = info->GetRenderFrameID();
  http_response_code =
      request->response_info().headers.get() ?
          request->response_info().headers.get()->response_code() : -1;

  // If request is from the worker process on behalf of a renderer, use
  // the renderer process id, since it consumes the notification response
  // such as ssl state etc.
  // TODO(atwilson): need to notify all associated renderers in the case
  // of ssl state change (http://crbug.com/25357). For now, just notify
  // the first one (works for dedicated workers and shared workers with
  // a single process).
  int worker_render_frame_id;
  if (!WorkerServiceImpl::GetInstance()->GetRendererForWorker(
          info->GetChildID(), &origin_child_id, &worker_render_frame_id)) {
    origin_child_id = info->GetChildID();
  }
}

ResourceRequestDetails::~ResourceRequestDetails() {}

ResourceRedirectDetails::ResourceRedirectDetails(const net::URLRequest* request,
                                                 int cert_id,
                                                 const GURL& new_url)
    : ResourceRequestDetails(request, cert_id),
      new_url(new_url) {
}

ResourceRedirectDetails::~ResourceRedirectDetails() {}

}  // namespace content
