// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_stream.h"

#include "base/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_reliable_client_stream.h"
#include "net/quic/quic_utils.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/ssl/ssl_info.h"

namespace net {

static const size_t kHeaderBufInitialSize = 4096;

QuicHttpStream::QuicHttpStream(const base::WeakPtr<QuicClientSession>& session)
    : next_state_(STATE_NONE),
      session_(session),
      session_error_(OK),
      was_handshake_confirmed_(session->IsCryptoHandshakeConfirmed()),
      stream_(NULL),
      request_info_(NULL),
      request_body_stream_(NULL),
      priority_(MINIMUM_PRIORITY),
      response_info_(NULL),
      response_status_(OK),
      response_headers_received_(false),
      read_buf_(new GrowableIOBuffer()),
      closed_stream_received_bytes_(0),
      user_buffer_len_(0),
      weak_factory_(this) {
  DCHECK(session_);
  session_->AddObserver(this);
}

QuicHttpStream::~QuicHttpStream() {
  Close(false);
  if (session_)
    session_->RemoveObserver(this);
}

int QuicHttpStream::InitializeStream(const HttpRequestInfo* request_info,
                                     RequestPriority priority,
                                     const BoundNetLog& stream_net_log,
                                     const CompletionCallback& callback) {
  DCHECK(!stream_);
  if (!session_)
    return was_handshake_confirmed_ ? ERR_CONNECTION_CLOSED :
        ERR_QUIC_HANDSHAKE_FAILED;

  if (request_info->url.SchemeIsSecure()) {
    SSLInfo ssl_info;
    bool secure_session = session_->GetSSLInfo(&ssl_info) && ssl_info.cert;
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.SecureResourceSecureSession",
                          secure_session);
    if (!secure_session)
      return ERR_REQUEST_FOR_SECURE_RESOURCE_OVER_INSECURE_QUIC;
  }

  stream_net_log_ = stream_net_log;
  request_info_ = request_info;
  request_time_ = base::Time::Now();
  priority_ = priority;

  int rv = stream_request_.StartRequest(
      session_, &stream_, base::Bind(&QuicHttpStream::OnStreamReady,
                                     weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING) {
    callback_ = callback;
  } else if (rv == OK) {
    stream_->SetDelegate(this);
  } else if (!was_handshake_confirmed_) {
    rv = ERR_QUIC_HANDSHAKE_FAILED;
  }

  return rv;
}

void QuicHttpStream::OnStreamReady(int rv) {
  DCHECK(rv == OK || !stream_);
  if (rv == OK) {
    stream_->SetDelegate(this);
  } else if (!was_handshake_confirmed_) {
    rv = ERR_QUIC_HANDSHAKE_FAILED;
  }

  ResetAndReturn(&callback_).Run(rv);
}

int QuicHttpStream::SendRequest(const HttpRequestHeaders& request_headers,
                                HttpResponseInfo* response,
                                const CompletionCallback& callback) {
  CHECK(stream_);
  CHECK(!request_body_stream_);
  CHECK(!response_info_);
  CHECK(!callback.is_null());
  CHECK(response);

  QuicPriority priority = ConvertRequestPriorityToQuicPriority(priority_);
  stream_->set_priority(priority);
  // Store the serialized request headers.
  CreateSpdyHeadersFromHttpRequest(*request_info_, request_headers,
                                   &request_headers_, SPDY3, /*direct=*/true);

  // Store the request body.
  request_body_stream_ = request_info_->upload_data_stream;
  if (request_body_stream_) {
    // TODO(rch): Can we be more precise about when to allocate
    // raw_request_body_buf_. Removed the following check. DoReadRequestBody()
    // was being called even if we didn't yet allocate raw_request_body_buf_.
    //   && (request_body_stream_->size() ||
    //       request_body_stream_->is_chunked()))
    // Use 10 packets as the body buffer size to give enough space to
    // help ensure we don't often send out partial packets.
    raw_request_body_buf_ = new IOBufferWithSize(10 * kMaxPacketSize);
    // The request body buffer is empty at first.
    request_body_buf_ = new DrainableIOBuffer(raw_request_body_buf_.get(), 0);
  }

  // Store the response info.
  response_info_ = response;

  next_state_ = STATE_SEND_HEADERS;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv > 0 ? OK : rv;
}

UploadProgress QuicHttpStream::GetUploadProgress() const {
  if (!request_body_stream_)
    return UploadProgress();

  return UploadProgress(request_body_stream_->position(),
                        request_body_stream_->size());
}

int QuicHttpStream::ReadResponseHeaders(const CompletionCallback& callback) {
  CHECK(!callback.is_null());

  if (stream_ == NULL)
    return response_status_;

  // Check if we already have the response headers. If so, return synchronously.
  if (response_headers_received_)
    return OK;

  // Still waiting for the response, return IO_PENDING.
  CHECK(callback_.is_null());
  callback_ = callback;
  return ERR_IO_PENDING;
}

int QuicHttpStream::ReadResponseBody(
    IOBuffer* buf, int buf_len, const CompletionCallback& callback) {
  CHECK(buf);
  CHECK(buf_len);
  CHECK(!callback.is_null());

  // If we have data buffered, complete the IO immediately.
  if (!response_body_.empty()) {
    int bytes_read = 0;
    while (!response_body_.empty() && buf_len > 0) {
      scoped_refptr<IOBufferWithSize> data = response_body_.front();
      const int bytes_to_copy = std::min(buf_len, data->size());
      memcpy(&(buf->data()[bytes_read]), data->data(), bytes_to_copy);
      buf_len -= bytes_to_copy;
      if (bytes_to_copy == data->size()) {
        response_body_.pop_front();
      } else {
        const int bytes_remaining = data->size() - bytes_to_copy;
        IOBufferWithSize* new_buffer = new IOBufferWithSize(bytes_remaining);
        memcpy(new_buffer->data(), &(data->data()[bytes_to_copy]),
               bytes_remaining);
        response_body_.pop_front();
        response_body_.push_front(make_scoped_refptr(new_buffer));
      }
      bytes_read += bytes_to_copy;
    }
    return bytes_read;
  }

  if (!stream_) {
    // If the stream is already closed, there is no body to read.
    return response_status_;
  }

  CHECK(callback_.is_null());
  CHECK(!user_buffer_.get());
  CHECK_EQ(0, user_buffer_len_);

  callback_ = callback;
  user_buffer_ = buf;
  user_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

void QuicHttpStream::Close(bool not_reusable) {
  // Note: the not_reusable flag has no meaning for SPDY streams.
  if (stream_) {
    closed_stream_received_bytes_ = stream_->stream_bytes_read();
    stream_->SetDelegate(NULL);
    stream_->Reset(QUIC_STREAM_CANCELLED);
    stream_ = NULL;
  }
}

HttpStream* QuicHttpStream::RenewStreamForAuth() {
  return NULL;
}

bool QuicHttpStream::IsResponseBodyComplete() const {
  return next_state_ == STATE_OPEN && !stream_;
}

bool QuicHttpStream::CanFindEndOfResponse() const {
  return true;
}

bool QuicHttpStream::IsConnectionReused() const {
  // TODO(rch): do something smarter here.
  return stream_ && stream_->id() > 1;
}

void QuicHttpStream::SetConnectionReused() {
  // QUIC doesn't need an indicator here.
}

bool QuicHttpStream::IsConnectionReusable() const {
  // QUIC streams aren't considered reusable.
  return false;
}

int64 QuicHttpStream::GetTotalReceivedBytes() const {
  if (stream_) {
    return stream_->stream_bytes_read();
  }

  return closed_stream_received_bytes_;
}

bool QuicHttpStream::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  // TODO(mmenke):  Figure out what to do here.
  return true;
}

void QuicHttpStream::GetSSLInfo(SSLInfo* ssl_info) {
  DCHECK(stream_);
  stream_->GetSSLInfo(ssl_info);
}

void QuicHttpStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  DCHECK(stream_);
  NOTIMPLEMENTED();
}

bool QuicHttpStream::IsSpdyHttpStream() const {
  return false;
}

void QuicHttpStream::Drain(HttpNetworkSession* session) {
  Close(false);
  delete this;
}

void QuicHttpStream::SetPriority(RequestPriority priority) {
  priority_ = priority;
}

int QuicHttpStream::OnDataReceived(const char* data, int length) {
  DCHECK_NE(0, length);
  // Are we still reading the response headers.
  if (!response_headers_received_) {
    // Grow the read buffer if necessary.
    if (read_buf_->RemainingCapacity() < length) {
      size_t additional_capacity = length - read_buf_->RemainingCapacity();
      if (additional_capacity < kHeaderBufInitialSize)
        additional_capacity = kHeaderBufInitialSize;
      read_buf_->SetCapacity(read_buf_->capacity() + additional_capacity);
    }
    memcpy(read_buf_->data(), data, length);
    read_buf_->set_offset(read_buf_->offset() + length);
    int rv = ParseResponseHeaders();
    if (rv != ERR_IO_PENDING && !callback_.is_null()) {
      DoCallback(rv);
    }
    return OK;
  }

  if (callback_.is_null()) {
    BufferResponseBody(data, length);
    return OK;
  }

  if (length <= user_buffer_len_) {
    memcpy(user_buffer_->data(), data, length);
  } else {
    memcpy(user_buffer_->data(), data, user_buffer_len_);
    int delta = length - user_buffer_len_;
    BufferResponseBody(data + user_buffer_len_, delta);
    length = user_buffer_len_;
  }

  user_buffer_ = NULL;
  user_buffer_len_ = 0;
  DoCallback(length);
  return OK;
}

void QuicHttpStream::OnClose(QuicErrorCode error) {
  if (error != QUIC_NO_ERROR) {
    response_status_ = was_handshake_confirmed_ ?
        ERR_QUIC_PROTOCOL_ERROR : ERR_QUIC_HANDSHAKE_FAILED;
  } else if (!response_headers_received_) {
    response_status_ = ERR_ABORTED;
  }

  closed_stream_received_bytes_ = stream_->stream_bytes_read();
  stream_ = NULL;
  if (!callback_.is_null())
    DoCallback(response_status_);
}

void QuicHttpStream::OnError(int error) {
  stream_ = NULL;
  response_status_ = was_handshake_confirmed_ ?
      error : ERR_QUIC_HANDSHAKE_FAILED;
  if (!callback_.is_null())
    DoCallback(response_status_);
}

bool QuicHttpStream::HasSendHeadersComplete() {
  return next_state_ > STATE_SEND_HEADERS_COMPLETE;
}

void QuicHttpStream::OnCryptoHandshakeConfirmed() {
  was_handshake_confirmed_ = true;
}

void QuicHttpStream::OnSessionClosed(int error) {
  session_error_ = error;
  session_.reset();
}

void QuicHttpStream::OnIOComplete(int rv) {
  rv = DoLoop(rv);

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    DoCallback(rv);
  }
}

void QuicHttpStream::DoCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(!callback_.is_null());

  // The client callback can do anything, including destroying this class,
  // so any pending callback must be issued after everything else is done.
  base::ResetAndReturn(&callback_).Run(rv);
}

int QuicHttpStream::DoLoop(int rv) {
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_SEND_HEADERS:
        CHECK_EQ(OK, rv);
        rv = DoSendHeaders();
        break;
      case STATE_SEND_HEADERS_COMPLETE:
        rv = DoSendHeadersComplete(rv);
        break;
      case STATE_READ_REQUEST_BODY:
        CHECK_EQ(OK, rv);
        rv = DoReadRequestBody();
        break;
      case STATE_READ_REQUEST_BODY_COMPLETE:
        rv = DoReadRequestBodyComplete(rv);
        break;
      case STATE_SEND_BODY:
        CHECK_EQ(OK, rv);
        rv = DoSendBody();
        break;
      case STATE_SEND_BODY_COMPLETE:
        rv = DoSendBodyComplete(rv);
        break;
      case STATE_OPEN:
        CHECK_EQ(OK, rv);
        break;
      default:
        NOTREACHED() << "next_state_: " << next_state_;
        break;
    }
  } while (next_state_ != STATE_NONE && next_state_ != STATE_OPEN &&
           rv != ERR_IO_PENDING);

  return rv;
}

int QuicHttpStream::DoSendHeaders() {
  if (!stream_)
    return ERR_UNEXPECTED;

  // Log the actual request with the URL Request's net log.
  stream_net_log_.AddEvent(
      NetLog::TYPE_HTTP_TRANSACTION_QUIC_SEND_REQUEST_HEADERS,
      base::Bind(&QuicRequestNetLogCallback, stream_->id(), &request_headers_,
                 priority_));
  // Also log to the QuicSession's net log.
  stream_->net_log().AddEvent(
      NetLog::TYPE_QUIC_HTTP_STREAM_SEND_REQUEST_HEADERS,
      base::Bind(&QuicRequestNetLogCallback, stream_->id(), &request_headers_,
                 priority_));

  bool has_upload_data = request_body_stream_ != NULL;

  next_state_ = STATE_SEND_HEADERS_COMPLETE;
  int rv = stream_->WriteHeaders(request_headers_, !has_upload_data, NULL);
  request_headers_.clear();
  return rv;
}

int QuicHttpStream::DoSendHeadersComplete(int rv) {
  if (rv < 0)
    return rv;

  next_state_ = request_body_stream_ ?
      STATE_READ_REQUEST_BODY : STATE_OPEN;

  return OK;
}

int QuicHttpStream::DoReadRequestBody() {
  next_state_ = STATE_READ_REQUEST_BODY_COMPLETE;
  return request_body_stream_->Read(
      raw_request_body_buf_.get(),
      raw_request_body_buf_->size(),
      base::Bind(&QuicHttpStream::OnIOComplete, weak_factory_.GetWeakPtr()));
}

int QuicHttpStream::DoReadRequestBodyComplete(int rv) {
  // |rv| is the result of read from the request body from the last call to
  // DoSendBody().
  if (rv < 0)
    return rv;

  request_body_buf_ = new DrainableIOBuffer(raw_request_body_buf_.get(), rv);
  if (rv == 0) {  // Reached the end.
    DCHECK(request_body_stream_->IsEOF());
  }

  next_state_ = STATE_SEND_BODY;
  return OK;
}

int QuicHttpStream::DoSendBody() {
  if (!stream_)
    return ERR_UNEXPECTED;

  CHECK(request_body_stream_);
  CHECK(request_body_buf_.get());
  const bool eof = request_body_stream_->IsEOF();
  int len = request_body_buf_->BytesRemaining();
  if (len > 0 || eof) {
    next_state_ = STATE_SEND_BODY_COMPLETE;
    base::StringPiece data(request_body_buf_->data(), len);
    return stream_->WriteStreamData(
        data, eof,
        base::Bind(&QuicHttpStream::OnIOComplete, weak_factory_.GetWeakPtr()));
  }

  next_state_ = STATE_OPEN;
  return OK;
}

int QuicHttpStream::DoSendBodyComplete(int rv) {
  if (rv < 0)
    return rv;

  request_body_buf_->DidConsume(request_body_buf_->BytesRemaining());

  if (!request_body_stream_->IsEOF()) {
    next_state_ = STATE_READ_REQUEST_BODY;
    return OK;
  }

  next_state_ = STATE_OPEN;
  return OK;
}

int QuicHttpStream::ParseResponseHeaders() {
  size_t read_buf_len = static_cast<size_t>(read_buf_->offset());
  SpdyFramer framer(SPDY3);
  SpdyHeaderBlock headers;
  char* data = read_buf_->StartOfBuffer();
  size_t len = framer.ParseHeaderBlockInBuffer(data, read_buf_->offset(),
                                               &headers);

  if (len == 0) {
    return ERR_IO_PENDING;
  }

  // Save the remaining received data.
  size_t delta = read_buf_len - len;
  if (delta > 0) {
    BufferResponseBody(data + len, delta);
  }

  // The URLRequest logs these headers, so only log to the QuicSession's
  // net log.
  stream_->net_log().AddEvent(
      NetLog::TYPE_QUIC_HTTP_STREAM_READ_RESPONSE_HEADERS,
      base::Bind(&SpdyHeaderBlockNetLogCallback, &headers));

  if (!SpdyHeadersToHttpResponse(headers, SPDY3, response_info_)) {
    DLOG(WARNING) << "Invalid headers";
    return ERR_QUIC_PROTOCOL_ERROR;
  }
  // Put the peer's IP address and port into the response.
  IPEndPoint address = stream_->GetPeerAddress();
  response_info_->socket_address = HostPortPair::FromIPEndPoint(address);
  response_info_->connection_info =
      HttpResponseInfo::CONNECTION_INFO_QUIC1_SPDY3;
  response_info_->vary_data
      .Init(*request_info_, *response_info_->headers.get());
  response_info_->was_npn_negotiated = true;
  response_info_->npn_negotiated_protocol = "quic/1+spdy/3";
  response_info_->response_time = base::Time::Now();
  response_info_->request_time = request_time_;
  response_headers_received_ = true;

  return OK;
}

void QuicHttpStream::BufferResponseBody(const char* data, int length) {
  if (length == 0)
    return;
  IOBufferWithSize* io_buffer = new IOBufferWithSize(length);
  memcpy(io_buffer->data(), data, length);
  response_body_.push_back(make_scoped_refptr(io_buffer));
}

}  // namespace net
