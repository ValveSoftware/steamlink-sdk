// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_delegate.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_manager.h"
#include "net/url_request/url_request_netlog_params.h"
#include "net/url_request/url_request_redirect_job.h"

using base::Time;
using std::string;

namespace net {

namespace {

// Max number of http redirects to follow.  Same number as gecko.
const int kMaxRedirects = 20;

// Discard headers which have meaning in POST (Content-Length, Content-Type,
// Origin).
void StripPostSpecificHeaders(HttpRequestHeaders* headers) {
  // These are headers that may be attached to a POST.
  headers->RemoveHeader(HttpRequestHeaders::kContentLength);
  headers->RemoveHeader(HttpRequestHeaders::kContentType);
  headers->RemoveHeader(HttpRequestHeaders::kOrigin);
}

// TODO(battre): Delete this, see http://crbug.com/89321:
// This counter keeps track of the identifiers used for URL requests so far.
// 0 is reserved to represent an invalid ID.
uint64 g_next_url_request_identifier = 1;

// This lock protects g_next_url_request_identifier.
base::LazyInstance<base::Lock>::Leaky
    g_next_url_request_identifier_lock = LAZY_INSTANCE_INITIALIZER;

// Returns an prior unused identifier for URL requests.
uint64 GenerateURLRequestIdentifier() {
  base::AutoLock lock(g_next_url_request_identifier_lock.Get());
  return g_next_url_request_identifier++;
}

// True once the first URLRequest was started.
bool g_url_requests_started = false;

// True if cookies are accepted by default.
bool g_default_can_use_cookies = true;

// When the URLRequest first assempts load timing information, it has the times
// at which each event occurred.  The API requires the time which the request
// was blocked on each phase.  This function handles the conversion.
//
// In the case of reusing a SPDY session, old proxy results may have been
// reused, so proxy resolution times may be before the request was started.
//
// Due to preconnect and late binding, it is also possible for the connection
// attempt to start before a request has been started, or proxy resolution
// completed.
//
// This functions fixes both those cases.
void ConvertRealLoadTimesToBlockingTimes(
    net::LoadTimingInfo* load_timing_info) {
  DCHECK(!load_timing_info->request_start.is_null());

  // Earliest time possible for the request to be blocking on connect events.
  base::TimeTicks block_on_connect = load_timing_info->request_start;

  if (!load_timing_info->proxy_resolve_start.is_null()) {
    DCHECK(!load_timing_info->proxy_resolve_end.is_null());

    // Make sure the proxy times are after request start.
    if (load_timing_info->proxy_resolve_start < load_timing_info->request_start)
      load_timing_info->proxy_resolve_start = load_timing_info->request_start;
    if (load_timing_info->proxy_resolve_end < load_timing_info->request_start)
      load_timing_info->proxy_resolve_end = load_timing_info->request_start;

    // Connect times must also be after the proxy times.
    block_on_connect = load_timing_info->proxy_resolve_end;
  }

  // Make sure connection times are after start and proxy times.

  net::LoadTimingInfo::ConnectTiming* connect_timing =
      &load_timing_info->connect_timing;
  if (!connect_timing->dns_start.is_null()) {
    DCHECK(!connect_timing->dns_end.is_null());
    if (connect_timing->dns_start < block_on_connect)
      connect_timing->dns_start = block_on_connect;
    if (connect_timing->dns_end < block_on_connect)
      connect_timing->dns_end = block_on_connect;
  }

  if (!connect_timing->connect_start.is_null()) {
    DCHECK(!connect_timing->connect_end.is_null());
    if (connect_timing->connect_start < block_on_connect)
      connect_timing->connect_start = block_on_connect;
    if (connect_timing->connect_end < block_on_connect)
      connect_timing->connect_end = block_on_connect;
  }

  if (!connect_timing->ssl_start.is_null()) {
    DCHECK(!connect_timing->ssl_end.is_null());
    if (connect_timing->ssl_start < block_on_connect)
      connect_timing->ssl_start = block_on_connect;
    if (connect_timing->ssl_end < block_on_connect)
      connect_timing->ssl_end = block_on_connect;
  }
}

}  // namespace

void URLRequest::Deprecated::RegisterRequestInterceptor(
    Interceptor* interceptor) {
  URLRequest::RegisterRequestInterceptor(interceptor);
}

void URLRequest::Deprecated::UnregisterRequestInterceptor(
    Interceptor* interceptor) {
  URLRequest::UnregisterRequestInterceptor(interceptor);
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest::Interceptor

URLRequestJob* URLRequest::Interceptor::MaybeInterceptRedirect(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const GURL& location) {
  return NULL;
}

URLRequestJob* URLRequest::Interceptor::MaybeInterceptResponse(
    URLRequest* request, NetworkDelegate* network_delegate) {
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest::Delegate

void URLRequest::Delegate::OnReceivedRedirect(URLRequest* request,
                                              const GURL& new_url,
                                              bool* defer_redirect) {
}

void URLRequest::Delegate::OnAuthRequired(URLRequest* request,
                                          AuthChallengeInfo* auth_info) {
  request->CancelAuth();
}

void URLRequest::Delegate::OnCertificateRequested(
    URLRequest* request,
    SSLCertRequestInfo* cert_request_info) {
  request->Cancel();
}

void URLRequest::Delegate::OnSSLCertificateError(URLRequest* request,
                                                 const SSLInfo& ssl_info,
                                                 bool is_hsts_ok) {
  request->Cancel();
}

void URLRequest::Delegate::OnBeforeNetworkStart(URLRequest* request,
                                                bool* defer) {
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest

URLRequest::URLRequest(const GURL& url,
                       RequestPriority priority,
                       Delegate* delegate,
                       const URLRequestContext* context)
    : identifier_(GenerateURLRequestIdentifier()) {
  Init(url, priority, delegate, context, NULL);
}

URLRequest::URLRequest(const GURL& url,
                       RequestPriority priority,
                       Delegate* delegate,
                       const URLRequestContext* context,
                       CookieStore* cookie_store)
    : identifier_(GenerateURLRequestIdentifier()) {
  Init(url, priority, delegate, context, cookie_store);
}

URLRequest::~URLRequest() {
  Cancel();

  if (network_delegate_) {
    network_delegate_->NotifyURLRequestDestroyed(this);
    if (job_.get())
      job_->NotifyURLRequestDestroyed();
  }

  if (job_.get())
    OrphanJob();

  int deleted = context_->url_requests()->erase(this);
  CHECK_EQ(1, deleted);

  int net_error = OK;
  // Log error only on failure, not cancellation, as even successful requests
  // are "cancelled" on destruction.
  if (status_.status() == URLRequestStatus::FAILED)
    net_error = status_.error();
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_REQUEST_ALIVE, net_error);
}

// static
void URLRequest::RegisterRequestInterceptor(Interceptor* interceptor) {
  URLRequestJobManager::GetInstance()->RegisterRequestInterceptor(interceptor);
}

// static
void URLRequest::UnregisterRequestInterceptor(Interceptor* interceptor) {
  URLRequestJobManager::GetInstance()->UnregisterRequestInterceptor(
      interceptor);
}

void URLRequest::Init(const GURL& url,
                      RequestPriority priority,
                      Delegate* delegate,
                      const URLRequestContext* context,
                      CookieStore* cookie_store) {
  context_ = context;
  network_delegate_ = context->network_delegate();
  net_log_ = BoundNetLog::Make(context->net_log(), NetLog::SOURCE_URL_REQUEST);
  url_chain_.push_back(url);
  method_ = "GET";
  referrer_policy_ = CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
  load_flags_ = LOAD_NORMAL;
  delegate_ = delegate;
  is_pending_ = false;
  is_redirecting_ = false;
  redirect_limit_ = kMaxRedirects;
  priority_ = priority;
  calling_delegate_ = false;
  use_blocked_by_as_load_param_ =false;
  before_request_callback_ = base::Bind(&URLRequest::BeforeRequestComplete,
                                        base::Unretained(this));
  has_notified_completion_ = false;
  received_response_content_length_ = 0;
  creation_time_ = base::TimeTicks::Now();
  notified_before_network_start_ = false;

  SIMPLE_STATS_COUNTER("URLRequestCount");

  // Sanity check out environment.
  DCHECK(base::MessageLoop::current())
      << "The current base::MessageLoop must exist";

  CHECK(context);
  context->url_requests()->insert(this);
  cookie_store_ = cookie_store;
  if (cookie_store_ == NULL)
    cookie_store_ = context->cookie_store();

  net_log_.BeginEvent(NetLog::TYPE_REQUEST_ALIVE);
}

void URLRequest::EnableChunkedUpload() {
  DCHECK(!upload_data_stream_ || upload_data_stream_->is_chunked());
  if (!upload_data_stream_) {
    upload_data_stream_.reset(
        new UploadDataStream(UploadDataStream::CHUNKED, 0));
  }
}

void URLRequest::AppendChunkToUpload(const char* bytes,
                                     int bytes_len,
                                     bool is_last_chunk) {
  DCHECK(upload_data_stream_);
  DCHECK(upload_data_stream_->is_chunked());
  DCHECK_GT(bytes_len, 0);
  upload_data_stream_->AppendChunk(bytes, bytes_len, is_last_chunk);
}

void URLRequest::set_upload(scoped_ptr<UploadDataStream> upload) {
  DCHECK(!upload->is_chunked());
  upload_data_stream_ = upload.Pass();
}

const UploadDataStream* URLRequest::get_upload() const {
  return upload_data_stream_.get();
}

bool URLRequest::has_upload() const {
  return upload_data_stream_.get() != NULL;
}

void URLRequest::SetExtraRequestHeaderById(int id, const string& value,
                                           bool overwrite) {
  DCHECK(!is_pending_ || is_redirecting_);
  NOTREACHED() << "implement me!";
}

void URLRequest::SetExtraRequestHeaderByName(const string& name,
                                             const string& value,
                                             bool overwrite) {
  DCHECK(!is_pending_ || is_redirecting_);
  if (overwrite) {
    extra_request_headers_.SetHeader(name, value);
  } else {
    extra_request_headers_.SetHeaderIfMissing(name, value);
  }
}

void URLRequest::RemoveRequestHeaderByName(const string& name) {
  DCHECK(!is_pending_ || is_redirecting_);
  extra_request_headers_.RemoveHeader(name);
}

void URLRequest::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  DCHECK(!is_pending_);
  extra_request_headers_ = headers;

  // NOTE: This method will likely become non-trivial once the other setters
  // for request headers are implemented.
}

bool URLRequest::GetFullRequestHeaders(HttpRequestHeaders* headers) const {
  if (!job_.get())
    return false;

  return job_->GetFullRequestHeaders(headers);
}

int64 URLRequest::GetTotalReceivedBytes() const {
  if (!job_.get())
    return 0;

  return job_->GetTotalReceivedBytes();
}

LoadStateWithParam URLRequest::GetLoadState() const {
  // The !blocked_by_.empty() check allows |this| to report it's blocked on a
  // delegate before it has been started.
  if (calling_delegate_ || !blocked_by_.empty()) {
    return LoadStateWithParam(
        LOAD_STATE_WAITING_FOR_DELEGATE,
        use_blocked_by_as_load_param_ ? base::UTF8ToUTF16(blocked_by_) :
                                        base::string16());
  }
  return LoadStateWithParam(job_.get() ? job_->GetLoadState() : LOAD_STATE_IDLE,
                            base::string16());
}

base::Value* URLRequest::GetStateAsValue() const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("url", original_url().possibly_invalid_spec());

  if (url_chain_.size() > 1) {
    base::ListValue* list = new base::ListValue();
    for (std::vector<GURL>::const_iterator url = url_chain_.begin();
         url != url_chain_.end(); ++url) {
      list->AppendString(url->possibly_invalid_spec());
    }
    dict->Set("url_chain", list);
  }

  dict->SetInteger("load_flags", load_flags_);

  LoadStateWithParam load_state = GetLoadState();
  dict->SetInteger("load_state", load_state.state);
  if (!load_state.param.empty())
    dict->SetString("load_state_param", load_state.param);
  if (!blocked_by_.empty())
    dict->SetString("delegate_info", blocked_by_);

  dict->SetString("method", method_);
  dict->SetBoolean("has_upload", has_upload());
  dict->SetBoolean("is_pending", is_pending_);

  // Add the status of the request.  The status should always be IO_PENDING, and
  // the error should always be OK, unless something is holding onto a request
  // that has finished or a request was leaked.  Neither of these should happen.
  switch (status_.status()) {
    case URLRequestStatus::SUCCESS:
      dict->SetString("status", "SUCCESS");
      break;
    case URLRequestStatus::IO_PENDING:
      dict->SetString("status", "IO_PENDING");
      break;
    case URLRequestStatus::CANCELED:
      dict->SetString("status", "CANCELED");
      break;
    case URLRequestStatus::FAILED:
      dict->SetString("status", "FAILED");
      break;
  }
  if (status_.error() != OK)
    dict->SetInteger("net_error", status_.error());
  return dict;
}

void URLRequest::LogBlockedBy(const char* blocked_by) {
  DCHECK(blocked_by);
  DCHECK_GT(strlen(blocked_by), 0u);

  // Only log information to NetLog during startup and certain deferring calls
  // to delegates.  For all reads but the first, do nothing.
  if (!calling_delegate_ && !response_info_.request_time.is_null())
    return;

  LogUnblocked();
  blocked_by_ = blocked_by;
  use_blocked_by_as_load_param_ = false;

  net_log_.BeginEvent(
      NetLog::TYPE_DELEGATE_INFO,
      NetLog::StringCallback("delegate_info", &blocked_by_));
}

void URLRequest::LogAndReportBlockedBy(const char* source) {
  LogBlockedBy(source);
  use_blocked_by_as_load_param_ = true;
}

void URLRequest::LogUnblocked() {
  if (blocked_by_.empty())
    return;

  net_log_.EndEvent(NetLog::TYPE_DELEGATE_INFO);
  blocked_by_.clear();
}

UploadProgress URLRequest::GetUploadProgress() const {
  if (!job_.get()) {
    // We haven't started or the request was cancelled
    return UploadProgress();
  }
  if (final_upload_progress_.position()) {
    // The first job completed and none of the subsequent series of
    // GETs when following redirects will upload anything, so we return the
    // cached results from the initial job, the POST.
    return final_upload_progress_;
  }
  return job_->GetUploadProgress();
}

void URLRequest::GetResponseHeaderById(int id, string* value) {
  DCHECK(job_.get());
  NOTREACHED() << "implement me!";
}

void URLRequest::GetResponseHeaderByName(const string& name, string* value) {
  DCHECK(value);
  if (response_info_.headers.get()) {
    response_info_.headers->GetNormalizedHeader(name, value);
  } else {
    value->clear();
  }
}

void URLRequest::GetAllResponseHeaders(string* headers) {
  DCHECK(headers);
  if (response_info_.headers.get()) {
    response_info_.headers->GetNormalizedHeaders(headers);
  } else {
    headers->clear();
  }
}

HostPortPair URLRequest::GetSocketAddress() const {
  DCHECK(job_.get());
  return job_->GetSocketAddress();
}

HttpResponseHeaders* URLRequest::response_headers() const {
  return response_info_.headers.get();
}

void URLRequest::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  *load_timing_info = load_timing_info_;
}

bool URLRequest::GetResponseCookies(ResponseCookies* cookies) {
  DCHECK(job_.get());
  return job_->GetResponseCookies(cookies);
}

void URLRequest::GetMimeType(string* mime_type) {
  DCHECK(job_.get());
  job_->GetMimeType(mime_type);
}

void URLRequest::GetCharset(string* charset) {
  DCHECK(job_.get());
  job_->GetCharset(charset);
}

int URLRequest::GetResponseCode() const {
  DCHECK(job_.get());
  return job_->GetResponseCode();
}

void URLRequest::SetLoadFlags(int flags) {
  if ((load_flags_ & LOAD_IGNORE_LIMITS) != (flags & LOAD_IGNORE_LIMITS)) {
    DCHECK(!job_);
    DCHECK(flags & LOAD_IGNORE_LIMITS);
    DCHECK_EQ(priority_, MAXIMUM_PRIORITY);
  }
  load_flags_ = flags;

  // This should be a no-op given the above DCHECKs, but do this
  // anyway for release mode.
  if ((load_flags_ & LOAD_IGNORE_LIMITS) != 0)
    SetPriority(MAXIMUM_PRIORITY);
}

// static
void URLRequest::SetDefaultCookiePolicyToBlock() {
  CHECK(!g_url_requests_started);
  g_default_can_use_cookies = false;
}

// static
bool URLRequest::IsHandledProtocol(const std::string& scheme) {
  return URLRequestJobManager::SupportsScheme(scheme);
}

// static
bool URLRequest::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  return IsHandledProtocol(url.scheme());
}

void URLRequest::set_first_party_for_cookies(
    const GURL& first_party_for_cookies) {
  first_party_for_cookies_ = first_party_for_cookies;
}

void URLRequest::set_method(const std::string& method) {
  DCHECK(!is_pending_);
  method_ = method;
}

// static
std::string URLRequest::ComputeMethodForRedirect(
    const std::string& method,
    int http_status_code) {
  // For 303 redirects, all request methods except HEAD are converted to GET,
  // as per the latest httpbis draft.  The draft also allows POST requests to
  // be converted to GETs when following 301/302 redirects, for historical
  // reasons. Most major browsers do this and so shall we.  Both RFC 2616 and
  // the httpbis draft say to prompt the user to confirm the generation of new
  // requests, other than GET and HEAD requests, but IE omits these prompts and
  // so shall we.
  // See:  https://tools.ietf.org/html/draft-ietf-httpbis-p2-semantics-17#section-7.3
  if ((http_status_code == 303 && method != "HEAD") ||
      ((http_status_code == 301 || http_status_code == 302) &&
       method == "POST")) {
    return "GET";
  }
  return method;
}

void URLRequest::SetReferrer(const std::string& referrer) {
  DCHECK(!is_pending_);
  GURL referrer_url(referrer);
  UMA_HISTOGRAM_BOOLEAN("Net.URLRequest_SetReferrer_IsEmptyOrValid",
                        referrer_url.is_empty() || referrer_url.is_valid());
  if (referrer_url.is_valid()) {
    referrer_ = referrer_url.GetAsReferrer().spec();
  } else {
    referrer_ = referrer;
  }
}

void URLRequest::set_referrer_policy(ReferrerPolicy referrer_policy) {
  DCHECK(!is_pending_);
  referrer_policy_ = referrer_policy;
}

void URLRequest::set_delegate(Delegate* delegate) {
  delegate_ = delegate;
}

void URLRequest::Start() {
  // Some values can be NULL, but the job factory must not be.
  DCHECK(context_->job_factory());

  DCHECK_EQ(network_delegate_, context_->network_delegate());
  // Anything that sets |blocked_by_| before start should have cleaned up after
  // itself.
  DCHECK(blocked_by_.empty());

  g_url_requests_started = true;
  response_info_.request_time = base::Time::Now();

  load_timing_info_ = LoadTimingInfo();
  load_timing_info_.request_start_time = response_info_.request_time;
  load_timing_info_.request_start = base::TimeTicks::Now();

  // Only notify the delegate for the initial request.
  if (network_delegate_) {
    OnCallToDelegate();
    int error = network_delegate_->NotifyBeforeURLRequest(
        this, before_request_callback_, &delegate_redirect_url_);
    // If ERR_IO_PENDING is returned, the delegate will invoke
    // |before_request_callback_| later.
    if (error != ERR_IO_PENDING)
      BeforeRequestComplete(error);
    return;
  }

  StartJob(URLRequestJobManager::GetInstance()->CreateJob(
      this, network_delegate_));
}

///////////////////////////////////////////////////////////////////////////////

void URLRequest::BeforeRequestComplete(int error) {
  DCHECK(!job_.get());
  DCHECK_NE(ERR_IO_PENDING, error);
  DCHECK_EQ(network_delegate_, context_->network_delegate());

  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, status_.status());

  OnCallToDelegateComplete();

  if (error != OK) {
    std::string source("delegate");
    net_log_.AddEvent(NetLog::TYPE_CANCELLED,
                      NetLog::StringCallback("source", &source));
    StartJob(new URLRequestErrorJob(this, network_delegate_, error));
  } else if (!delegate_redirect_url_.is_empty()) {
    GURL new_url;
    new_url.Swap(&delegate_redirect_url_);

    URLRequestRedirectJob* job = new URLRequestRedirectJob(
        this, network_delegate_, new_url,
        // Use status code 307 to preserve the method, so POST requests work.
        URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT, "Delegate");
    StartJob(job);
  } else {
    StartJob(URLRequestJobManager::GetInstance()->CreateJob(
        this, network_delegate_));
  }
}

void URLRequest::StartJob(URLRequestJob* job) {
  DCHECK(!is_pending_);
  DCHECK(!job_.get());

  net_log_.BeginEvent(
      NetLog::TYPE_URL_REQUEST_START_JOB,
      base::Bind(&NetLogURLRequestStartCallback,
                 &url(), &method_, load_flags_, priority_,
                 upload_data_stream_ ? upload_data_stream_->identifier() : -1));

  job_ = job;
  job_->SetExtraRequestHeaders(extra_request_headers_);
  job_->SetPriority(priority_);

  if (upload_data_stream_.get())
    job_->SetUpload(upload_data_stream_.get());

  is_pending_ = true;
  is_redirecting_ = false;

  response_info_.was_cached = false;

  // If the referrer is secure, but the requested URL is not, the referrer
  // policy should be something non-default. If you hit this, please file a
  // bug.
  if (referrer_policy_ ==
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE &&
      GURL(referrer_).SchemeIsSecure() && !url().SchemeIsSecure()) {
#if !defined(OFFICIAL_BUILD)
    LOG(FATAL) << "Trying to send secure referrer for insecure load";
#endif
    referrer_.clear();
    base::RecordAction(
        base::UserMetricsAction("Net.URLRequest_StartJob_InvalidReferrer"));
  }

  // Don't allow errors to be sent from within Start().
  // TODO(brettw) this may cause NotifyDone to be sent synchronously,
  // we probably don't want this: they should be sent asynchronously so
  // the caller does not get reentered.
  job_->Start();
}

void URLRequest::Restart() {
  // Should only be called if the original job didn't make any progress.
  DCHECK(job_.get() && !job_->has_response_started());
  RestartWithJob(
      URLRequestJobManager::GetInstance()->CreateJob(this, network_delegate_));
}

void URLRequest::RestartWithJob(URLRequestJob *job) {
  DCHECK(job->request() == this);
  PrepareToRestart();
  StartJob(job);
}

void URLRequest::Cancel() {
  DoCancel(ERR_ABORTED, SSLInfo());
}

void URLRequest::CancelWithError(int error) {
  DoCancel(error, SSLInfo());
}

void URLRequest::CancelWithSSLError(int error, const SSLInfo& ssl_info) {
  // This should only be called on a started request.
  if (!is_pending_ || !job_.get() || job_->has_response_started()) {
    NOTREACHED();
    return;
  }
  DoCancel(error, ssl_info);
}

void URLRequest::DoCancel(int error, const SSLInfo& ssl_info) {
  DCHECK(error < 0);
  // If cancelled while calling a delegate, clear delegate info.
  if (calling_delegate_) {
    LogUnblocked();
    OnCallToDelegateComplete();
  }

  // If the URL request already has an error status, then canceling is a no-op.
  // Plus, we don't want to change the error status once it has been set.
  if (status_.is_success()) {
    status_.set_status(URLRequestStatus::CANCELED);
    status_.set_error(error);
    response_info_.ssl_info = ssl_info;

    // If the request hasn't already been completed, log a cancellation event.
    if (!has_notified_completion_) {
      // Don't log an error code on ERR_ABORTED, since that's redundant.
      net_log_.AddEventWithNetErrorCode(NetLog::TYPE_CANCELLED,
                                        error == ERR_ABORTED ? OK : error);
    }
  }

  if (is_pending_ && job_.get())
    job_->Kill();

  // We need to notify about the end of this job here synchronously. The
  // Job sends an asynchronous notification but by the time this is processed,
  // our |context_| is NULL.
  NotifyRequestCompleted();

  // The Job will call our NotifyDone method asynchronously.  This is done so
  // that the Delegate implementation can call Cancel without having to worry
  // about being called recursively.
}

bool URLRequest::Read(IOBuffer* dest, int dest_size, int* bytes_read) {
  DCHECK(job_.get());
  DCHECK(bytes_read);
  *bytes_read = 0;

  // If this is the first read, end the delegate call that may have started in
  // OnResponseStarted.
  OnCallToDelegateComplete();

  // This handles a cancel that happens while paused.
  // TODO(ahendrickson): DCHECK() that it is not done after
  // http://crbug.com/115705 is fixed.
  if (job_->is_done())
    return false;

  if (dest_size == 0) {
    // Caller is not too bright.  I guess we've done what they asked.
    return true;
  }

  // Once the request fails or is cancelled, read will just return 0 bytes
  // to indicate end of stream.
  if (!status_.is_success()) {
    return true;
  }

  bool rv = job_->Read(dest, dest_size, bytes_read);
  // If rv is false, the status cannot be success.
  DCHECK(rv || status_.status() != URLRequestStatus::SUCCESS);
  if (rv && *bytes_read <= 0 && status_.is_success())
    NotifyRequestCompleted();
  return rv;
}

void URLRequest::StopCaching() {
  DCHECK(job_.get());
  job_->StopCaching();
}

void URLRequest::NotifyReceivedRedirect(const GURL& location,
                                        bool* defer_redirect) {
  is_redirecting_ = true;

  URLRequestJob* job =
      URLRequestJobManager::GetInstance()->MaybeInterceptRedirect(
          this, network_delegate_, location);
  if (job) {
    RestartWithJob(job);
  } else if (delegate_) {
    OnCallToDelegate();
    delegate_->OnReceivedRedirect(this, location, defer_redirect);
    // |this| may be have been destroyed here.
  }
}

void URLRequest::NotifyBeforeNetworkStart(bool* defer) {
  if (delegate_ && !notified_before_network_start_) {
    OnCallToDelegate();
    delegate_->OnBeforeNetworkStart(this, defer);
    if (!*defer)
      OnCallToDelegateComplete();
    notified_before_network_start_ = true;
  }
}

void URLRequest::ResumeNetworkStart() {
  DCHECK(job_);
  DCHECK(notified_before_network_start_);

  OnCallToDelegateComplete();
  job_->ResumeNetworkStart();
}

void URLRequest::NotifyResponseStarted() {
  int net_error = OK;
  if (!status_.is_success())
    net_error = status_.error();
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_URL_REQUEST_START_JOB,
                                    net_error);

  URLRequestJob* job =
      URLRequestJobManager::GetInstance()->MaybeInterceptResponse(
          this, network_delegate_);
  if (job) {
    RestartWithJob(job);
  } else {
    if (delegate_) {
      // In some cases (e.g. an event was canceled), we might have sent the
      // completion event and receive a NotifyResponseStarted() later.
      if (!has_notified_completion_ && status_.is_success()) {
        if (network_delegate_)
          network_delegate_->NotifyResponseStarted(this);
      }

      // Notify in case the entire URL Request has been finished.
      if (!has_notified_completion_ && !status_.is_success())
        NotifyRequestCompleted();

      OnCallToDelegate();
      delegate_->OnResponseStarted(this);
      // Nothing may appear below this line as OnResponseStarted may delete
      // |this|.
    }
  }
}

void URLRequest::FollowDeferredRedirect() {
  CHECK(job_.get());
  CHECK(status_.is_success());

  job_->FollowDeferredRedirect();
}

void URLRequest::SetAuth(const AuthCredentials& credentials) {
  DCHECK(job_.get());
  DCHECK(job_->NeedsAuth());

  job_->SetAuth(credentials);
}

void URLRequest::CancelAuth() {
  DCHECK(job_.get());
  DCHECK(job_->NeedsAuth());

  job_->CancelAuth();
}

void URLRequest::ContinueWithCertificate(X509Certificate* client_cert) {
  DCHECK(job_.get());

  job_->ContinueWithCertificate(client_cert);
}

void URLRequest::ContinueDespiteLastError() {
  DCHECK(job_.get());

  job_->ContinueDespiteLastError();
}

void URLRequest::PrepareToRestart() {
  DCHECK(job_.get());

  // Close the current URL_REQUEST_START_JOB, since we will be starting a new
  // one.
  net_log_.EndEvent(NetLog::TYPE_URL_REQUEST_START_JOB);

  OrphanJob();

  response_info_ = HttpResponseInfo();
  response_info_.request_time = base::Time::Now();

  load_timing_info_ = LoadTimingInfo();
  load_timing_info_.request_start_time = response_info_.request_time;
  load_timing_info_.request_start = base::TimeTicks::Now();

  status_ = URLRequestStatus();
  is_pending_ = false;
}

void URLRequest::OrphanJob() {
  // When calling this function, please check that URLRequestHttpJob is
  // not in between calling NetworkDelegate::NotifyHeadersReceived receiving
  // the call back. This is currently guaranteed by the following strategies:
  // - OrphanJob is called on JobRestart, in this case the URLRequestJob cannot
  //   be receiving any headers at that time.
  // - OrphanJob is called in ~URLRequest, in this case
  //   NetworkDelegate::NotifyURLRequestDestroyed notifies the NetworkDelegate
  //   that the callback becomes invalid.
  job_->Kill();
  job_->DetachRequest();  // ensures that the job will not call us again
  job_ = NULL;
}

int URLRequest::Redirect(const GURL& location, int http_status_code) {
  // Matches call in NotifyReceivedRedirect.
  OnCallToDelegateComplete();
  if (net_log_.IsLogging()) {
    net_log_.AddEvent(
        NetLog::TYPE_URL_REQUEST_REDIRECTED,
        NetLog::StringCallback("location", &location.possibly_invalid_spec()));
  }

  if (network_delegate_)
    network_delegate_->NotifyBeforeRedirect(this, location);

  if (redirect_limit_ <= 0) {
    DVLOG(1) << "disallowing redirect: exceeds limit";
    return ERR_TOO_MANY_REDIRECTS;
  }

  if (!location.is_valid())
    return ERR_INVALID_URL;

  if (!job_->IsSafeRedirect(location)) {
    DVLOG(1) << "disallowing redirect: unsafe protocol";
    return ERR_UNSAFE_REDIRECT;
  }

  if (!final_upload_progress_.position())
    final_upload_progress_ = job_->GetUploadProgress();
  PrepareToRestart();

  std::string new_method(ComputeMethodForRedirect(method_, http_status_code));
  if (new_method != method_) {
    if (method_ == "POST") {
      // If being switched from POST, must remove headers that were specific to
      // the POST and don't have meaning in other methods. For example the
      // inclusion of a multipart Content-Type header in GET can cause problems
      // with some servers:
      // http://code.google.com/p/chromium/issues/detail?id=843
      StripPostSpecificHeaders(&extra_request_headers_);
    }
    upload_data_stream_.reset();
    method_.swap(new_method);
  }

  // Suppress the referrer if we're redirecting out of https.
  if (referrer_policy_ ==
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE &&
      GURL(referrer_).SchemeIsSecure() && !location.SchemeIsSecure()) {
    referrer_.clear();
  }

  url_chain_.push_back(location);
  --redirect_limit_;

  Start();
  return OK;
}

const URLRequestContext* URLRequest::context() const {
  return context_;
}

int64 URLRequest::GetExpectedContentSize() const {
  int64 expected_content_size = -1;
  if (job_.get())
    expected_content_size = job_->expected_content_size();

  return expected_content_size;
}

void URLRequest::SetPriority(RequestPriority priority) {
  DCHECK_GE(priority, MINIMUM_PRIORITY);
  DCHECK_LE(priority, MAXIMUM_PRIORITY);

  if ((load_flags_ & LOAD_IGNORE_LIMITS) && (priority != MAXIMUM_PRIORITY)) {
    NOTREACHED();
    // Maintain the invariant that requests with IGNORE_LIMITS set
    // have MAXIMUM_PRIORITY for release mode.
    return;
  }

  if (priority_ == priority)
    return;

  priority_ = priority;
  if (job_.get()) {
    net_log_.AddEvent(NetLog::TYPE_URL_REQUEST_SET_PRIORITY,
                      NetLog::IntegerCallback("priority", priority_));
    job_->SetPriority(priority_);
  }
}

bool URLRequest::GetHSTSRedirect(GURL* redirect_url) const {
  const GURL& url = this->url();
  if (!url.SchemeIs("http"))
    return false;
  TransportSecurityState* state = context()->transport_security_state();
  if (state &&
      state->ShouldUpgradeToSSL(
          url.host(),
          SSLConfigService::IsSNIAvailable(context()->ssl_config_service()))) {
    url::Replacements<char> replacements;
    const char kNewScheme[] = "https";
    replacements.SetScheme(kNewScheme, url::Component(0, strlen(kNewScheme)));
    *redirect_url = url.ReplaceComponents(replacements);
    return true;
  }
  return false;
}

void URLRequest::NotifyAuthRequired(AuthChallengeInfo* auth_info) {
  NetworkDelegate::AuthRequiredResponse rv =
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  auth_info_ = auth_info;
  if (network_delegate_) {
    OnCallToDelegate();
    rv = network_delegate_->NotifyAuthRequired(
        this,
        *auth_info,
        base::Bind(&URLRequest::NotifyAuthRequiredComplete,
                   base::Unretained(this)),
        &auth_credentials_);
    if (rv == NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING)
      return;
  }

  NotifyAuthRequiredComplete(rv);
}

void URLRequest::NotifyAuthRequiredComplete(
    NetworkDelegate::AuthRequiredResponse result) {
  OnCallToDelegateComplete();

  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, status_.status());

  // NotifyAuthRequired may be called multiple times, such as
  // when an authentication attempt fails. Clear out the data
  // so it can be reset on another round.
  AuthCredentials credentials = auth_credentials_;
  auth_credentials_ = AuthCredentials();
  scoped_refptr<AuthChallengeInfo> auth_info;
  auth_info.swap(auth_info_);

  switch (result) {
    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION:
      // Defer to the URLRequest::Delegate, since the NetworkDelegate
      // didn't take an action.
      if (delegate_)
        delegate_->OnAuthRequired(this, auth_info.get());
      break;

    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH:
      SetAuth(credentials);
      break;

    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH:
      CancelAuth();
      break;

    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING:
      NOTREACHED();
      break;
  }
}

void URLRequest::NotifyCertificateRequested(
    SSLCertRequestInfo* cert_request_info) {
  if (delegate_)
    delegate_->OnCertificateRequested(this, cert_request_info);
}

void URLRequest::NotifySSLCertificateError(const SSLInfo& ssl_info,
                                           bool fatal) {
  if (delegate_)
    delegate_->OnSSLCertificateError(this, ssl_info, fatal);
}

bool URLRequest::CanGetCookies(const CookieList& cookie_list) const {
  DCHECK(!(load_flags_ & LOAD_DO_NOT_SEND_COOKIES));
  if (network_delegate_) {
    return network_delegate_->CanGetCookies(*this, cookie_list);
  }
  return g_default_can_use_cookies;
}

bool URLRequest::CanSetCookie(const std::string& cookie_line,
                              CookieOptions* options) const {
  DCHECK(!(load_flags_ & LOAD_DO_NOT_SAVE_COOKIES));
  if (network_delegate_) {
    return network_delegate_->CanSetCookie(*this, cookie_line, options);
  }
  return g_default_can_use_cookies;
}

bool URLRequest::CanEnablePrivacyMode() const {
  if (network_delegate_) {
    return network_delegate_->CanEnablePrivacyMode(url(),
                                                   first_party_for_cookies_);
  }
  return !g_default_can_use_cookies;
}


void URLRequest::NotifyReadCompleted(int bytes_read) {
  // Notify in case the entire URL Request has been finished.
  if (bytes_read <= 0)
    NotifyRequestCompleted();

  // Notify NetworkChangeNotifier that we just received network data.
  // This is to identify cases where the NetworkChangeNotifier thinks we
  // are off-line but we are still receiving network data (crbug.com/124069),
  // and to get rough network connection measurements.
  if (bytes_read > 0 && !was_cached())
    NetworkChangeNotifier::NotifyDataReceived(*this, bytes_read);

  if (delegate_)
    delegate_->OnReadCompleted(this, bytes_read);

  // Nothing below this line as OnReadCompleted may delete |this|.
}

void URLRequest::OnHeadersComplete() {
  // Cache load timing information now, as information will be lost once the
  // socket is closed and the ClientSocketHandle is Reset, which will happen
  // once the body is complete.  The start times should already be populated.
  if (job_.get()) {
    // Keep a copy of the two times the URLRequest sets.
    base::TimeTicks request_start = load_timing_info_.request_start;
    base::Time request_start_time = load_timing_info_.request_start_time;

    // Clear load times.  Shouldn't be neded, but gives the GetLoadTimingInfo a
    // consistent place to start from.
    load_timing_info_ = LoadTimingInfo();
    job_->GetLoadTimingInfo(&load_timing_info_);

    load_timing_info_.request_start = request_start;
    load_timing_info_.request_start_time = request_start_time;

    ConvertRealLoadTimesToBlockingTimes(&load_timing_info_);
  }
}

void URLRequest::NotifyRequestCompleted() {
  // TODO(battre): Get rid of this check, according to willchan it should
  // not be needed.
  if (has_notified_completion_)
    return;

  is_pending_ = false;
  is_redirecting_ = false;
  has_notified_completion_ = true;
  if (network_delegate_)
    network_delegate_->NotifyCompleted(this, job_.get() != NULL);
}

void URLRequest::OnCallToDelegate() {
  DCHECK(!calling_delegate_);
  DCHECK(blocked_by_.empty());
  calling_delegate_ = true;
  net_log_.BeginEvent(NetLog::TYPE_URL_REQUEST_DELEGATE);
}

void URLRequest::OnCallToDelegateComplete() {
  // This should have been cleared before resuming the request.
  DCHECK(blocked_by_.empty());
  if (!calling_delegate_)
    return;
  calling_delegate_ = false;
  net_log_.EndEvent(NetLog::TYPE_URL_REQUEST_DELEGATE);
}

void URLRequest::set_stack_trace(const base::debug::StackTrace& stack_trace) {
  base::debug::StackTrace* stack_trace_copy =
      new base::debug::StackTrace(NULL, 0);
  *stack_trace_copy = stack_trace;
  stack_trace_.reset(stack_trace_copy);
}

const base::debug::StackTrace* URLRequest::stack_trace() const {
  return stack_trace_.get();
}

}  // namespace net
