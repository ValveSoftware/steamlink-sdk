// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_url_request_job.h"

#include "base/strings/string_number_conversions.h"
#include "content/browser/streams/stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

namespace content {

StreamURLRequestJob::StreamURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    scoped_refptr<Stream> stream)
    : net::URLRequestJob(request, network_delegate),
      weak_factory_(this),
      stream_(stream),
      headers_set_(false),
      pending_buffer_size_(0),
      total_bytes_read_(0),
      max_range_(0),
      request_failed_(false) {
  DCHECK(stream_.get());
  stream_->SetReadObserver(this);
}

StreamURLRequestJob::~StreamURLRequestJob() {
  ClearStream();
}

void StreamURLRequestJob::OnDataAvailable(Stream* stream) {
  // Clear the IO_PENDING status.
  SetStatus(net::URLRequestStatus());
  // Do nothing if pending_buffer_ is empty, i.e. there's no ReadRawData()
  // operation waiting for IO completion.
  if (!pending_buffer_.get())
    return;

  // pending_buffer_ is set to the IOBuffer instance provided to ReadRawData()
  // by URLRequestJob.

  int bytes_read;
  switch (stream_->ReadRawData(
      pending_buffer_.get(), pending_buffer_size_, &bytes_read)) {
    case Stream::STREAM_HAS_DATA:
      DCHECK_GT(bytes_read, 0);
      break;
    case Stream::STREAM_COMPLETE:
      // Ensure this. Calling NotifyReadComplete call with 0 signals
      // completion.
      bytes_read = 0;
      break;
    case Stream::STREAM_EMPTY:
      NOTREACHED();
      break;
    case Stream::STREAM_ABORTED:
      // Handle this as connection reset.
      NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                       net::ERR_CONNECTION_RESET));
      break;
  }

  // Clear the buffers before notifying the read is complete, so that it is
  // safe for the observer to read.
  pending_buffer_ = NULL;
  pending_buffer_size_ = 0;

  total_bytes_read_ += bytes_read;
  NotifyReadComplete(bytes_read);
}

// net::URLRequestJob methods.
void StreamURLRequestJob::Start() {
  // Continue asynchronously.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&StreamURLRequestJob::DidStart, weak_factory_.GetWeakPtr()));
}

void StreamURLRequestJob::Kill() {
  net::URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
  ClearStream();
}

bool StreamURLRequestJob::ReadRawData(net::IOBuffer* buf,
                                      int buf_size,
                                      int* bytes_read) {
  if (request_failed_)
    return true;

  DCHECK(buf);
  DCHECK(bytes_read);
  int to_read = buf_size;
  if (max_range_ && to_read) {
    if (to_read + total_bytes_read_ > max_range_)
      to_read = max_range_ - total_bytes_read_;

    if (to_read <= 0) {
      *bytes_read = 0;
      return true;
    }
  }

  switch (stream_->ReadRawData(buf, to_read, bytes_read)) {
    case Stream::STREAM_HAS_DATA:
    case Stream::STREAM_COMPLETE:
      total_bytes_read_ += *bytes_read;
      return true;
    case Stream::STREAM_EMPTY:
      pending_buffer_ = buf;
      pending_buffer_size_ = to_read;
      SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
      return false;
    case Stream::STREAM_ABORTED:
      // Handle this as connection reset.
      NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                       net::ERR_CONNECTION_RESET));
      return false;
  }
  NOTREACHED();
  return false;
}

bool StreamURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!response_info_)
    return false;

  // TODO(zork): Support registered MIME types if needed.
  return response_info_->headers->GetMimeType(mime_type);
}

void StreamURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (response_info_)
    *info = *response_info_;
}

int StreamURLRequestJob::GetResponseCode() const {
  if (!response_info_)
    return -1;

  return response_info_->headers->response_code();
}

void StreamURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        // Streams don't support seeking, so a non-zero starting position
        // doesn't make sense.
        if (ranges[0].first_byte_position() == 0) {
          max_range_ = ranges[0].last_byte_position() + 1;
        } else {
          NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
          return;
        }
      } else {
        NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
        return;
      }
    }
  }
}

void StreamURLRequestJob::DidStart() {
  // We only support GET request.
  if (request()->method() != "GET") {
    NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
    return;
  }

  HeadersCompleted(net::HTTP_OK);
}

void StreamURLRequestJob::NotifyFailure(int error_code) {
  request_failed_ = true;

  // If we already return the headers on success, we can't change the headers
  // now. Instead, we just error out.
  if (headers_set_) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     error_code));
    return;
  }

  // TODO(zork): Share these with BlobURLRequestJob.
  net::HttpStatusCode status_code = net::HTTP_INTERNAL_SERVER_ERROR;
  switch (error_code) {
    case net::ERR_ACCESS_DENIED:
      status_code = net::HTTP_FORBIDDEN;
      break;
    case net::ERR_FILE_NOT_FOUND:
      status_code = net::HTTP_NOT_FOUND;
      break;
    case net::ERR_METHOD_NOT_SUPPORTED:
      status_code = net::HTTP_METHOD_NOT_ALLOWED;
      break;
    case net::ERR_FAILED:
      break;
    default:
      DCHECK(false);
      break;
  }
  HeadersCompleted(status_code);
}

void StreamURLRequestJob::HeadersCompleted(net::HttpStatusCode status_code) {
  std::string status("HTTP/1.1 ");
  status.append(base::IntToString(status_code));
  status.append(" ");
  status.append(net::GetHttpReasonPhrase(status_code));
  status.append("\0\0", 2);
  net::HttpResponseHeaders* headers = new net::HttpResponseHeaders(status);

  if (status_code == net::HTTP_OK) {
    std::string content_type_header(net::HttpRequestHeaders::kContentType);
    content_type_header.append(": ");
    content_type_header.append("text/plain");
    headers->AddHeader(content_type_header);
  }

  response_info_.reset(new net::HttpResponseInfo());
  response_info_->headers = headers;

  headers_set_ = true;

  NotifyHeadersComplete();
}

void StreamURLRequestJob::ClearStream() {
  if (stream_.get()) {
    stream_->RemoveReadObserver(this);
    stream_ = NULL;
  }
}

}  // namespace content
