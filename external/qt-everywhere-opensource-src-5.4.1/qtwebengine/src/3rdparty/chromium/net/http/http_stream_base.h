// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpStreamBase is an interface for reading and writing data to an
// HTTP-like stream that keeps the client agnostic of the actual underlying
// transport layer.  This provides an abstraction for HttpStream and
// WebSocketHandshakeStreamBase.

#ifndef NET_HTTP_HTTP_STREAM_BASE_H_
#define NET_HTTP_HTTP_STREAM_BASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/base/upload_progress.h"

namespace net {

class BoundNetLog;
class HttpNetworkSession;
class HttpRequestHeaders;
struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
struct LoadTimingInfo;
class SSLCertRequestInfo;
class SSLInfo;

class NET_EXPORT_PRIVATE HttpStreamBase {
 public:
  HttpStreamBase() {}
  virtual ~HttpStreamBase() {}

  // Initialize stream.  Must be called before calling SendRequest().
  // |request_info| must outlive the HttpStreamBase.
  // Returns a net error code, possibly ERR_IO_PENDING.
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               RequestPriority priority,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) = 0;

  // Writes the headers and uploads body data to the underlying socket.
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the result will be passed to the callback
  // when available. Returns OK on success.
  //
  // The callback will only be invoked once the first full set of headers have
  // been received, at which point |response| will have been populated with that
  // set of headers, and is safe to read, until/unless ReadResponseHeaders is
  // called.
  //
  // |response| must remain valid until all sets of headers has been read, or
  // the HttpStreamBase is destroyed. There's typically only one set of
  // headers, except in the case of 1xx responses (See ReadResponseHeaders).
  virtual int SendRequest(const HttpRequestHeaders& request_headers,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) = 0;

  // Reads from the underlying socket until the next set of response headers
  // have been completely received.  This may only be called on 1xx responses
  // after SendRequest has completed successfully, to read the next set of
  // headers.
  //
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the result will be passed to the callback when
  // available. Returns OK on success. The response headers are available in
  // the HttpResponseInfo passed in to original call to SendRequest.
  virtual int ReadResponseHeaders(const CompletionCallback& callback) = 0;

  // Reads response body data, up to |buf_len| bytes. |buf_len| should be a
  // reasonable size (<2MB). The number of bytes read is returned, or an
  // error is returned upon failure.  0 indicates that the request has been
  // fully satisfied and there is no more data to read.
  // ERR_CONNECTION_CLOSED is returned when the connection has been closed
  // prematurely.  ERR_IO_PENDING is returned if the operation could not be
  // completed synchronously, in which case the result will be passed to the
  // callback when available. If the operation is not completed immediately,
  // the socket acquires a reference to the provided buffer until the callback
  // is invoked or the socket is destroyed.
  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) = 0;

  // Closes the stream.
  // |not_reusable| indicates if the stream can be used for further requests.
  // In the case of HTTP, where we re-use the byte-stream (e.g. the connection)
  // this means we need to close the connection; in the case of SPDY, where the
  // underlying stream is never reused, it has no effect.
  // TODO(mbelshe): We should figure out how to fold the not_reusable flag
  //                into the stream implementation itself so that the caller
  //                does not need to pass it at all.  We might also be able to
  //                eliminate the SetConnectionReused() below.
  virtual void Close(bool not_reusable) = 0;

  // Indicates if the response body has been completely read.
  virtual bool IsResponseBodyComplete() const = 0;

  // Indicates that the end of the response is detectable. This means that
  // the response headers indicate either chunked encoding or content length.
  // If neither is sent, the server must close the connection for us to detect
  // the end of the response.
  // TODO(rch): Rename this method, so that it is clear why it exists
  // particularly as it applies to QUIC and SPDY for which the end of the
  // response is always findable.
  virtual bool CanFindEndOfResponse() const = 0;

  // A stream exists on top of a connection.  If the connection has been used
  // to successfully exchange data in the past, error handling for the
  // stream is done differently.  This method returns true if the underlying
  // connection is reused or has been connected and idle for some time.
  virtual bool IsConnectionReused() const = 0;
  virtual void SetConnectionReused() = 0;

  // Checks whether the current state of the underlying connection
  // allows it to be reused.
  virtual bool IsConnectionReusable() const = 0;

  // Get the total number of bytes received from network for this stream.
  virtual int64 GetTotalReceivedBytes() const = 0;

  // Populates the connection establishment part of |load_timing_info|, and
  // socket ID.  |load_timing_info| must have all null times when called.
  // Returns false and does nothing if there is no underlying connection, either
  // because one has yet to be assigned to the stream, or because the underlying
  // socket has been closed.
  //
  // In practice, this means that this function will always succeed any time
  // between when the full headers have been received and the stream has been
  // closed.
  virtual bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const = 0;

  // Get the SSLInfo associated with this stream's connection.  This should
  // only be called for streams over SSL sockets, otherwise the behavior is
  // undefined.
  virtual void GetSSLInfo(SSLInfo* ssl_info) = 0;

  // Get the SSLCertRequestInfo associated with this stream's connection.
  // This should only be called for streams over SSL sockets, otherwise the
  // behavior is undefined.
  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) = 0;

  // HACK(willchan): Really, we should move the HttpResponseDrainer logic into
  // the HttpStream implementation. This is just a quick hack.
  virtual bool IsSpdyHttpStream() const = 0;

  // In the case of an HTTP error or redirect, flush the response body (usually
  // a simple error or "this page has moved") so that we can re-use the
  // underlying connection. This stream is responsible for deleting itself when
  // draining is complete.
  virtual void Drain(HttpNetworkSession* session) = 0;

  // Called when the priority of the parent transaction changes.
  virtual void SetPriority(RequestPriority priority) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpStreamBase);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_BASE_H_
