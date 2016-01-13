// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_read_from_cache_job.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_histograms.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace content {

ServiceWorkerReadFromCacheJob::ServiceWorkerReadFromCacheJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    base::WeakPtr<ServiceWorkerContextCore> context,
    int64 response_id)
    : net::URLRequestJob(request, network_delegate),
      context_(context),
      response_id_(response_id),
      has_been_killed_(false),
      weak_factory_(this) {
}

ServiceWorkerReadFromCacheJob::~ServiceWorkerReadFromCacheJob() {
}

void ServiceWorkerReadFromCacheJob::Start() {
  if (!context_) {
    NotifyStartError(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  // Create a response reader and start reading the headers,
  // we'll continue when thats done.
  reader_ = context_->storage()->CreateResponseReader(response_id_);
  http_info_io_buffer_ = new HttpResponseInfoIOBuffer;
  reader_->ReadInfo(
      http_info_io_buffer_,
      base::Bind(&ServiceWorkerReadFromCacheJob::OnReadInfoComplete,
                 weak_factory_.GetWeakPtr()));
  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
}

void ServiceWorkerReadFromCacheJob::Kill() {
  if (has_been_killed_)
    return;
  weak_factory_.InvalidateWeakPtrs();
  has_been_killed_ = true;
  reader_.reset();
  context_.reset();
  http_info_io_buffer_ = NULL;
  http_info_.reset();
  range_response_info_.reset();
  net::URLRequestJob::Kill();
}

net::LoadState ServiceWorkerReadFromCacheJob::GetLoadState() const {
  if (reader_.get() && reader_->IsReadPending())
    return net::LOAD_STATE_READING_RESPONSE;
  return net::LOAD_STATE_IDLE;
}

bool ServiceWorkerReadFromCacheJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

bool ServiceWorkerReadFromCacheJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

void ServiceWorkerReadFromCacheJob::GetResponseInfo(
    net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
}

int ServiceWorkerReadFromCacheJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

void ServiceWorkerReadFromCacheJob::SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) {
  std::string value;
  std::vector<net::HttpByteRange> ranges;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &value) ||
      !net::HttpUtil::ParseRangeHeader(value, &ranges)) {
    return;
  }

  // If multiple ranges are requested, we play dumb and
  // return the entire response with 200 OK.
  if (ranges.size() == 1U)
    range_requested_ = ranges[0];
}

bool ServiceWorkerReadFromCacheJob::ReadRawData(
    net::IOBuffer* buf,
    int buf_size,
    int *bytes_read) {
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!reader_->IsReadPending());
  reader_->ReadData(
      buf, buf_size, base::Bind(&ServiceWorkerReadFromCacheJob::OnReadComplete,
                                weak_factory_.GetWeakPtr()));
  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  return false;
}

const net::HttpResponseInfo* ServiceWorkerReadFromCacheJob::http_info() const {
  if (!http_info_)
    return NULL;
  if (range_response_info_)
    return range_response_info_.get();
  return http_info_.get();
}

void ServiceWorkerReadFromCacheJob::OnReadInfoComplete(int result) {
  scoped_refptr<ServiceWorkerReadFromCacheJob> protect(this);
  if (!http_info_io_buffer_->http_info) {
    DCHECK(result < 0);
    ServiceWorkerHistograms::CountReadResponseResult(
        ServiceWorkerHistograms::READ_HEADERS_ERROR);
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
    return;
  }
  DCHECK(result >= 0);
  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status
  http_info_.reset(http_info_io_buffer_->http_info.release());
  if (is_range_request())
    SetupRangeResponse(http_info_io_buffer_->response_data_size);
  http_info_io_buffer_ = NULL;
  NotifyHeadersComplete();
}

void ServiceWorkerReadFromCacheJob::SetupRangeResponse(int resource_size) {
  DCHECK(is_range_request() && http_info_.get() && reader_.get());
  if (resource_size < 0 || !range_requested_.ComputeBounds(resource_size)) {
    range_requested_ = net::HttpByteRange();
    return;
  }

  DCHECK(range_requested_.IsValid());
  int offset = static_cast<int>(range_requested_.first_byte_position());
  int length = static_cast<int>(range_requested_.last_byte_position() -
                                range_requested_.first_byte_position() + 1);

  // Tell the reader about the range to read.
  reader_->SetReadRange(offset, length);

  // Make a copy of the full response headers and fix them up
  // for the range we'll be returning.
  range_response_info_.reset(new net::HttpResponseInfo(*http_info_));
  net::HttpResponseHeaders* headers = range_response_info_->headers.get();
  headers->UpdateWithNewRange(
      range_requested_, resource_size, true /* replace status line */);
}

void ServiceWorkerReadFromCacheJob::OnReadComplete(int result) {
  ServiceWorkerHistograms::ReadResponseResult check_result;
  if (result == 0) {
    check_result = ServiceWorkerHistograms::READ_OK;
    NotifyDone(net::URLRequestStatus());
  } else if (result < 0) {
    check_result = ServiceWorkerHistograms::READ_DATA_ERROR;
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
  } else {
    check_result = ServiceWorkerHistograms::READ_OK;
    SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status
  }
  ServiceWorkerHistograms::CountReadResponseResult(check_result);
  NotifyReadComplete(result);
}

}  // namespace content
