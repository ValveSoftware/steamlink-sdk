// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_stream.h"

#include "base/memory/scoped_ptr.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_parser.h"
#include "net/socket/client_socket_handle.h"

namespace net {

HttpBasicStream::HttpBasicStream(ClientSocketHandle* connection,
                                 bool using_proxy)
    : state_(connection, using_proxy) {}

HttpBasicStream::~HttpBasicStream() {}

int HttpBasicStream::InitializeStream(const HttpRequestInfo* request_info,
                                      RequestPriority priority,
                                      const BoundNetLog& net_log,
                                      const CompletionCallback& callback) {
  state_.Initialize(request_info, priority, net_log, callback);
  return OK;
}

int HttpBasicStream::SendRequest(const HttpRequestHeaders& headers,
                                 HttpResponseInfo* response,
                                 const CompletionCallback& callback) {
  DCHECK(parser());
  return parser()->SendRequest(
      state_.GenerateRequestLine(), headers, response, callback);
}

UploadProgress HttpBasicStream::GetUploadProgress() const {
  return parser()->GetUploadProgress();
}

int HttpBasicStream::ReadResponseHeaders(const CompletionCallback& callback) {
  return parser()->ReadResponseHeaders(callback);
}

int HttpBasicStream::ReadResponseBody(IOBuffer* buf,
                                      int buf_len,
                                      const CompletionCallback& callback) {
  return parser()->ReadResponseBody(buf, buf_len, callback);
}

void HttpBasicStream::Close(bool not_reusable) {
  parser()->Close(not_reusable);
}

HttpStream* HttpBasicStream::RenewStreamForAuth() {
  DCHECK(IsResponseBodyComplete());
  DCHECK(!parser()->IsMoreDataBuffered());
  // The HttpStreamParser object still has a pointer to the connection. Just to
  // be extra-sure it doesn't touch the connection again, delete it here rather
  // than leaving it until the destructor is called.
  state_.DeleteParser();
  return new HttpBasicStream(state_.ReleaseConnection().release(),
                             state_.using_proxy());
}

bool HttpBasicStream::IsResponseBodyComplete() const {
  return parser()->IsResponseBodyComplete();
}

bool HttpBasicStream::CanFindEndOfResponse() const {
  return parser()->CanFindEndOfResponse();
}

bool HttpBasicStream::IsConnectionReused() const {
  return parser()->IsConnectionReused();
}

void HttpBasicStream::SetConnectionReused() { parser()->SetConnectionReused(); }

bool HttpBasicStream::IsConnectionReusable() const {
  return parser()->IsConnectionReusable();
}

int64 HttpBasicStream::GetTotalReceivedBytes() const {
  if (parser())
    return parser()->received_bytes();
  return 0;
}

bool HttpBasicStream::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  return state_.connection()->GetLoadTimingInfo(IsConnectionReused(),
                                                load_timing_info);
}

void HttpBasicStream::GetSSLInfo(SSLInfo* ssl_info) {
  parser()->GetSSLInfo(ssl_info);
}

void HttpBasicStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  parser()->GetSSLCertRequestInfo(cert_request_info);
}

bool HttpBasicStream::IsSpdyHttpStream() const { return false; }

void HttpBasicStream::Drain(HttpNetworkSession* session) {
  HttpResponseBodyDrainer* drainer = new HttpResponseBodyDrainer(this);
  drainer->Start(session);
  // |drainer| will delete itself.
}

void HttpBasicStream::SetPriority(RequestPriority priority) {
  // TODO(akalin): Plumb this through to |connection_|.
}

}  // namespace net
