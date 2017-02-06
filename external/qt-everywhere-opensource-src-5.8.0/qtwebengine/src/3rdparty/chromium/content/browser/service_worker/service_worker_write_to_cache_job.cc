// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_write_to_cache_job.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_cache_writer.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/net/url_request_service_worker_data.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

namespace content {

namespace {

const char kKilledError[] = "The request to fetch the script was interrupted.";
const char kBadHTTPResponseError[] =
    "A bad HTTP response code (%d) was received when fetching the script.";
const char kSSLError[] =
    "An SSL certificate error occurred when fetching the script.";
const char kBadMIMEError[] = "The script has an unsupported MIME type ('%s').";
const char kNoMIMEError[] = "The script does not have a MIME type.";
const char kClientAuthenticationError[] =
    "Client authentication was required to fetch the script.";
const char kRedirectError[] =
    "The script resource is behind a redirect, which is disallowed.";
const char kServiceWorkerAllowed[] = "Service-Worker-Allowed";

}  // namespace

const net::Error ServiceWorkerWriteToCacheJob::kIdenticalScriptError =
    net::ERR_FILE_EXISTS;

ServiceWorkerWriteToCacheJob::ServiceWorkerWriteToCacheJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    ResourceType resource_type,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* version,
    int extra_load_flags,
    int64_t resource_id,
    int64_t incumbent_resource_id)
    : net::URLRequestJob(request, network_delegate),
      resource_type_(resource_type),
      context_(context),
      url_(request->url()),
      resource_id_(resource_id),
      incumbent_resource_id_(incumbent_resource_id),
      version_(version),
      has_been_killed_(false),
      did_notify_started_(false),
      did_notify_finished_(false),
      weak_factory_(this) {
  InitNetRequest(extra_load_flags);
}

ServiceWorkerWriteToCacheJob::~ServiceWorkerWriteToCacheJob() {
  DCHECK_EQ(did_notify_started_, did_notify_finished_);
}

void ServiceWorkerWriteToCacheJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ServiceWorkerWriteToCacheJob::StartAsync,
                            weak_factory_.GetWeakPtr()));
}

void ServiceWorkerWriteToCacheJob::StartAsync() {
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerWriteToCacheJob::ExecutingJob",
                           this,
                           "URL", request_->url().spec());
  if (!context_) {
    // NotifyStartError is not safe to call synchronously in Start().
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  cache_writer_.reset(new ServiceWorkerCacheWriter(
      CreateCacheResponseReader(), CreateCacheResponseReader(),
      CreateCacheResponseWriter()));
  version_->script_cache_map()->NotifyStartedCaching(url_, resource_id_);
  did_notify_started_ = true;
  StartNetRequest();
}

void ServiceWorkerWriteToCacheJob::Kill() {
  if (has_been_killed_)
    return;
  weak_factory_.InvalidateWeakPtrs();
  has_been_killed_ = true;
  net_request_.reset();
  if (did_notify_started_) {
    net::Error error = NotifyFinishedCaching(
        net::URLRequestStatus::FromError(net::ERR_ABORTED), kKilledError);
    DCHECK_EQ(net::ERR_ABORTED, error);
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

int ServiceWorkerWriteToCacheJob::ReadRawData(net::IOBuffer* buf,
                                              int buf_size) {
  int bytes_read = 0;
  net::URLRequestStatus status = ReadNetData(buf, buf_size, &bytes_read);
  if (status.is_io_pending())
    return net::ERR_IO_PENDING;

  if (!status.is_success()) {
    net::Error error = NotifyFinishedCaching(status, kFetchScriptError);
    DCHECK_EQ(status.error(), error);
    return error;
  }

  return HandleNetData(bytes_read);
}

const net::HttpResponseInfo* ServiceWorkerWriteToCacheJob::http_info() const {
  return http_info_.get();
}

void ServiceWorkerWriteToCacheJob::InitNetRequest(
    int extra_load_flags) {
  DCHECK(request());
  net_request_ = request()->context()->CreateRequest(
      request()->url(), request()->priority(), this);
  net_request_->set_first_party_for_cookies(
      request()->first_party_for_cookies());
  net_request_->set_initiator(request()->initiator());
  net_request_->SetReferrer(request()->referrer());
  net_request_->SetUserData(URLRequestServiceWorkerData::kUserDataKey,
                            new URLRequestServiceWorkerData());
  if (extra_load_flags)
    net_request_->SetLoadFlags(net_request_->load_flags() | extra_load_flags);

  if (resource_type_ == RESOURCE_TYPE_SERVICE_WORKER) {
    // This will get copied into net_request_ when URLRequest::StartJob calls
    // ServiceWorkerWriteToCacheJob::SetExtraRequestHeaders.
    request()->SetExtraRequestHeaderByName("Service-Worker", "script", true);
  }
}

void ServiceWorkerWriteToCacheJob::StartNetRequest() {
  TRACE_EVENT_ASYNC_STEP_INTO0("ServiceWorker",
                               "ServiceWorkerWriteToCacheJob::ExecutingJob",
                               this,
                               "NetRequest");
  net_request_->Start();  // We'll continue in OnResponseStarted.
}

net::URLRequestStatus ServiceWorkerWriteToCacheJob::ReadNetData(
    net::IOBuffer* buf,
    int buf_size,
    int* bytes_read) {
  DCHECK_GT(buf_size, 0);
  DCHECK(bytes_read);
  io_buffer_ = buf;
  io_buffer_bytes_ = 0;
  if (!net_request_->Read(buf, buf_size, bytes_read))
    DCHECK_NE(net::URLRequestStatus::SUCCESS, net_request_->status().status());

  return net_request_->status();
}

void ServiceWorkerWriteToCacheJob::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  DCHECK_EQ(net_request_.get(), request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnReceivedRedirect");
  // Script resources can't redirect.
  NotifyStartErrorHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                               net::ERR_UNSAFE_REDIRECT),
                         kRedirectError);
}

void ServiceWorkerWriteToCacheJob::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  DCHECK_EQ(net_request_.get(), request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnAuthRequired");
  // TODO(michaeln): Pass this thru to our jobs client.
  NotifyStartErrorHelper(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED),
      kClientAuthenticationError);
}

void ServiceWorkerWriteToCacheJob::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK_EQ(net_request_.get(), request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnCertificateRequested");
  // TODO(michaeln): Pass this thru to our jobs client.
  // see NotifyCertificateRequested.
  NotifyStartErrorHelper(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED),
      kClientAuthenticationError);
}

void ServiceWorkerWriteToCacheJob::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK_EQ(net_request_.get(), request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnSSLCertificateError");
  // TODO(michaeln): Pass this thru to our jobs client,
  // see NotifySSLCertificateError.
  NotifyStartErrorHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                               net::ERR_INSECURE_RESPONSE),
                         kSSLError);
}

void ServiceWorkerWriteToCacheJob::OnBeforeNetworkStart(
    net::URLRequest* request,
    bool* defer) {
  DCHECK_EQ(net_request_.get(), request);
  TRACE_EVENT0("ServiceWorker",
               "ServiceWorkerWriteToCacheJob::OnBeforeNetworkStart");
  NotifyBeforeNetworkStart(defer);
}

void ServiceWorkerWriteToCacheJob::OnResponseStarted(
    net::URLRequest* request) {
  DCHECK_EQ(net_request_.get(), request);
  if (!request->status().is_success()) {
    NotifyStartErrorHelper(request->status(), kFetchScriptError);
    return;
  }
  if (request->GetResponseCode() / 100 != 2) {
    std::string error_message =
        base::StringPrintf(kBadHTTPResponseError, request->GetResponseCode());
    NotifyStartErrorHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                                 net::ERR_INVALID_RESPONSE),
                           error_message);
    // TODO(michaeln): Instead of error'ing immediately, send the net
    // response to our consumer, just don't cache it?
    return;
  }
  // OnSSLCertificateError is not called when the HTTPS connection is reused.
  // So we check cert_status here.
  if (net::IsCertStatusError(request->ssl_info().cert_status)) {
    const net::HttpNetworkSession::Params* session_params =
        request->context()->GetNetworkSessionParams();
    if (!session_params || !session_params->ignore_certificate_errors) {
      NotifyStartErrorHelper(
          net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                net::ERR_INSECURE_RESPONSE),
          kSSLError);
      return;
    }
  }

  if (version_->script_url() == url_) {
    std::string mime_type;
    request->GetMimeType(&mime_type);
    if (mime_type != "application/x-javascript" &&
        mime_type != "text/javascript" &&
        mime_type != "application/javascript") {
      std::string error_message =
          mime_type.empty()
              ? kNoMIMEError
              : base::StringPrintf(kBadMIMEError, mime_type.c_str());
      NotifyStartErrorHelper(
          net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                net::ERR_INSECURE_RESPONSE),
          error_message);
      return;
    }

    if (!CheckPathRestriction(request))
      return;

    version_->SetMainScriptHttpResponseInfo(net_request_->response_info());
  }

  if (net_request_->response_info().network_accessed &&
      !(net_request_->response_info().was_cached)) {
    version_->embedded_worker()->OnNetworkAccessedForScriptLoad();
  }

  http_info_.reset(new net::HttpResponseInfo(net_request_->response_info()));
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer(
          new net::HttpResponseInfo(net_request_->response_info()));
  net::Error error = cache_writer_->MaybeWriteHeaders(
      info_buffer.get(),
      base::Bind(&ServiceWorkerWriteToCacheJob::OnWriteHeadersComplete,
                 weak_factory_.GetWeakPtr()));
  if (error == net::ERR_IO_PENDING)
    return;
  OnWriteHeadersComplete(error);
}

void ServiceWorkerWriteToCacheJob::OnWriteHeadersComplete(net::Error error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);
  if (error != net::OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_HEADERS_ERROR);
    NotifyStartError(net::URLRequestStatus::FromError(error));
    return;
  }
  NotifyHeadersComplete();
}

void ServiceWorkerWriteToCacheJob::OnWriteDataComplete(net::Error error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);
  if (io_buffer_bytes_ == 0)
    error = NotifyFinishedCaching(net::URLRequestStatus::FromError(error), "");
  if (error != net::OK) {
    ServiceWorkerMetrics::CountWriteResponseResult(
        ServiceWorkerMetrics::WRITE_DATA_ERROR);
    ReadRawDataComplete(error);
    return;
  }
  ServiceWorkerMetrics::CountWriteResponseResult(
      ServiceWorkerMetrics::WRITE_OK);
  ReadRawDataComplete(io_buffer_bytes_);
}

void ServiceWorkerWriteToCacheJob::OnReadCompleted(net::URLRequest* request,
                                                   int bytes_read) {
  DCHECK_EQ(net_request_.get(), request);

  int result;
  if (bytes_read < 0) {
    DCHECK(!request->status().is_success());
    result = NotifyFinishedCaching(request->status(), kFetchScriptError);
  } else {
    result = HandleNetData(bytes_read);
  }

  // ReadRawDataComplete will be called in OnWriteDataComplete, so return early.
  if (result == net::ERR_IO_PENDING)
    return;

  ReadRawDataComplete(result);
}

bool ServiceWorkerWriteToCacheJob::CheckPathRestriction(
    net::URLRequest* request) {
  std::string service_worker_allowed;
  const net::HttpResponseHeaders* headers = request->response_headers();
  bool has_header = headers->EnumerateHeader(nullptr, kServiceWorkerAllowed,
                                             &service_worker_allowed);

  std::string error_message;
  if (!ServiceWorkerUtils::IsPathRestrictionSatisfied(
          version_->scope(), url_,
          has_header ? &service_worker_allowed : nullptr, &error_message)) {
    NotifyStartErrorHelper(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                                 net::ERR_INSECURE_RESPONSE),
                           error_message);
    return false;
  }
  return true;
}

int ServiceWorkerWriteToCacheJob::HandleNetData(int bytes_read) {
  io_buffer_bytes_ = bytes_read;
  net::Error error = cache_writer_->MaybeWriteData(
      io_buffer_.get(), bytes_read,
      base::Bind(&ServiceWorkerWriteToCacheJob::OnWriteDataComplete,
                 weak_factory_.GetWeakPtr()));

  // In case of ERR_IO_PENDING, this logic is done in OnWriteDataComplete.
  if (error != net::ERR_IO_PENDING && bytes_read == 0) {
    error = NotifyFinishedCaching(net::URLRequestStatus::FromError(error),
                                  std::string());
  }
  return error == net::OK ? bytes_read : error;
}

void ServiceWorkerWriteToCacheJob::NotifyStartErrorHelper(
    const net::URLRequestStatus& status,
    const std::string& status_message) {
  DCHECK(!status.is_io_pending());
  NotifyFinishedCaching(status, status_message);
  NotifyStartError(status);
}

net::Error ServiceWorkerWriteToCacheJob::NotifyFinishedCaching(
    net::URLRequestStatus status,
    const std::string& status_message) {
  net::Error result = static_cast<net::Error>(status.error());
  if (did_notify_finished_)
    return result;

  if (status.status() != net::URLRequestStatus::SUCCESS) {
    // AddMessageToConsole must be called before this job notifies that an error
    // occurred because the worker stops soon after receiving the error
    // response.
    version_->embedded_worker()->AddMessageToConsole(
        CONSOLE_MESSAGE_LEVEL_ERROR,
        status_message.empty() ? kFetchScriptError : status_message);
  }

  int size = -1;
  if (status.is_success())
    size = cache_writer_->bytes_written();

  // If all the calls to MaybeWriteHeaders/MaybeWriteData succeeded, but the
  // incumbent entry wasn't actually replaced because the new entry was
  // equivalent, the new version didn't actually install because it already
  // exists.
  if (status.status() == net::URLRequestStatus::SUCCESS &&
      !cache_writer_->did_replace()) {
    status = net::URLRequestStatus::FromError(kIdenticalScriptError);
    version_->SetStartWorkerStatusCode(SERVICE_WORKER_ERROR_EXISTS);
    version_->script_cache_map()->NotifyFinishedCaching(url_, size, status,
                                                        std::string());
  } else {
    version_->script_cache_map()->NotifyFinishedCaching(url_, size, status,
                                                        status_message);
  }

  did_notify_finished_ = true;
  return result;
}

std::unique_ptr<ServiceWorkerResponseReader>
ServiceWorkerWriteToCacheJob::CreateCacheResponseReader() {
  if (incumbent_resource_id_ == kInvalidServiceWorkerResourceId ||
      !version_->pause_after_download()) {
    return nullptr;
  }
  return context_->storage()->CreateResponseReader(incumbent_resource_id_);
}

std::unique_ptr<ServiceWorkerResponseWriter>
ServiceWorkerWriteToCacheJob::CreateCacheResponseWriter() {
  return context_->storage()->CreateResponseWriter(resource_id_);
}

}  // namespace content
