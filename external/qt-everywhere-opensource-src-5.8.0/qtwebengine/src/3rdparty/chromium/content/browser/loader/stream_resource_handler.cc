// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/stream_resource_handler.h"

#include "base/logging.h"

namespace content {

StreamResourceHandler::StreamResourceHandler(net::URLRequest* request,
                                             StreamRegistry* registry,
                                             const GURL& origin)
    : ResourceHandler(request) {
  writer_.InitializeStream(registry, origin);
}

StreamResourceHandler::~StreamResourceHandler() {
}

void StreamResourceHandler::SetController(ResourceController* controller) {
  writer_.set_controller(controller);
  ResourceHandler::SetController(controller);
}

bool StreamResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* resp,
    bool* defer) {
  return true;
}

bool StreamResourceHandler::OnResponseStarted(ResourceResponse* resp,
                                              bool* defer) {
  return true;
}

bool StreamResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return true;
}

bool StreamResourceHandler::OnBeforeNetworkStart(const GURL& url, bool* defer) {
  return true;
}

bool StreamResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                       int* buf_size,
                                       int min_size) {
  writer_.OnWillRead(buf, buf_size, min_size);
  return true;
}

bool StreamResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  writer_.OnReadCompleted(bytes_read, defer);
  return true;
}

void StreamResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& sec_info,
    bool* defer) {
  writer_.Finalize();
}

void StreamResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

}  // namespace content
