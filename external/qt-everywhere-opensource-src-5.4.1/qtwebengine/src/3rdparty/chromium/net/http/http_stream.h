// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpStream provides an abstraction for a basic http streams, SPDY, and QUIC.
// The HttpStream subtype is expected to manage the underlying transport
// appropriately.  For example, a basic http stream will return the transport
// socket to the pool for reuse.  SPDY streams on the other hand leave the
// transport socket management to the SpdySession.

#ifndef NET_HTTP_HTTP_STREAM_H_
#define NET_HTTP_HTTP_STREAM_H_

#include "base/basictypes.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/upload_progress.h"
#include "net/http/http_stream_base.h"

namespace net {

class IOBuffer;

class NET_EXPORT_PRIVATE HttpStream : public HttpStreamBase {
 public:
  HttpStream() {}
  virtual ~HttpStream() {}

  // Queries the UploadDataStream for its progress (bytes sent).
  virtual UploadProgress GetUploadProgress() const = 0;

  // Returns a new (not initialized) stream using the same underlying
  // connection and invalidates the old stream - no further methods should be
  // called on the old stream.  The caller should ensure that the response body
  // from the previous request is drained before calling this method.  If the
  // subclass does not support renewing the stream, NULL is returned.
  virtual HttpStream* RenewStreamForAuth() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_H_
