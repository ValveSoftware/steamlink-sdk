// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_write_to_cache_job.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_histograms.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

namespace content {

ServiceWorkerWriteToCacheJob::ServiceWorkerWriteToCacheJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* version,
    int64 response_id)
    : net::URLRequestJob(request, network_delegate),
      context_(context),
      url_(request->url()),
      response_id_(response_id),
      version_(version),
      has_been_killed_(false),
      did_notify_started_(false),
      did_notify_finished_(false),
      weak_factory_(this) {
  InitNetRequest();
}

ServiceWorkerWriteToCacheJob::~ServiceWorkerWriteToCacheJob() {
  DCHECK_EQ(did_notify_started_, did_notify_finished_);
}

void ServiceWorkerWriteToCacheJob::Start() {
  if (!context_) {
    NotifyStartError(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }
  version_->script_cache_map()->NotifyStartedCaching(
      url_, response_id_);
  did_notify_started_ = true;
  StartNetRequest();
}

void ServiceWorkerWriteToCacheJob::Kill() {
  if (has_been_killed_)
    return;
  weak_factory_.InvalidateWeakPtrs();
  has_been_killed_ = true;
  net_request_.reset();
  if (did_notify_started_ && !did_notify_finished_) {
    version_->script_cache_map()->NotifyFinishedCaching(
        url_, false);
    did_notify_finished_ = true;
  }
  writer_.reset();
  context_.reset();
  net::URLRequestJob::Kill();
}

net::LoadState ServiceWorkerWriteToCacheJob::GetLoadState() const {
  if (writer_ && writer_->IsWritePending())
    return net::LOAD_STATE_WAITING_FOR_APPCACHE;
  if (net_request_)
    return net_request_->GetLoadState().state;
  return net::LOAD_STATE_IDLE;
}

bool ServiceWorkerWriteToCacheJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

bool ServiceWorkerWriteToCacheJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

void ServiceWorkerWriteToCacheJob::GetResponseInfo(
    net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
}

int ServiceWorkerWriteToCacheJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

void ServiceWorkerWriteToCacheJob::SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) {
  std::string value;
  DCHECK(!headers.GetHeader(net::HttpRequestHeaders::kRange, &value));
  net_request_->SetExtraRequestHeaders(headers);
}

bool ServiceWorkerWriteToCacheJob::ReadRawData(
    net::IOBuffer* buf,
    int buf_size,
    int *bytes_read) {
  net::URLRequestStatus status = ReadNetData(buf, buf_size, bytes_read);
  SetStatus(status);
  if (status.is_io_pending())
    return false;

  // No more data to process, the job is complete.
  io_buffer_ = NULL;
  version_->script_cache_map()->NotifyFinishedCaching(
      url_, status.is_success());
  did_notify_finished_ = true;
  return status.is_success();
}

const net::HttpResponseInfo* ServiceWorkerWriteToCacheJob::http_info() const {
  return http_info_.get();
}

void ServiceWorkerWriteToCacheJob::InitNetRequest() {
  DCHECK(request());
  net_request_ = request()->context()->CreateRequest(
      request()->url(),
      request()->priority(),
      this,
      this->GetCookieStore());
  net_request_->set_first_party_for_cookies(
      request()->first_party_for_cookies());
  net_request_->SetReferrer(request()->referrer());
  net_request_->SetExtraRequestHeaders(request()->extra_request_headers());
}

void ServiceWorkerWriteToCacheJob::StartNetRequest() {
  net_request_->Start();  // We'll continue in OnResponseStarted.
}

net::URLRequestStatus ServiceWorkerWriteToCacheJob::ReadNetData(
    net::IOBuffer* buf,
    int buf_size,
    int *bytes_read) {
  DCHECK_GT(buf_size, 0);
  DCHECK(bytes_read);

  *bytes_read = 0;
  io_buffer_ = buf;
  int net_bytes_read = 0;
  if (!net_request_->Read(buf, buf_size, &net_bytes_read)) {
    if (net_request_->status().is_io_pending())
      return net_request_->status();
    DCHECK(!net_request_->status().is_success());
    return net_request_->status();
  }

  if (net_bytes_read != 0) {
    WriteDataToCache(net_bytes_read);
    DCHECK(GetStatus().is_io_pending());
    return GetStatus();
  }

  DCHECK(net_request_->status().is_success());
  return net_request_->status();
}

void ServiceWorkerWriteToCacheJob::WriteHeadersToCache() {
  if (!context_) {
    AsyncNotifyDoneHelper(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }
  writer_ = context_->storage()->CreateResponseWriter(response_id_);
  info_buffer_ = new HttpResponseInfoIOBuffer(
      new net::HttpResponseInfo(net_request_->response_info()));
  writer_->WriteInfo(
      info_buffer_,
      base::Bind(&ServiceWorkerWriteToCacheJob::OnWriteHeadersComplete,
                 weak_factory_.GetWeakPtr()));
  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
}

void ServiceWorkerWriteToCacheJob::OnWriteHeadersComplete(int result) {
  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status
  if (result < 0) {
    ServiceWorkerHistograms::CountWriteResponseResult(
        ServiceWorkerHistograms::WRITE_HEADERS_ERROR);
    AsyncNotifyDoneHelper(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, result));
    return;
  }
  http_info_.reset(info_buffer_->http_info.release());
  info_buffer_ = NULL;
  NotifyHeadersComplete();
}

void ServiceWorkerWriteToCacheJob::WriteDataToCache(int amount_to_write) {
  DCHECK_NE(0, amount_to_write);
  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  writer_->WriteData(
      io_buffer_, amount_to_write,
      base::Bind(&ServiceWorkerWriteToCacheJob::OnWriteDataComplete,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerWriteToCacheJob::OnWriteDataComplete(int result) {
  DCHECK_NE(0, result);
  io_buffer_ = NULL;
  if (!context_) {
    AsyncNotifyDoneHelper(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }
  if (result < 0) {
    ServiceWorkerHistograms::CountWriteResponseResult(
        ServiceWorkerHistograms::WRITE_DATA_ERROR);
    AsyncNotifyDoneHelper(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, result));
    return;
  }
  ServiceWorkerHistograms::CountWriteResponseResult(
      ServiceWorkerHistograms::WRITE_OK);
  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status
  NotifyReadComplete(result);
}

void ServiceWorkerWriteToCacheJob::OnReceivedRedirect(
    net::URLRequest* request,
    const GURL& new_url,
    bool* defer_redirect) {
  DCHECK_EQ(net_request_, request);
  // Script resources can't redirect.
  AsyncNotifyDoneHelper(net::URLRequestStatus(
      net::URLRequestStatus::FAILED, net::ERR_FAILED));
}

void ServiceWorkerWriteToCacheJob::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  DCHECK_EQ(net_request_, request);
  // TODO(michaeln): Pass this thru to our jobs client.
  AsyncNotifyDoneHelper(net::URLRequestStatus(
      net::URLRequestStatus::FAILED, net::ERR_FAILED));
}

void ServiceWorkerWriteToCacheJob::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK_EQ(net_request_, request);
  // TODO(michaeln): Pass this thru to our jobs client.
  // see NotifyCertificateRequested.
  AsyncNotifyDoneHelper(net::URLRequestStatus(
      net::URLRequestStatus::FAILED, net::ERR_FAILED));
}

void ServiceWorkerWriteToCacheJob:: OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK_EQ(net_request_, request);
  // TODO(michaeln): Pass this thru to our jobs client,
  // see NotifySSLCertificateError.
  AsyncNotifyDoneHelper(net::URLRequestStatus(
      net::URLRequestStatus::FAILED, net::ERR_FAILED));
}

void ServiceWorkerWriteToCacheJob::OnBeforeNetworkStart(
    net::URLRequest* request,
    bool* defer) {
  DCHECK_EQ(net_request_, request);
  NotifyBeforeNetworkStart(defer);
}

void ServiceWorkerWriteToCacheJob::OnResponseStarted(
    net::URLRequest* request) {
  DCHECK_EQ(net_request_, request);
  if (!request->status().is_success()) {
    AsyncNotifyDoneHelper(request->status());
    return;
  }
  if (request->GetResponseCode() / 100 != 2) {
    AsyncNotifyDoneHelper(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, net::ERR_FAILED));
    // TODO(michaeln): Instead of error'ing immediately, send the net
    // response to our consumer, just don't cache it?
    return;
  }
  WriteHeadersToCache();
}

void ServiceWorkerWriteToCacheJob::OnReadCompleted(
    net::URLRequest* request,
    int bytes_read) {
  DCHECK_EQ(net_request_, request);
  if (!request->status().is_success()) {
    AsyncNotifyDoneHelper(request->status());
    return;
  }
  if (bytes_read > 0) {
    WriteDataToCache(bytes_read);
    return;
  }
  // We're done with all.
  AsyncNotifyDoneHelper(request->status());
  return;
}

void ServiceWorkerWriteToCacheJob::AsyncNotifyDoneHelper(
    const net::URLRequestStatus& status) {
  DCHECK(!status.is_io_pending());
  version_->script_cache_map()->NotifyFinishedCaching(
      url_, status.is_success());
  did_notify_finished_ = true;
  SetStatus(status);
  NotifyDone(status);
}

}  // namespace content
