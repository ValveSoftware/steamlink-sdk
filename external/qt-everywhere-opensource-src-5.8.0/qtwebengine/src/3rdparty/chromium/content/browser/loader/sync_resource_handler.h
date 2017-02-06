// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SYNC_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_SYNC_RESOURCE_HANDLER_H_

#include <stdint.h>

#include <string>

#include "content/browser/loader/resource_handler.h"
#include "content/public/common/resource_response.h"

namespace IPC {
class Message;
}

namespace net {
class IOBuffer;
class URLRequest;
}

namespace content {
class ResourceContext;
class ResourceDispatcherHostImpl;
class ResourceMessageFilter;

// Used to complete a synchronous resource request in response to resource load
// events from the resource dispatcher host.
class SyncResourceHandler : public ResourceHandler {
 public:
  SyncResourceHandler(net::URLRequest* request,
                      IPC::Message* result_message,
                      ResourceDispatcherHostImpl* resource_dispatcher_host);
  ~SyncResourceHandler() override;

  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillStart(const GURL& url, bool* defer) override;
  bool OnBeforeNetworkStart(const GURL& url, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override;
  void OnDataDownloaded(int bytes_downloaded) override;

 private:
  enum { kReadBufSize = 3840 };

  scoped_refptr<net::IOBuffer> read_buffer_;

  SyncLoadResult result_;
  IPC::Message* result_message_;
  ResourceDispatcherHostImpl* rdh_;
  int64_t total_transfer_size_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SYNC_RESOURCE_HANDLER_H_
