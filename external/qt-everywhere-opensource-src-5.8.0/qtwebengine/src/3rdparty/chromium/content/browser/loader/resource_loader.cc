// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_loader.h"

#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/appcache/appcache_interceptor.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/cross_site_resource_handler.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/service_worker/service_worker_response_info.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/ssl/ssl_policy.h"
#include "content/common/ssl_status_serialization.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/security_style.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_platform_key.h"
#include "net/ssl/ssl_private_key.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

using base::TimeDelta;
using base::TimeTicks;

namespace content {
namespace {

void GetSSLStatusForRequest(const GURL& url,
                            const net::SSLInfo& ssl_info,
                            int child_id,
                            CertStore* cert_store,
                            SSLStatus* ssl_status) {
  DCHECK(ssl_info.cert);
  int cert_id = cert_store->StoreCert(ssl_info.cert.get(), child_id);

  *ssl_status = SSLStatus(SSLPolicy::GetSecurityStyleForResource(
                              url, cert_id, ssl_info.cert_status),
                          cert_id, ssl_info);
}

void PopulateResourceResponse(ResourceRequestInfoImpl* info,
                              net::URLRequest* request,
                              CertStore* cert_store,
                              ResourceResponse* response) {
  response->head.request_time = request->request_time();
  response->head.response_time = request->response_time();
  response->head.headers = request->response_headers();
  request->GetCharset(&response->head.charset);
  response->head.content_length = request->GetExpectedContentSize();
  request->GetMimeType(&response->head.mime_type);
  net::HttpResponseInfo response_info = request->response_info();
  response->head.was_fetched_via_spdy = response_info.was_fetched_via_spdy;
  response->head.was_npn_negotiated = response_info.was_npn_negotiated;
  response->head.npn_negotiated_protocol =
      response_info.npn_negotiated_protocol;
  response->head.connection_info = response_info.connection_info;
  response->head.was_fetched_via_proxy = request->was_fetched_via_proxy();
  response->head.proxy_server = response_info.proxy_server;
  response->head.socket_address = response_info.socket_address;
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (request_info)
    response->head.is_using_lofi = request_info->IsUsingLoFi();

  response->head.effective_connection_type =
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  if (info->IsMainFrame()) {
    net::NetworkQualityEstimator* estimator =
        request->context()->network_quality_estimator();
    if (estimator) {
      response->head.effective_connection_type =
          estimator->GetEffectiveConnectionType();
    }
  }

  const ServiceWorkerResponseInfo* service_worker_info =
      ServiceWorkerResponseInfo::ForRequest(request);
  if (service_worker_info)
    service_worker_info->GetExtraResponseInfo(&response->head);
  AppCacheInterceptor::GetExtraResponseInfo(
      request, &response->head.appcache_id,
      &response->head.appcache_manifest_url);
  if (info->is_load_timing_enabled())
    request->GetLoadTimingInfo(&response->head.load_timing);

  if (request->ssl_info().cert.get()) {
    SSLStatus ssl_status;
    GetSSLStatusForRequest(request->url(), request->ssl_info(),
                           info->GetChildID(), cert_store, &ssl_status);
    response->head.security_info = SerializeSecurityInfo(ssl_status);
    response->head.has_major_certificate_errors =
        net::IsCertStatusError(ssl_status.cert_status) &&
        !net::IsCertStatusMinorError(ssl_status.cert_status);
    if (info->ShouldReportRawHeaders()) {
      // Only pass the Signed Certificate Timestamps (SCTs) when the network
      // panel of the DevTools is open, i.e. ShouldReportRawHeaders() is set.
      // These data are used to populate the requests in the security panel too.
      response->head.signed_certificate_timestamps =
          request->ssl_info().signed_certificate_timestamps;
    }
  } else {
    // We should not have any SSL state.
    DCHECK(!request->ssl_info().cert_status);
    DCHECK_EQ(request->ssl_info().security_bits, -1);
    DCHECK_EQ(request->ssl_info().key_exchange_info, 0);
    DCHECK(!request->ssl_info().connection_status);
  }
}

}  // namespace

ResourceLoader::ResourceLoader(std::unique_ptr<net::URLRequest> request,
                               std::unique_ptr<ResourceHandler> handler,
                               CertStore* cert_store,
                               ResourceLoaderDelegate* delegate)
    : deferred_stage_(DEFERRED_NONE),
      request_(std::move(request)),
      handler_(std::move(handler)),
      delegate_(delegate),
      is_transferring_(false),
      times_cancelled_before_request_start_(0),
      started_request_(false),
      times_cancelled_after_request_start_(0),
      cert_store_(cert_store),
      weak_ptr_factory_(this) {
  request_->set_delegate(this);
  handler_->SetController(this);
}

ResourceLoader::~ResourceLoader() {
  if (login_delegate_.get())
    login_delegate_->OnRequestCancelled();
  ssl_client_auth_handler_.reset();

  // Run ResourceHandler destructor before we tear-down the rest of our state
  // as the ResourceHandler may want to inspect the URLRequest and other state.
  handler_.reset();
}

void ResourceLoader::StartRequest() {
  if (delegate_->HandleExternalProtocol(this, request_->url())) {
    CancelAndIgnore();
    return;
  }

  // Give the handler a chance to delay the URLRequest from being started.
  bool defer_start = false;
  if (!handler_->OnWillStart(request_->url(), &defer_start)) {
    Cancel();
    return;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::StartRequest", this,
                         TRACE_EVENT_FLAG_FLOW_OUT);
  if (defer_start) {
    deferred_stage_ = DEFERRED_START;
  } else {
    StartRequestInternal();
  }
}

void ResourceLoader::CancelRequest(bool from_renderer) {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::CancelRequest", this,
                         TRACE_EVENT_FLAG_FLOW_IN);
  CancelRequestInternal(net::ERR_ABORTED, from_renderer);
}

void ResourceLoader::CancelAndIgnore() {
  ResourceRequestInfoImpl* info = GetRequestInfo();
  info->set_was_ignored_by_handler(true);
  CancelRequest(false);
}

void ResourceLoader::CancelWithError(int error_code) {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::CancelWithError", this,
                         TRACE_EVENT_FLAG_FLOW_IN);
  CancelRequestInternal(error_code, false);
}

void ResourceLoader::MarkAsTransferring(
    const scoped_refptr<ResourceResponse>& response) {
  CHECK(IsResourceTypeFrame(GetRequestInfo()->GetResourceType()))
      << "Can only transfer for navigations";
  is_transferring_ = true;
  transferring_response_ = response;

  int child_id = GetRequestInfo()->GetChildID();
  AppCacheInterceptor::PrepareForCrossSiteTransfer(request(), child_id);
  ServiceWorkerRequestHandler* handler =
      ServiceWorkerRequestHandler::GetHandler(request());
  if (handler)
    handler->PrepareForCrossSiteTransfer(child_id);
}

void ResourceLoader::CompleteTransfer() {
  // Although CrossSiteResourceHandler defers at OnResponseStarted
  // (DEFERRED_READ), it may be seeing a replay of events via
  // MimeTypeResourceHandler, and so the request itself is actually deferred
  // at a later read stage.
  DCHECK(DEFERRED_READ == deferred_stage_ ||
         DEFERRED_RESPONSE_COMPLETE == deferred_stage_);
  DCHECK(is_transferring_);
  DCHECK(transferring_response_);

  // In some cases, a process transfer doesn't really happen and the
  // request is resumed in the original process. Real transfers to a new process
  // are completed via ResourceDispatcherHostImpl::UpdateRequestForTransfer.
  int child_id = GetRequestInfo()->GetChildID();
  AppCacheInterceptor::MaybeCompleteCrossSiteTransferInOldProcess(
      request(), child_id);
  ServiceWorkerRequestHandler* handler =
      ServiceWorkerRequestHandler::GetHandler(request());
  if (handler)
    handler->MaybeCompleteCrossSiteTransferInOldProcess(child_id);

  is_transferring_ = false;
  transferring_response_ = nullptr;
  GetRequestInfo()->cross_site_handler()->ResumeResponse();
}

ResourceRequestInfoImpl* ResourceLoader::GetRequestInfo() {
  return ResourceRequestInfoImpl::ForRequest(request_.get());
}

void ResourceLoader::ClearLoginDelegate() {
  login_delegate_ = NULL;
}

void ResourceLoader::OnReceivedRedirect(net::URLRequest* unused,
                                        const net::RedirectInfo& redirect_info,
                                        bool* defer) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "ResourceLoader::OnReceivedRedirect");
  DCHECK_EQ(request_.get(), unused);

  DVLOG(1) << "OnReceivedRedirect: " << request_->url().spec();
  DCHECK(request_->status().is_success());

  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          info->GetChildID(), redirect_info.new_url)) {
    DVLOG(1) << "Denied unauthorized request for "
             << redirect_info.new_url.possibly_invalid_spec();

    // Tell the renderer that this request was disallowed.
    Cancel();
    return;
  }

  delegate_->DidReceiveRedirect(this, redirect_info.new_url);

  if (delegate_->HandleExternalProtocol(this, redirect_info.new_url)) {
    // The request is complete so we can remove it.
    CancelAndIgnore();
    return;
  }

  scoped_refptr<ResourceResponse> response = new ResourceResponse();
  PopulateResourceResponse(info, request_.get(), cert_store_, response.get());
  if (!handler_->OnRequestRedirected(redirect_info, response.get(), defer)) {
    Cancel();
  } else if (*defer) {
    deferred_stage_ = DEFERRED_REDIRECT;  // Follow redirect when resumed.
  }
}

void ResourceLoader::OnAuthRequired(net::URLRequest* unused,
                                    net::AuthChallengeInfo* auth_info) {
  DCHECK_EQ(request_.get(), unused);

  ResourceRequestInfoImpl* info = GetRequestInfo();
  if (info->do_not_prompt_for_login()) {
    request_->CancelAuth();
    return;
  }

  // Create a login dialog on the UI thread to get authentication data, or pull
  // from cache and continue on the IO thread.

  DCHECK(!login_delegate_.get())
      << "OnAuthRequired called with login_delegate pending";
  login_delegate_ = delegate_->CreateLoginDelegate(this, auth_info);
  if (!login_delegate_.get())
    request_->CancelAuth();
}

void ResourceLoader::OnCertificateRequested(
    net::URLRequest* unused,
    net::SSLCertRequestInfo* cert_info) {
  DCHECK_EQ(request_.get(), unused);

  if (request_->load_flags() & net::LOAD_PREFETCH) {
    request_->Cancel();
    return;
  }

  DCHECK(!ssl_client_auth_handler_)
      << "OnCertificateRequested called with ssl_client_auth_handler pending";
  ssl_client_auth_handler_.reset(new SSLClientAuthHandler(
      delegate_->CreateClientCertStore(this), request_.get(), cert_info, this));
  ssl_client_auth_handler_->SelectCertificate();
}

void ResourceLoader::OnSSLCertificateError(net::URLRequest* request,
                                           const net::SSLInfo& ssl_info,
                                           bool fatal) {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  SSLManager::OnSSLCertificateError(
      weak_ptr_factory_.GetWeakPtr(), info->GetResourceType(), request_->url(),
      info->GetWebContentsGetterForRequest(), ssl_info, fatal);
}

void ResourceLoader::OnBeforeNetworkStart(net::URLRequest* unused,
                                          bool* defer) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "ResourceLoader::OnBeforeNetworkStart");
  DCHECK_EQ(request_.get(), unused);

  // Give the handler a chance to delay the URLRequest from using the network.
  if (!handler_->OnBeforeNetworkStart(request_->url(), defer)) {
    Cancel();
    return;
  } else if (*defer) {
    deferred_stage_ = DEFERRED_NETWORK_START;
  }
}

void ResourceLoader::OnResponseStarted(net::URLRequest* unused) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "ResourceLoader::OnResponseStarted");
  DCHECK_EQ(request_.get(), unused);

  DVLOG(1) << "OnResponseStarted: " << request_->url().spec();

  if (!request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  CompleteResponseStarted();

  if (is_deferred())
    return;

  if (request_->status().is_success())
    StartReading(false);  // Read the first chunk.
  else
    ResponseCompleted();
}

void ResourceLoader::OnReadCompleted(net::URLRequest* unused, int bytes_read) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "ResourceLoader::OnReadCompleted");
  DCHECK_EQ(request_.get(), unused);
  DVLOG(1) << "OnReadCompleted: \"" << request_->url().spec() << "\""
           << " bytes_read = " << bytes_read;

  // bytes_read == -1 always implies an error.
  if (bytes_read == -1 || !request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  CompleteRead(bytes_read);

  // If the handler cancelled or deferred the request, do not continue
  // processing the read. If cancelled, the URLRequest has already been
  // cancelled and will schedule an erroring OnReadCompleted later. If deferred,
  // do nothing until resumed.
  //
  // Note: if bytes_read is 0 (EOF) and the handler defers, resumption will call
  // ResponseCompleted().
  if (is_deferred() || !request_->status().is_success())
    return;

  if (bytes_read > 0) {
    StartReading(true);  // Read the next chunk.
  } else {
    // TODO(darin): Remove ScopedTracker below once crbug.com/475761 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("475761 ResponseCompleted()"));

    // URLRequest reported an EOF. Call ResponseCompleted.
    DCHECK_EQ(0, bytes_read);
    ResponseCompleted();
  }
}

void ResourceLoader::CancelSSLRequest(int error,
                                      const net::SSLInfo* ssl_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The request can be NULL if it was cancelled by the renderer (as the
  // request of the user navigating to a new page from the location bar).
  if (!request_->is_pending())
    return;
  DVLOG(1) << "CancelSSLRequest() url: " << request_->url().spec();

  if (ssl_info) {
    request_->CancelWithSSLError(error, *ssl_info);
  } else {
    request_->CancelWithError(error);
  }
}

void ResourceLoader::ContinueSSLRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DVLOG(1) << "ContinueSSLRequest() url: " << request_->url().spec();

  request_->ContinueDespiteLastError();
}

void ResourceLoader::ContinueWithCertificate(net::X509Certificate* cert) {
  DCHECK(ssl_client_auth_handler_);
  ssl_client_auth_handler_.reset();
  if (!cert) {
    request_->ContinueWithCertificate(nullptr, nullptr);
    return;
  }
  scoped_refptr<net::SSLPrivateKey> private_key =
      net::FetchClientCertPrivateKey(cert);
  request_->ContinueWithCertificate(cert, private_key.get());
}

void ResourceLoader::CancelCertificateSelection() {
  DCHECK(ssl_client_auth_handler_);
  ssl_client_auth_handler_.reset();
  request_->CancelWithError(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void ResourceLoader::Resume() {
  DCHECK(!is_transferring_);

  DeferredStage stage = deferred_stage_;
  deferred_stage_ = DEFERRED_NONE;
  switch (stage) {
    case DEFERRED_NONE:
      NOTREACHED();
      break;
    case DEFERRED_START:
      StartRequestInternal();
      break;
    case DEFERRED_NETWORK_START:
      request_->ResumeNetworkStart();
      break;
    case DEFERRED_REDIRECT:
      request_->FollowDeferredRedirect();
      break;
    case DEFERRED_READ:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&ResourceLoader::ResumeReading,
                                weak_ptr_factory_.GetWeakPtr()));
      break;
    case DEFERRED_RESPONSE_COMPLETE:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&ResourceLoader::ResponseCompleted,
                                weak_ptr_factory_.GetWeakPtr()));
      break;
    case DEFERRED_FINISH:
      // Delay self-destruction since we don't know how we were reached.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&ResourceLoader::CallDidFinishLoading,
                                weak_ptr_factory_.GetWeakPtr()));
      break;
  }
}

void ResourceLoader::Cancel() {
  CancelRequest(false);
}

void ResourceLoader::StartRequestInternal() {
  DCHECK(!request_->is_pending());

  if (!request_->status().is_success()) {
    return;
  }

  started_request_ = true;
  request_->Start();

  delegate_->DidStartRequest(this);
}

void ResourceLoader::CancelRequestInternal(int error, bool from_renderer) {
  DVLOG(1) << "CancelRequestInternal: " << request_->url().spec();

  ResourceRequestInfoImpl* info = GetRequestInfo();

  // WebKit will send us a cancel for downloads since it no longer handles
  // them.  In this case, ignore the cancel since we handle downloads in the
  // browser.
  if (from_renderer && (info->IsDownload() || info->is_stream()))
    return;

  if (from_renderer && info->detachable_handler()) {
    // TODO(davidben): Fix Blink handling of prefetches so they are not
    // cancelled on navigate away and end up in the local cache.
    info->detachable_handler()->Detach();
    return;
  }

  // TODO(darin): Perhaps we should really be looking to see if the status is
  // IO_PENDING?
  bool was_pending = request_->is_pending();

  if (login_delegate_.get()) {
    login_delegate_->OnRequestCancelled();
    login_delegate_ = NULL;
  }
  ssl_client_auth_handler_.reset();

  if (!started_request_) {
    times_cancelled_before_request_start_++;
  } else {
    times_cancelled_after_request_start_++;
  }

  request_->CancelWithError(error);

  if (!was_pending) {
    // If the request isn't in flight, then we won't get an asynchronous
    // notification from the request, so we have to signal ourselves to finish
    // this request.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ResourceLoader::ResponseCompleted,
                              weak_ptr_factory_.GetWeakPtr()));
  }
}

void ResourceLoader::CompleteResponseStarted() {
  ResourceRequestInfoImpl* info = GetRequestInfo();
  scoped_refptr<ResourceResponse> response = new ResourceResponse();
  PopulateResourceResponse(info, request_.get(), cert_store_, response.get());

  delegate_->DidReceiveResponse(this);

  // TODO(darin): Remove ScopedTracker below once crbug.com/475761 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("475761 OnResponseStarted()"));

  bool defer = false;
  if (!handler_->OnResponseStarted(response.get(), &defer)) {
    Cancel();
  } else if (defer) {
    read_deferral_start_time_ = base::TimeTicks::Now();
    deferred_stage_ = DEFERRED_READ;  // Read first chunk when resumed.
  }
}

void ResourceLoader::StartReading(bool is_continuation) {
  int bytes_read = 0;
  ReadMore(&bytes_read);

  // If IO is pending, wait for the URLRequest to call OnReadCompleted.
  if (request_->status().is_io_pending())
    return;

  if (!is_continuation || bytes_read <= 0) {
    OnReadCompleted(request_.get(), bytes_read);
  } else {
    // Else, trigger OnReadCompleted asynchronously to avoid starving the IO
    // thread in case the URLRequest can provide data synchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ResourceLoader::OnReadCompleted,
                   weak_ptr_factory_.GetWeakPtr(), request_.get(), bytes_read));
  }
}

void ResourceLoader::ResumeReading() {
  DCHECK(!is_deferred());

  if (!read_deferral_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Net.ResourceLoader.ReadDeferral",
                        base::TimeTicks::Now() - read_deferral_start_time_);
    read_deferral_start_time_ = base::TimeTicks();
  }
  if (request_->status().is_success()) {
    StartReading(false);  // Read the next chunk (OK to complete synchronously).
  } else {
    ResponseCompleted();
  }
}

void ResourceLoader::ReadMore(int* bytes_read) {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::ReadMore", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!is_deferred());

  // Make sure we track the buffer in at least one place.  This ensures it gets
  // deleted even in the case the request has already finished its job and
  // doesn't use the buffer.
  scoped_refptr<net::IOBuffer> buf;
  int buf_size;
  {
    // TODO(darin): Remove ScopedTracker below once crbug.com/475761 is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("475761 OnWillRead()"));

    if (!handler_->OnWillRead(&buf, &buf_size, -1)) {
      Cancel();
      return;
    }
  }

  DCHECK(buf.get());
  DCHECK(buf_size > 0);

  request_->Read(buf.get(), buf_size, bytes_read);

  // No need to check the return value here as we'll detect errors by
  // inspecting the URLRequest's status.
}

void ResourceLoader::CompleteRead(int bytes_read) {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::CompleteRead", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  DCHECK(bytes_read >= 0);
  DCHECK(request_->status().is_success());

  // TODO(darin): Remove ScopedTracker below once crbug.com/475761 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("475761 OnReadCompleted()"));

  bool defer = false;
  if (!handler_->OnReadCompleted(bytes_read, &defer)) {
    Cancel();
  } else if (defer) {
    deferred_stage_ =
        bytes_read > 0 ? DEFERRED_READ : DEFERRED_RESPONSE_COMPLETE;
  }

  // Note: the request may still have been cancelled while OnReadCompleted
  // returns true if OnReadCompleted caused request to get cancelled
  // out-of-band. (In AwResourceDispatcherHostDelegate::DownloadStarting, for
  // instance.)
}

void ResourceLoader::ResponseCompleted() {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::ResponseCompleted", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  DVLOG(1) << "ResponseCompleted: " << request_->url().spec();
  RecordHistograms();
  ResourceRequestInfoImpl* info = GetRequestInfo();

  std::string security_info;
  const net::SSLInfo& ssl_info = request_->ssl_info();
  if (ssl_info.cert.get() != NULL) {
    SSLStatus ssl_status;
    GetSSLStatusForRequest(request_->url(), ssl_info, info->GetChildID(),
                           cert_store_, &ssl_status);

    security_info = SerializeSecurityInfo(ssl_status);
  }

  bool defer = false;
  {
    // TODO(darin): Remove ScopedTracker below once crbug.com/475761 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("475761 OnResponseCompleted()"));

    handler_->OnResponseCompleted(request_->status(), security_info, &defer);
  }
  if (defer) {
    // The handler is not ready to die yet.  We will call DidFinishLoading when
    // we resume.
    deferred_stage_ = DEFERRED_FINISH;
  } else {
    // This will result in our destruction.
    CallDidFinishLoading();
  }
}

void ResourceLoader::CallDidFinishLoading() {
  TRACE_EVENT_WITH_FLOW0("loading", "ResourceLoader::CallDidFinishLoading",
                         this, TRACE_EVENT_FLAG_FLOW_IN);
  delegate_->DidFinishLoading(this);
}

void ResourceLoader::RecordHistograms() {
  ResourceRequestInfoImpl* info = GetRequestInfo();
  if (request_->response_info().network_accessed) {
    if (info->GetResourceType() == RESOURCE_TYPE_MAIN_FRAME) {
      UMA_HISTOGRAM_ENUMERATION("Net.HttpResponseInfo.ConnectionInfo.MainFrame",
                                request_->response_info().connection_info,
                                net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Net.HttpResponseInfo.ConnectionInfo.SubResource",
          request_->response_info().connection_info,
          net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS);
    }
  }

  if (info->GetResourceType() == RESOURCE_TYPE_PREFETCH) {
    PrefetchStatus status = STATUS_UNDEFINED;
    TimeDelta total_time = base::TimeTicks::Now() - request_->creation_time();

    switch (request_->status().status()) {
      case net::URLRequestStatus::SUCCESS:
        if (request_->was_cached()) {
          status = STATUS_SUCCESS_FROM_CACHE;
          UMA_HISTOGRAM_TIMES("Net.Prefetch.TimeSpentPrefetchingFromCache",
                              total_time);
        } else {
          status = STATUS_SUCCESS_FROM_NETWORK;
          UMA_HISTOGRAM_TIMES("Net.Prefetch.TimeSpentPrefetchingFromNetwork",
                              total_time);
        }
        break;
      case net::URLRequestStatus::CANCELED:
        status = STATUS_CANCELED;
        UMA_HISTOGRAM_TIMES("Net.Prefetch.TimeBeforeCancel", total_time);
        break;
      case net::URLRequestStatus::IO_PENDING:
      case net::URLRequestStatus::FAILED:
        status = STATUS_UNDEFINED;
        break;
    }

    UMA_HISTOGRAM_ENUMERATION("Net.Prefetch.Pattern", status, STATUS_MAX);
  } else if (request_->response_info().unused_since_prefetch) {
    TimeDelta total_time = base::TimeTicks::Now() - request_->creation_time();
    UMA_HISTOGRAM_TIMES("Net.Prefetch.TimeSpentOnPrefetchHit", total_time);
  }
}

}  // namespace content
