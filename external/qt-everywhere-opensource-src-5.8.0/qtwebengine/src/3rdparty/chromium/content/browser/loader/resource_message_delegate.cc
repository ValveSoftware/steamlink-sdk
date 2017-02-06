// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_message_delegate.h"

#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "net/url_request/url_request.h"

namespace content {

ResourceMessageDelegate::ResourceMessageDelegate(const net::URLRequest* request)
    : id_(ResourceRequestInfoImpl::ForRequest(request)->GetGlobalRequestID()) {
  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  rdh->RegisterResourceMessageDelegate(id_, this);
}

ResourceMessageDelegate::~ResourceMessageDelegate() {
  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  rdh->UnregisterResourceMessageDelegate(id_, this);
}

}  // namespace content
