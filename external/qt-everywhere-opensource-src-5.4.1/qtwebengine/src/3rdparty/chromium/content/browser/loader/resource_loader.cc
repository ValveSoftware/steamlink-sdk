// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_loader.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "content/browser/appcache/appcache_interceptor.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/cross_site_resource_handler.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/common/ssl_status_serialization.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/browser/signed_certificate_timestamp_store.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request_status.h"

using base::TimeDelta;
using base::TimeTicks;

namespace content {
namespace {

void PopulateResourceResponse(net::URLRequest* request,
                              ResourceResponse* response) {
  response->head.error_code = request->status().error();
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
  response->head.socket_address = request->GetSocketAddress();
  AppCacheInterceptor::GetExtraResponseInfo(
      request,
      &response->head.appcache_id,
      &response->head.appcache_manifest_url);
  // TODO(mmenke):  Figure out if LOAD_ENABLE_LOAD_TIMING is safe to remove.
  if (request->load_flags() & net::LOAD_ENABLE_LOAD_TIMING)
    request->GetLoadTimingInfo(&response->head.load_timing);
}

}  // namespace

ResourceLoader::ResourceLoader(scoped_ptr<net::URLRequest> request,
                               scoped_ptr<ResourceHandler> handler,
                               ResourceLoaderDelegate* delegate)
    : deferred_stage_(DEFERRED_NONE),
      request_(request.Pass()),
      handler_(handler.Pass()),
      delegate_(delegate),
      last_upload_position_(0),
      waiting_for_upload_progress_ack_(false),
      is_transferring_(false),
      weak_ptr_factory_(this) {
  request_->set_delegate(this);
  handler_->SetController(this);
}

ResourceLoader::~ResourceLoader() {
  if (login_delegate_.get())
    login_delegate_->OnRequestCancelled();
  if (ssl_client_auth_handler_.get())
    ssl_client_auth_handler_->OnRequestCancelled();

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

  if (defer_start) {
    deferred_stage_ = DEFERRED_START;
  } else {
    StartRequestInternal();
  }
}

void ResourceLoader::CancelRequest(bool from_renderer) {
  CancelRequestInternal(net::ERR_ABORTED, from_renderer);
}

void ResourceLoader::CancelAndIgnore() {
  ResourceRequestInfoImpl* info = GetRequestInfo();
  info->set_was_ignored_by_handler(true);
  CancelRequest(false);
}

void ResourceLoader::CancelWithError(int error_code) {
  CancelRequestInternal(error_code, false);
}

void ResourceLoader::ReportUploadProgress() {
  if (waiting_for_upload_progress_ack_)
    return;  // Send one progress event at a time.

  net::UploadProgress progress = request_->GetUploadProgress();
  if (!progress.size())
    return;  // Nothing to upload.

  if (progress.position() == last_upload_position_)
    return;  // No progress made since last time.

  const uint64 kHalfPercentIncrements = 200;
  const TimeDelta kOneSecond = TimeDelta::FromMilliseconds(1000);

  uint64 amt_since_last = progress.position() - last_upload_position_;
  TimeDelta time_since_last = TimeTicks::Now() - last_upload_ticks_;

  bool is_finished = (progress.size() == progress.position());
  bool enough_new_progress =
      (amt_since_last > (progress.size() / kHalfPercentIncrements));
  bool too_much_time_passed = time_since_last > kOneSecond;

  if (is_finished || enough_new_progress || too_much_time_passed) {
    if (request_->load_flags() & net::LOAD_ENABLE_UPLOAD_PROGRESS) {
      handler_->OnUploadProgress(progress.position(), progress.size());
      waiting_for_upload_progress_ack_ = true;
    }
    last_upload_ticks_ = TimeTicks::Now();
    last_upload_position_ = progress.position();
  }
}

void ResourceLoader::MarkAsTransferring() {
  CHECK(ResourceType::IsFrame(GetRequestInfo()->GetResourceType()))
      << "Can only transfer for navigations";
  is_transferring_ = true;
}

void ResourceLoader::CompleteTransfer() {
  // Although CrossSiteResourceHandler defers at OnResponseStarted
  // (DEFERRED_READ), it may be seeing a replay of events via
  // BufferedResourceHandler, and so the request itself is actually deferred at
  // a later read stage.
  DCHECK(DEFERRED_READ == deferred_stage_ ||
         DEFERRED_RESPONSE_COMPLETE == deferred_stage_);

  is_transferring_ = false;
  GetRequestInfo()->cross_site_handler()->ResumeResponse();
}

ResourceRequestInfoImpl* ResourceLoader::GetRequestInfo() {
  return ResourceRequestInfoImpl::ForRequest(request_.get());
}

void ResourceLoader::ClearLoginDelegate() {
  login_delegate_ = NULL;
}

void ResourceLoader::ClearSSLClientAuthHandler() {
  ssl_client_auth_handler_ = NULL;
}

void ResourceLoader::OnUploadProgressACK() {
  waiting_for_upload_progress_ack_ = false;
}

void ResourceLoader::OnReceivedRedirect(net::URLRequest* unused,
                                        const GURL& new_url,
                                        bool* defer) {
  DCHECK_EQ(request_.get(), unused);

  VLOG(1) << "OnReceivedRedirect: " << request_->url().spec();
  DCHECK(request_->status().is_success());

  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (info->GetProcessType() != PROCESS_TYPE_PLUGIN &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->
          CanRequestURL(info->GetChildID(), new_url)) {
    VLOG(1) << "Denied unauthorized request for "
            << new_url.possibly_invalid_spec();

    // Tell the renderer that this request was disallowed.
    Cancel();
    return;
  }

  delegate_->DidReceiveRedirect(this, new_url);

  if (delegate_->HandleExternalProtocol(this, new_url)) {
    // The request is complete so we can remove it.
    CancelAndIgnore();
    return;
  }

  scoped_refptr<ResourceResponse> response(new ResourceResponse());
  PopulateResourceResponse(request_.get(), response.get());

  if (!handler_->OnRequestRedirected(new_url, response.get(), defer)) {
    Cancel();
  } else if (*defer) {
    deferred_stage_ = DEFERRED_REDIRECT;  // Follow redirect when resumed.
  }
}

void ResourceLoader::OnAuthRequired(net::URLRequest* unused,
                                    net::AuthChallengeInfo* auth_info) {
  DCHECK_EQ(request_.get(), unused);

  if (request_->load_flags() & net::LOAD_DO_NOT_PROMPT_FOR_LOGIN) {
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

  DCHECK(!ssl_client_auth_handler_.get())
      << "OnCertificateRequested called with ssl_client_auth_handler pending";
  ssl_client_auth_handler_ = new SSLClientAuthHandler(
      GetRequestInfo()->GetContext()->CreateClientCertStore(),
      request_.get(),
      cert_info);
  ssl_client_auth_handler_->SelectCertificate();
}

void ResourceLoader::OnSSLCertificateError(net::URLRequest* request,
                                           const net::SSLInfo& ssl_info,
                                           bool fatal) {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  int render_process_id;
  int render_frame_id;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_id))
    NOTREACHED();

  SSLManager::OnSSLCertificateError(
      weak_ptr_factory_.GetWeakPtr(),
      info->GetGlobalRequestID(),
      info->GetResourceType(),
      request_->url(),
      render_process_id,
      render_frame_id,
      ssl_info,
      fatal);
}

void ResourceLoader::OnBeforeNetworkStart(net::URLRequest* unused,
                                          bool* defer) {
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
  DCHECK_EQ(request_.get(), unused);

  VLOG(1) << "OnResponseStarted: " << request_->url().spec();

  // The CanLoadPage check should take place after any server redirects have
  // finished, at the point in time that we know a page will commit in the
  // renderer process.
  ResourceRequestInfoImpl* info = GetRequestInfo();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanLoadPage(info->GetChildID(),
                           request_->url(),
                           info->GetResourceType())) {
    Cancel();
    return;
  }

  if (!request_->status().is_success()) {
    ResponseCompleted();
    return;
  }

  // We want to send a final upload progress message prior to sending the
  // response complete message even if we're waiting for an ack to to a
  // previous upload progress message.
  waiting_for_upload_progress_ack_ = false;
  ReportUploadProgress();

  CompleteResponseStarted();

  if (is_deferred())
    return;

  if (request_->status().is_success()) {
    StartReading(false);  // Read the first chunk.
  } else {
    ResponseCompleted();
  }
}

void ResourceLoader::OnReadCompleted(net::URLRequest* unused, int bytes_read) {
  DCHECK_EQ(request_.get(), unused);
  VLOG(1) << "OnReadCompleted: \"" << request_->url().spec() << "\""
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
    // URLRequest reported an EOF. Call ResponseCompleted.
    DCHECK_EQ(0, bytes_read);
    ResponseCompleted();
  }
}

void ResourceLoader::CancelSSLRequest(const GlobalRequestID& id,
                                      int error,
                                      const net::SSLInfo* ssl_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

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

void ResourceLoader::ContinueSSLRequest(const GlobalRequestID& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DVLOG(1) << "ContinueSSLRequest() url: " << request_->url().spec();

  request_->ContinueDespiteLastError();
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
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ResourceLoader::ResumeReading,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    case DEFERRED_RESPONSE_COMPLETE:
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ResourceLoader::ResponseCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    case DEFERRED_FINISH:
      // Delay self-destruction since we don't know how we were reached.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ResourceLoader::CallDidFinishLoading,
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

  request_->Start();

  delegate_->DidStartRequest(this);
}

void ResourceLoader::CancelRequestInternal(int error, bool from_renderer) {
  VLOG(1) << "CancelRequestInternal: " << request_->url().spec();

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
  if (ssl_client_auth_handler_.get()) {
    ssl_client_auth_handler_->OnRequestCancelled();
    ssl_client_auth_handler_ = NULL;
  }

  request_->CancelWithError(error);

  if (!was_pending) {
    // If the request isn't in flight, then we won't get an asynchronous
    // notification from the request, so we have to signal ourselves to finish
    // this request.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ResourceLoader::ResponseCompleted,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ResourceLoader::StoreSignedCertificateTimestamps(
    const net::SignedCertificateTimestampAndStatusList& sct_list,
    int process_id,
    SignedCertificateTimestampIDStatusList* sct_ids) {
  SignedCertificateTimestampStore* sct_store(
      SignedCertificateTimestampStore::GetInstance());

  for (net::SignedCertificateTimestampAndStatusList::const_iterator iter =
       sct_list.begin(); iter != sct_list.end(); ++iter) {
    const int sct_id(sct_store->Store(iter->sct, process_id));
    sct_ids->push_back(
        SignedCertificateTimestampIDAndStatus(sct_id, iter->status));
  }
}

void ResourceLoader::CompleteResponseStarted() {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  scoped_refptr<ResourceResponse> response(new ResourceResponse());
  PopulateResourceResponse(request_.get(), response.get());

  if (request_->ssl_info().cert.get()) {
    int cert_id = CertStore::GetInstance()->StoreCert(
        request_->ssl_info().cert.get(), info->GetChildID());

    SignedCertificateTimestampIDStatusList signed_certificate_timestamp_ids;
    StoreSignedCertificateTimestamps(
        request_->ssl_info().signed_certificate_timestamps,
        info->GetChildID(),
        &signed_certificate_timestamp_ids);

    response->head.security_info = SerializeSecurityInfo(
        cert_id,
        request_->ssl_info().cert_status,
        request_->ssl_info().security_bits,
        request_->ssl_info().connection_status,
        signed_certificate_timestamp_ids);
  } else {
    // We should not have any SSL state.
    DCHECK(!request_->ssl_info().cert_status &&
           request_->ssl_info().security_bits == -1 &&
           !request_->ssl_info().connection_status);
  }

  delegate_->DidReceiveResponse(this);

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
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ResourceLoader::OnReadCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   request_.get(),
                   bytes_read));
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
  DCHECK(!is_deferred());

  // Make sure we track the buffer in at least one place.  This ensures it gets
  // deleted even in the case the request has already finished its job and
  // doesn't use the buffer.
  scoped_refptr<net::IOBuffer> buf;
  int buf_size;
  if (!handler_->OnWillRead(&buf, &buf_size, -1)) {
    Cancel();
    return;
  }

  DCHECK(buf);
  DCHECK(buf_size > 0);

  request_->Read(buf.get(), buf_size, bytes_read);

  // No need to check the return value here as we'll detect errors by
  // inspecting the URLRequest's status.
}

void ResourceLoader::CompleteRead(int bytes_read) {
  DCHECK(bytes_read >= 0);
  DCHECK(request_->status().is_success());

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
  VLOG(1) << "ResponseCompleted: " << request_->url().spec();
  RecordHistograms();
  ResourceRequestInfoImpl* info = GetRequestInfo();

  std::string security_info;
  const net::SSLInfo& ssl_info = request_->ssl_info();
  if (ssl_info.cert.get() != NULL) {
    int cert_id = CertStore::GetInstance()->StoreCert(ssl_info.cert.get(),
                                                      info->GetChildID());
    SignedCertificateTimestampIDStatusList signed_certificate_timestamp_ids;
    StoreSignedCertificateTimestamps(ssl_info.signed_certificate_timestamps,
                                     info->GetChildID(),
                                     &signed_certificate_timestamp_ids);

    security_info = SerializeSecurityInfo(
        cert_id, ssl_info.cert_status, ssl_info.security_bits,
        ssl_info.connection_status, signed_certificate_timestamp_ids);
  }

  bool defer = false;
  handler_->OnResponseCompleted(request_->status(), security_info, &defer);
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
  delegate_->DidFinishLoading(this);
}

void ResourceLoader::RecordHistograms() {
  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (info->GetResourceType() == ResourceType::PREFETCH) {
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
  }
}

}  // namespace content
