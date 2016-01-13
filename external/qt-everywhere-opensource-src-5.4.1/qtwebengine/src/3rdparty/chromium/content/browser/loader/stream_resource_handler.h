// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_STREAM_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_STREAM_RESOURCE_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/streams/stream_write_observer.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

class StreamRegistry;

// Redirect this resource to a stream.
class StreamResourceHandler : public StreamWriteObserver,
                              public ResourceHandler {
 public:
  // |origin| will be used to construct the URL for the Stream. See
  // WebCore::BlobURL and and WebCore::SecurityOrigin in Blink to understand
  // how origin check is done on resource loading.
  StreamResourceHandler(net::URLRequest* request,
                        StreamRegistry* registry,
                        const GURL& origin);
  virtual ~StreamResourceHandler();

  virtual bool OnUploadProgress(uint64 position, uint64 size) OVERRIDE;

  // Not needed, as this event handler ought to be the final resource.
  virtual bool OnRequestRedirected(const GURL& url,
                                   ResourceResponse* resp,
                                   bool* defer) OVERRIDE;

  virtual bool OnResponseStarted(ResourceResponse* resp,
                                 bool* defer) OVERRIDE;

  virtual bool OnWillStart(const GURL& url, bool* defer) OVERRIDE;

  virtual bool OnBeforeNetworkStart(const GURL& url, bool* defer) OVERRIDE;

  // Create a new buffer to store received data.
  virtual bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                          int* buf_size,
                          int min_size) OVERRIDE;

  // A read was completed, forward the data to the Stream.
  virtual bool OnReadCompleted(int bytes_read, bool* defer) OVERRIDE;

  virtual void OnResponseCompleted(const net::URLRequestStatus& status,
                                   const std::string& sec_info,
                                   bool* defer) OVERRIDE;

  virtual void OnDataDownloaded(int bytes_downloaded) OVERRIDE;

  Stream* stream() { return stream_.get(); }

 private:
  virtual void OnSpaceAvailable(Stream* stream) OVERRIDE;
  virtual void OnClose(Stream* stream) OVERRIDE;

  scoped_refptr<Stream> stream_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  DISALLOW_COPY_AND_ASSIGN(StreamResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_STREAM_RESOURCE_HANDLER_H_
