// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_STREAM_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_STREAM_RESOURCE_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/loader/stream_writer.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

class StreamRegistry;

// Redirect this resource to a stream.
class StreamResourceHandler : public ResourceHandler {
 public:
  // |origin| will be used to construct the URL for the Stream. See
  // WebCore::BlobURL and and WebCore::SecurityOrigin in Blink to understand
  // how origin check is done on resource loading.
  StreamResourceHandler(net::URLRequest* request,
                        StreamRegistry* registry,
                        const GURL& origin);
  ~StreamResourceHandler() override;

  void SetController(ResourceController* controller) override;

  // Not needed, as this event handler ought to be the final resource.
  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* resp,
                           bool* defer) override;

  bool OnResponseStarted(ResourceResponse* resp, bool* defer) override;

  bool OnWillStart(const GURL& url, bool* defer) override;

  bool OnBeforeNetworkStart(const GURL& url, bool* defer) override;

  // Create a new buffer to store received data.
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;

  // A read was completed, forward the data to the Stream.
  bool OnReadCompleted(int bytes_read, bool* defer) override;

  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& sec_info,
                           bool* defer) override;

  void OnDataDownloaded(int bytes_downloaded) override;

  Stream* stream() { return writer_.stream(); }

 private:
  StreamWriter writer_;

  DISALLOW_COPY_AND_ASSIGN(StreamResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_STREAM_RESOURCE_HANDLER_H_
