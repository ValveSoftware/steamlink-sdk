// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_websocket_stream.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"
#include "url/gurl.h"

namespace net {

SpdyWebSocketStream::SpdyWebSocketStream(
    const base::WeakPtr<SpdySession>& spdy_session, Delegate* delegate)
    : spdy_session_(spdy_session),
      pending_send_data_length_(0),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  DCHECK(spdy_session_.get());
  DCHECK(delegate_);
}

SpdyWebSocketStream::~SpdyWebSocketStream() {
  delegate_ = NULL;
  Close();
}

int SpdyWebSocketStream::InitializeStream(const GURL& url,
                                          RequestPriority request_priority,
                                          const BoundNetLog& net_log) {
  if (!spdy_session_)
    return ERR_SOCKET_NOT_CONNECTED;

  int rv = stream_request_.StartRequest(
      SPDY_BIDIRECTIONAL_STREAM, spdy_session_, url, request_priority, net_log,
      base::Bind(&SpdyWebSocketStream::OnSpdyStreamCreated,
                 weak_ptr_factory_.GetWeakPtr()));

  if (rv == OK) {
    stream_ = stream_request_.ReleaseStream();
    DCHECK(stream_.get());
    stream_->SetDelegate(this);
  }
  return rv;
}

int SpdyWebSocketStream::SendRequest(scoped_ptr<SpdyHeaderBlock> headers) {
  if (!stream_.get()) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  int result = stream_->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND);
  if (result < OK && result != ERR_IO_PENDING)
    Close();
  return result;
}

int SpdyWebSocketStream::SendData(const char* data, int length) {
  if (!stream_.get()) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  DCHECK_GE(length, 0);
  pending_send_data_length_ = static_cast<size_t>(length);
  scoped_refptr<IOBuffer> buf(new IOBuffer(length));
  memcpy(buf->data(), data, length);
  stream_->SendData(buf.get(), length, MORE_DATA_TO_SEND);
  return ERR_IO_PENDING;
}

void SpdyWebSocketStream::Close() {
  if (stream_.get()) {
    stream_->Close();
    DCHECK(!stream_.get());
  }
}

void SpdyWebSocketStream::OnRequestHeadersSent() {
  DCHECK(delegate_);
  delegate_->OnSentSpdyHeaders();
}

SpdyResponseHeadersStatus SpdyWebSocketStream::OnResponseHeadersUpdated(
    const SpdyHeaderBlock& response_headers) {
  DCHECK(delegate_);
  delegate_->OnSpdyResponseHeadersUpdated(response_headers);
  return RESPONSE_HEADERS_ARE_COMPLETE;
}

void SpdyWebSocketStream::OnDataReceived(scoped_ptr<SpdyBuffer> buffer) {
  DCHECK(delegate_);
  delegate_->OnReceivedSpdyData(buffer.Pass());
}

void SpdyWebSocketStream::OnDataSent() {
  DCHECK(delegate_);
  delegate_->OnSentSpdyData(pending_send_data_length_);
  pending_send_data_length_ = 0;
}

void SpdyWebSocketStream::OnClose(int status) {
  stream_.reset();

  // Destruction without Close() call OnClose() with delegate_ being NULL.
  if (!delegate_)
    return;
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  delegate->OnCloseSpdyStream();
}

void SpdyWebSocketStream::OnSpdyStreamCreated(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  if (result == OK) {
    stream_ = stream_request_.ReleaseStream();
    DCHECK(stream_.get());
    stream_->SetDelegate(this);
  }
  DCHECK(delegate_);
  delegate_->OnCreatedSpdyStream(result);
}

}  // namespace net
