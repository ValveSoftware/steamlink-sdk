// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/proxy_connect_redirect_http_stream.h"

#include <cstddef>

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

ProxyConnectRedirectHttpStream::ProxyConnectRedirectHttpStream(
    LoadTimingInfo* load_timing_info)
    : has_load_timing_info_(load_timing_info != NULL) {
  if (has_load_timing_info_)
    load_timing_info_ = *load_timing_info;
}

ProxyConnectRedirectHttpStream::~ProxyConnectRedirectHttpStream() {}

int ProxyConnectRedirectHttpStream::InitializeStream(
    const HttpRequestInfo* request_info,
    RequestPriority priority,
    const BoundNetLog& net_log,
    const CompletionCallback& callback) {
  NOTREACHED();
  return OK;
}

int ProxyConnectRedirectHttpStream::SendRequest(
    const HttpRequestHeaders& request_headers,
    HttpResponseInfo* response,
    const CompletionCallback& callback) {
  NOTREACHED();
  return OK;
}

int ProxyConnectRedirectHttpStream::ReadResponseHeaders(
    const CompletionCallback& callback) {
  NOTREACHED();
  return OK;
}

int ProxyConnectRedirectHttpStream::ReadResponseBody(
    IOBuffer* buf,
    int buf_len,
    const CompletionCallback& callback) {
  NOTREACHED();
  return OK;
}

void ProxyConnectRedirectHttpStream::Close(bool not_reusable) {}

bool ProxyConnectRedirectHttpStream::IsResponseBodyComplete() const {
  NOTREACHED();
  return true;
}

bool ProxyConnectRedirectHttpStream::CanFindEndOfResponse() const {
  return true;
}

bool ProxyConnectRedirectHttpStream::IsConnectionReused() const {
  NOTREACHED();
  return false;
}

void ProxyConnectRedirectHttpStream::SetConnectionReused() {
  NOTREACHED();
}

bool ProxyConnectRedirectHttpStream::IsConnectionReusable() const {
  NOTREACHED();
  return false;
}

int64 ProxyConnectRedirectHttpStream::GetTotalReceivedBytes() const {
  return 0;
}

bool ProxyConnectRedirectHttpStream::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  if (!has_load_timing_info_)
    return false;

  *load_timing_info = load_timing_info_;
  return true;
}

void ProxyConnectRedirectHttpStream::GetSSLInfo(SSLInfo* ssl_info) {
  NOTREACHED();
}

void ProxyConnectRedirectHttpStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
}

bool ProxyConnectRedirectHttpStream::IsSpdyHttpStream() const {
  NOTREACHED();
  return false;
}

void ProxyConnectRedirectHttpStream::Drain(HttpNetworkSession* session) {
  NOTREACHED();
}

void ProxyConnectRedirectHttpStream::SetPriority(RequestPriority priority) {
  // Nothing to do.
}

UploadProgress ProxyConnectRedirectHttpStream::GetUploadProgress() const {
  NOTREACHED();
  return UploadProgress();
}

HttpStream* ProxyConnectRedirectHttpStream::RenewStreamForAuth() {
  NOTREACHED();
  return NULL;
}

}  // namespace
