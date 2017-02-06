// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/layered_resource_handler.h"

#include <utility>

#include "base/logging.h"

namespace content {

LayeredResourceHandler::LayeredResourceHandler(
    net::URLRequest* request,
    std::unique_ptr<ResourceHandler> next_handler)
    : ResourceHandler(request), next_handler_(std::move(next_handler)) {}

LayeredResourceHandler::~LayeredResourceHandler() {
}

void LayeredResourceHandler::SetController(ResourceController* controller) {
  ResourceHandler::SetController(controller);

  // Pass the controller down to the next handler.  This method is intended to
  // be overriden by subclasses of LayeredResourceHandler that need to insert a
  // different ResourceController.

  DCHECK(next_handler_.get());
  next_handler_->SetController(controller);
}

bool LayeredResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  DCHECK(next_handler_.get());
  return next_handler_->OnRequestRedirected(redirect_info, response, defer);
}

bool LayeredResourceHandler::OnResponseStarted(ResourceResponse* response,
                                               bool* defer) {
  DCHECK(next_handler_.get());
  return next_handler_->OnResponseStarted(response, defer);
}

bool LayeredResourceHandler::OnWillStart(const GURL& url,
                                         bool* defer) {
  DCHECK(next_handler_.get());
  return next_handler_->OnWillStart(url, defer);
}

bool LayeredResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                  bool* defer) {
  DCHECK(next_handler_.get());
  return next_handler_->OnBeforeNetworkStart(url, defer);
}

bool LayeredResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                        int* buf_size,
                                        int min_size) {
  DCHECK(next_handler_.get());
  return next_handler_->OnWillRead(buf, buf_size, min_size);
}

bool LayeredResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK(next_handler_.get());
  return next_handler_->OnReadCompleted(bytes_read, defer);
}

void LayeredResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  DCHECK(next_handler_.get());
  next_handler_->OnResponseCompleted(status, security_info, defer);
}

void LayeredResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  DCHECK(next_handler_.get());
  next_handler_->OnDataDownloaded(bytes_downloaded);
}

}  // namespace content
