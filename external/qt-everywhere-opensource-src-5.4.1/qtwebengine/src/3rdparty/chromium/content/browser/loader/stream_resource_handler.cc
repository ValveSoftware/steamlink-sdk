// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/stream_resource_handler.h"

#include "base/guid.h"
#include "base/logging.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_registry.h"
#include "content/public/browser/resource_controller.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request_status.h"
#include "url/url_constants.h"

namespace content {

StreamResourceHandler::StreamResourceHandler(net::URLRequest* request,
                                             StreamRegistry* registry,
                                             const GURL& origin)
    : ResourceHandler(request),
      read_buffer_(NULL) {
  // TODO(tyoshino): Find a way to share this with the blob URL creation in
  // WebKit.
  GURL url(std::string(url::kBlobScheme) + ":" + origin.spec() +
           base::GenerateGUID());
  stream_ = new Stream(registry, this, url);
}

StreamResourceHandler::~StreamResourceHandler() {
  stream_->RemoveWriteObserver(this);
}

bool StreamResourceHandler::OnUploadProgress(uint64 position,
                                             uint64 size) {
  return true;
}

bool StreamResourceHandler::OnRequestRedirected(const GURL& url,
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
  static const int kReadBufSize = 32768;

  DCHECK(buf && buf_size);
  if (!read_buffer_.get())
    read_buffer_ = new net::IOBuffer(kReadBufSize);
  *buf = read_buffer_.get();
  *buf_size = kReadBufSize;

  return true;
}

bool StreamResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  if (!bytes_read)
    return true;

  // We have more data to read.
  DCHECK(read_buffer_.get());

  // Release the ownership of the buffer, and store a reference
  // to it. A new one will be allocated in OnWillRead().
  scoped_refptr<net::IOBuffer> buffer;
  read_buffer_.swap(buffer);
  stream_->AddData(buffer, bytes_read);

  if (!stream_->can_add_data())
    *defer = true;

  return true;
}

void StreamResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& sec_info,
    bool* defer) {
  stream_->Finalize();
}

void StreamResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

void StreamResourceHandler::OnSpaceAvailable(Stream* stream) {
  controller()->Resume();
}

void StreamResourceHandler::OnClose(Stream* stream) {
  controller()->Cancel();
}

}  // namespace content
