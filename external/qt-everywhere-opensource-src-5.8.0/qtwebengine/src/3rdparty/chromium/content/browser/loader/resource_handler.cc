// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_handler.h"

#include "content/browser/loader/resource_request_info_impl.h"

namespace content {

ResourceHandler::ResourceHandler(net::URLRequest* request)
    : controller_(NULL),
      request_(request) {
}

void ResourceHandler::SetController(ResourceController* controller) {
  controller_ = controller;
}

ResourceRequestInfoImpl* ResourceHandler::GetRequestInfo() const {
  return ResourceRequestInfoImpl::ForRequest(request_);
}

int ResourceHandler::GetRequestID() const {
  return GetRequestInfo()->GetRequestID();
}

ResourceMessageFilter* ResourceHandler::GetFilter() const {
  return GetRequestInfo()->filter();
}

}  // namespace content
