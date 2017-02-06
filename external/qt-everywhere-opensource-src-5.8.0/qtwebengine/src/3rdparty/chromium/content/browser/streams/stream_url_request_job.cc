// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_url_request_job.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
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
    : net::URLRangeRequestJob(request, network_delegate),
      stream_(stream),
      headers_set_(false),
      pending_buffer_size_(0),
      total_bytes_read_(0),
      max_range_(0),
      request_failed_(false),
      weak_factory_(this) {
  DCHECK(stream_.get());
  stream_->SetReadObserver(this);
}

StreamURLRequestJob::~StreamURLRequestJob() {
  ClearStream();
}

void StreamURLRequestJob::OnDataAvailable(Stream* stream) {
  // Do nothing if pending_buffer_ is empty, i.e. there's no ReadRawData()
  // operation waiting for IO completion.
  if (!pending_buffer_.get())
    return;

  // pending_buffer_ is set to the IOBuffer instance provided to ReadRawData()
  // by URLRequestJob.

  int result = 0;
  switch (stream_->ReadRawData(pending_buffer_.get(), pending_buffer_size_,
                               &result)) {
    case Stream::STREAM_HAS_DATA:
      DCHECK_GT(result, 0);
      break;
    case Stream::STREAM_COMPLETE:
      // Ensure ReadRawData gives net::OK.
      DCHECK_EQ(net::OK, result);
      break;
    case Stream::STREAM_EMPTY:
      NOTREACHED();
      break;
    case Stream::STREAM_ABORTED:
      // Handle this as connection reset.
      result = net::ERR_CONNECTION_RESET;
      break;
  }

  // Clear the buffers before notifying the read is complete, so that it is
  // safe for the observer to read.
  pending_buffer_ = NULL;
  pending_buffer_size_ = 0;

  if (result > 0)
    total_bytes_read_ += result;
  ReadRawDataComplete(result);
}

// net::URLRequestJob methods.
void StreamURLRequestJob::Start() {
  // Continue asynchronously.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&StreamURLRequestJob::DidStart, weak_factory_.GetWeakPtr()));
}

void StreamURLRequestJob::Kill() {
  net::URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
  ClearStream();
}

int StreamURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  // TODO(ellyjones): This is not right. The old code returned true here, but
  // ReadRawData's old contract was to return true only for synchronous
  // successes, which had the effect of treating all errors as synchronous EOFs.
  // See https://crbug.com/508957
  if (request_failed_)
    return 0;

  DCHECK(buf);
  int to_read = buf_size;
  if (max_range_ && to_read) {
    if (to_read + total_bytes_read_ > max_range_)
      to_read = max_range_ - total_bytes_read_;

    if (to_read == 0)
      return 0;
  }

  int bytes_read = 0;
  switch (stream_->ReadRawData(buf, to_read, &bytes_read)) {
    case Stream::STREAM_HAS_DATA:
    case Stream::STREAM_COMPLETE:
      total_bytes_read_ += bytes_read;
      return bytes_read;
    case Stream::STREAM_EMPTY:
      pending_buffer_ = buf;
      pending_buffer_size_ = to_read;
      return net::ERR_IO_PENDING;
    case Stream::STREAM_ABORTED:
      // Handle this as connection reset.
      return net::ERR_CONNECTION_RESET;
  }
  NOTREACHED();
  return net::ERR_FAILED;
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

void StreamURLRequestJob::DidStart() {
  if (range_parse_result() == net::OK && ranges().size() > 0) {
    // Only one range is supported, and it must start at the first byte.
    if (ranges().size() > 1 || ranges()[0].first_byte_position() != 0) {
      NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
      return;
    }

    max_range_ = ranges()[0].last_byte_position() + 1;
  }

  // This class only supports GET requests.
  if (request()->method() != "GET") {
    NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
    return;
  }

  HeadersCompleted(net::HTTP_OK);
}

void StreamURLRequestJob::NotifyFailure(int error_code) {
  request_failed_ = true;

  // This method can only be called before headers are set.
  DCHECK(!headers_set_);

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
