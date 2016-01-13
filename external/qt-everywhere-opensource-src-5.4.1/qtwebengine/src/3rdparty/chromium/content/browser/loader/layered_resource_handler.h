// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_LAYERED_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_LAYERED_RESOURCE_HANDLER_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/loader/resource_handler.h"
#include "content/common/content_export.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

// A ResourceHandler that simply delegates all calls to a next handler.  This
// class is intended to be subclassed.
class CONTENT_EXPORT LayeredResourceHandler : public ResourceHandler {
 public:
  LayeredResourceHandler(net::URLRequest* request,
                         scoped_ptr<ResourceHandler> next_handler);
  virtual ~LayeredResourceHandler();

  // ResourceHandler implementation:
  virtual void SetController(ResourceController* controller) OVERRIDE;
  virtual bool OnUploadProgress(uint64 position, uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnWillStart(const GURL& url, bool* defer) OVERRIDE;
  virtual bool OnBeforeNetworkStart(const GURL& url, bool* defer) OVERRIDE;
  virtual bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int bytes_read,
                               bool* defer) OVERRIDE;
  virtual void OnResponseCompleted(const net::URLRequestStatus& status,
                                   const std::string& security_info,
                                   bool* defer) OVERRIDE;
  virtual void OnDataDownloaded(int bytes_downloaded) OVERRIDE;

  scoped_ptr<ResourceHandler> next_handler_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_LAYERED_RESOURCE_HANDLER_H_
