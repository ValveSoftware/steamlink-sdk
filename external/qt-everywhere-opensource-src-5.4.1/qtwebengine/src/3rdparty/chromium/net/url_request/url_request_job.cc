// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/filter/filter.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace net {

URLRequestJob::URLRequestJob(URLRequest* request,
                             NetworkDelegate* network_delegate)
    : request_(request),
      done_(false),
      prefilter_bytes_read_(0),
      postfilter_bytes_read_(0),
      filter_input_byte_count_(0),
      filter_needs_more_output_space_(false),
      filtered_read_buffer_len_(0),
      has_handled_response_(false),
      expected_content_size_(-1),
      deferred_redirect_status_code_(-1),
      network_delegate_(network_delegate),
      weak_factory_(this) {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->AddObserver(this);
}

void URLRequestJob::SetUpload(UploadDataStream* upload) {
}

void URLRequestJob::SetExtraRequestHeaders(const HttpRequestHeaders& headers) {
}

void URLRequestJob::SetPriority(RequestPriority priority) {
}

void URLRequestJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  // Make sure the request is notified that we are done.  We assume that the
  // request took care of setting its error status before calling Kill.
  if (request_)
    NotifyCanceled();
}

void URLRequestJob::DetachRequest() {
  request_ = NULL;
}

// This function calls ReadData to get stream data. If a filter exists, passes
// the data to the attached filter. Then returns the output from filter back to
// the caller.
bool URLRequestJob::Read(IOBuffer* buf, int buf_size, int *bytes_read) {
  bool rv = false;

  DCHECK_LT(buf_size, 1000000);  // Sanity check.
  DCHECK(buf);
  DCHECK(bytes_read);
  DCHECK(filtered_read_buffer_.get() == NULL);
  DCHECK_EQ(0, filtered_read_buffer_len_);

  *bytes_read = 0;

  // Skip Filter if not present.
  if (!filter_.get()) {
    rv = ReadRawDataHelper(buf, buf_size, bytes_read);
  } else {
    // Save the caller's buffers while we do IO
    // in the filter's buffers.
    filtered_read_buffer_ = buf;
    filtered_read_buffer_len_ = buf_size;

    if (ReadFilteredData(bytes_read)) {
      rv = true;   // We have data to return.

      // It is fine to call DoneReading even if ReadFilteredData receives 0
      // bytes from the net, but we avoid making that call if we know for
      // sure that's the case (ReadRawDataHelper path).
      if (*bytes_read == 0)
        DoneReading();
    } else {
      rv = false;  // Error, or a new IO is pending.
    }
  }
  if (rv && *bytes_read == 0)
    NotifyDone(URLRequestStatus());
  return rv;
}

void URLRequestJob::StopCaching() {
  // Nothing to do here.
}

bool URLRequestJob::GetFullRequestHeaders(HttpRequestHeaders* headers) const {
  // Most job types don't send request headers.
  return false;
}

int64 URLRequestJob::GetTotalReceivedBytes() const {
  return 0;
}

LoadState URLRequestJob::GetLoadState() const {
  return LOAD_STATE_IDLE;
}

UploadProgress URLRequestJob::GetUploadProgress() const {
  return UploadProgress();
}

bool URLRequestJob::GetCharset(std::string* charset) {
  return false;
}

void URLRequestJob::GetResponseInfo(HttpResponseInfo* info) {
}

void URLRequestJob::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  // Only certain request types return more than just request start times.
}

bool URLRequestJob::GetResponseCookies(std::vector<std::string>* cookies) {
  return false;
}

Filter* URLRequestJob::SetupFilter() const {
  return NULL;
}

bool URLRequestJob::IsRedirectResponse(GURL* location,
                                       int* http_status_code) {
  // For non-HTTP jobs, headers will be null.
  HttpResponseHeaders* headers = request_->response_headers();
  if (!headers)
    return false;

  std::string value;
  if (!headers->IsRedirect(&value))
    return false;

  *location = request_->url().Resolve(value);
  *http_status_code = headers->response_code();
  return true;
}

bool URLRequestJob::CopyFragmentOnRedirect(const GURL& location) const {
  return true;
}

bool URLRequestJob::IsSafeRedirect(const GURL& location) {
  return true;
}

bool URLRequestJob::NeedsAuth() {
  return false;
}

void URLRequestJob::GetAuthChallengeInfo(
    scoped_refptr<AuthChallengeInfo>* auth_info) {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::SetAuth(const AuthCredentials& credentials) {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::CancelAuth() {
  // This will only be called if NeedsAuth() returns true, in which
  // case the derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::ContinueWithCertificate(
    X509Certificate* client_cert) {
  // The derived class should implement this!
  NOTREACHED();
}

void URLRequestJob::ContinueDespiteLastError() {
  // Implementations should know how to recover from errors they generate.
  // If this code was reached, we are trying to recover from an error that
  // we don't know how to recover from.
  NOTREACHED();
}

void URLRequestJob::FollowDeferredRedirect() {
  DCHECK(deferred_redirect_status_code_ != -1);

  // NOTE: deferred_redirect_url_ may be invalid, and attempting to redirect to
  // such an URL will fail inside FollowRedirect.  The DCHECK above asserts
  // that we called OnReceivedRedirect.

  // It is also possible that FollowRedirect will drop the last reference to
  // this job, so we need to reset our members before calling it.

  GURL redirect_url = deferred_redirect_url_;
  int redirect_status_code = deferred_redirect_status_code_;

  deferred_redirect_url_ = GURL();
  deferred_redirect_status_code_ = -1;

  FollowRedirect(redirect_url, redirect_status_code);
}

void URLRequestJob::ResumeNetworkStart() {
  // This should only be called for HTTP Jobs, and implemented in the derived
  // class.
  NOTREACHED();
}

bool URLRequestJob::GetMimeType(std::string* mime_type) const {
  return false;
}

int URLRequestJob::GetResponseCode() const {
  return -1;
}

HostPortPair URLRequestJob::GetSocketAddress() const {
  return HostPortPair();
}

void URLRequestJob::OnSuspend() {
  Kill();
}

void URLRequestJob::NotifyURLRequestDestroyed() {
}

URLRequestJob::~URLRequestJob() {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->RemoveObserver(this);
}

void URLRequestJob::NotifyCertificateRequested(
    SSLCertRequestInfo* cert_request_info) {
  if (!request_)
    return;  // The request was destroyed, so there is no more work to do.

  request_->NotifyCertificateRequested(cert_request_info);
}

void URLRequestJob::NotifySSLCertificateError(const SSLInfo& ssl_info,
                                              bool fatal) {
  if (!request_)
    return;  // The request was destroyed, so there is no more work to do.

  request_->NotifySSLCertificateError(ssl_info, fatal);
}

bool URLRequestJob::CanGetCookies(const CookieList& cookie_list) const {
  if (!request_)
    return false;  // The request was destroyed, so there is no more work to do.

  return request_->CanGetCookies(cookie_list);
}

bool URLRequestJob::CanSetCookie(const std::string& cookie_line,
                                 CookieOptions* options) const {
  if (!request_)
    return false;  // The request was destroyed, so there is no more work to do.

  return request_->CanSetCookie(cookie_line, options);
}

bool URLRequestJob::CanEnablePrivacyMode() const {
  if (!request_)
    return false;  // The request was destroyed, so there is no more work to do.

  return request_->CanEnablePrivacyMode();
}

CookieStore* URLRequestJob::GetCookieStore() const {
  DCHECK(request_);

  return request_->cookie_store();
}

void URLRequestJob::NotifyBeforeNetworkStart(bool* defer) {
  if (!request_)
    return;

  request_->NotifyBeforeNetworkStart(defer);
}

void URLRequestJob::NotifyHeadersComplete() {
  if (!request_ || !request_->has_delegate())
    return;  // The request was destroyed, so there is no more work to do.

  if (has_handled_response_)
    return;

  DCHECK(!request_->status().is_io_pending());

  // Initialize to the current time, and let the subclass optionally override
  // the time stamps if it has that information.  The default request_time is
  // set by URLRequest before it calls our Start method.
  request_->response_info_.response_time = base::Time::Now();
  GetResponseInfo(&request_->response_info_);

  // When notifying the delegate, the delegate can release the request
  // (and thus release 'this').  After calling to the delgate, we must
  // check the request pointer to see if it still exists, and return
  // immediately if it has been destroyed.  self_preservation ensures our
  // survival until we can get out of this method.
  scoped_refptr<URLRequestJob> self_preservation(this);

  if (request_)
    request_->OnHeadersComplete();

  GURL new_location;
  int http_status_code;
  if (IsRedirectResponse(&new_location, &http_status_code)) {
    // Redirect response bodies are not read. Notify the transaction
    // so it does not treat being stopped as an error.
    DoneReadingRedirectResponse();

    const GURL& url = request_->url();

    // Move the reference fragment of the old location to the new one if the
    // new one has none. This duplicates mozilla's behavior.
    if (url.is_valid() && url.has_ref() && !new_location.has_ref() &&
        CopyFragmentOnRedirect(new_location)) {
      GURL::Replacements replacements;
      // Reference the |ref| directly out of the original URL to avoid a
      // malloc.
      replacements.SetRef(url.spec().data(),
                          url.parsed_for_possibly_invalid_spec().ref);
      new_location = new_location.ReplaceComponents(replacements);
    }

    bool defer_redirect = false;
    request_->NotifyReceivedRedirect(new_location, &defer_redirect);

    // Ensure that the request wasn't detached or destroyed in
    // NotifyReceivedRedirect
    if (!request_ || !request_->has_delegate())
      return;

    // If we were not cancelled, then maybe follow the redirect.
    if (request_->status().is_success()) {
      if (defer_redirect) {
        deferred_redirect_url_ = new_location;
        deferred_redirect_status_code_ = http_status_code;
      } else {
        FollowRedirect(new_location, http_status_code);
      }
      return;
    }
  } else if (NeedsAuth()) {
    scoped_refptr<AuthChallengeInfo> auth_info;
    GetAuthChallengeInfo(&auth_info);
    // Need to check for a NULL auth_info because the server may have failed
    // to send a challenge with the 401 response.
    if (auth_info.get()) {
      request_->NotifyAuthRequired(auth_info.get());
      // Wait for SetAuth or CancelAuth to be called.
      return;
    }
  }

  has_handled_response_ = true;
  if (request_->status().is_success())
    filter_.reset(SetupFilter());

  if (!filter_.get()) {
    std::string content_length;
    request_->GetResponseHeaderByName("content-length", &content_length);
    if (!content_length.empty())
      base::StringToInt64(content_length, &expected_content_size_);
  }

  request_->NotifyResponseStarted();
}

void URLRequestJob::NotifyReadComplete(int bytes_read) {
  if (!request_ || !request_->has_delegate())
    return;  // The request was destroyed, so there is no more work to do.

  // TODO(darin): Bug 1004233. Re-enable this test once all of the chrome
  // unit_tests have been fixed to not trip this.
#if 0
  DCHECK(!request_->status().is_io_pending());
#endif
  // The headers should be complete before reads complete
  DCHECK(has_handled_response_);

  OnRawReadComplete(bytes_read);

  // Don't notify if we had an error.
  if (!request_->status().is_success())
    return;

  // When notifying the delegate, the delegate can release the request
  // (and thus release 'this').  After calling to the delegate, we must
  // check the request pointer to see if it still exists, and return
  // immediately if it has been destroyed.  self_preservation ensures our
  // survival until we can get out of this method.
  scoped_refptr<URLRequestJob> self_preservation(this);

  if (filter_.get()) {
    // Tell the filter that it has more data
    FilteredDataRead(bytes_read);

    // Filter the data.
    int filter_bytes_read = 0;
    if (ReadFilteredData(&filter_bytes_read)) {
      if (!filter_bytes_read)
        DoneReading();
      request_->NotifyReadCompleted(filter_bytes_read);
    }
  } else {
    request_->NotifyReadCompleted(bytes_read);
  }
  DVLOG(1) << __FUNCTION__ << "() "
           << "\"" << (request_ ? request_->url().spec() : "???") << "\""
           << " pre bytes read = " << bytes_read
           << " pre total = " << prefilter_bytes_read_
           << " post total = " << postfilter_bytes_read_;
}

void URLRequestJob::NotifyStartError(const URLRequestStatus &status) {
  DCHECK(!has_handled_response_);
  has_handled_response_ = true;
  if (request_) {
    // There may be relevant information in the response info even in the
    // error case.
    GetResponseInfo(&request_->response_info_);

    request_->set_status(status);
    request_->NotifyResponseStarted();
    // We may have been deleted.
  }
}

void URLRequestJob::NotifyDone(const URLRequestStatus &status) {
  DCHECK(!done_) << "Job sending done notification twice";
  if (done_)
    return;
  done_ = true;

  // Unless there was an error, we should have at least tried to handle
  // the response before getting here.
  DCHECK(has_handled_response_ || !status.is_success());

  // As with NotifyReadComplete, we need to take care to notice if we were
  // destroyed during a delegate callback.
  if (request_) {
    request_->set_is_pending(false);
    // With async IO, it's quite possible to have a few outstanding
    // requests.  We could receive a request to Cancel, followed shortly
    // by a successful IO.  For tracking the status(), once there is
    // an error, we do not change the status back to success.  To
    // enforce this, only set the status if the job is so far
    // successful.
    if (request_->status().is_success()) {
      if (status.status() == URLRequestStatus::FAILED) {
        request_->net_log().AddEventWithNetErrorCode(NetLog::TYPE_FAILED,
                                                     status.error());
      }
      request_->set_status(status);
    }
  }

  // Complete this notification later.  This prevents us from re-entering the
  // delegate if we're done because of a synchronous call.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestJob::CompleteNotifyDone,
                 weak_factory_.GetWeakPtr()));
}

void URLRequestJob::CompleteNotifyDone() {
  // Check if we should notify the delegate that we're done because of an error.
  if (request_ &&
      !request_->status().is_success() &&
      request_->has_delegate()) {
    // We report the error differently depending on whether we've called
    // OnResponseStarted yet.
    if (has_handled_response_) {
      // We signal the error by calling OnReadComplete with a bytes_read of -1.
      request_->NotifyReadCompleted(-1);
    } else {
      has_handled_response_ = true;
      request_->NotifyResponseStarted();
    }
  }
}

void URLRequestJob::NotifyCanceled() {
  if (!done_) {
    NotifyDone(URLRequestStatus(URLRequestStatus::CANCELED, ERR_ABORTED));
  }
}

void URLRequestJob::NotifyRestartRequired() {
  DCHECK(!has_handled_response_);
  if (GetStatus().status() != URLRequestStatus::CANCELED)
    request_->Restart();
}

void URLRequestJob::OnCallToDelegate() {
  request_->OnCallToDelegate();
}

void URLRequestJob::OnCallToDelegateComplete() {
  request_->OnCallToDelegateComplete();
}

bool URLRequestJob::ReadRawData(IOBuffer* buf, int buf_size,
                                int *bytes_read) {
  DCHECK(bytes_read);
  *bytes_read = 0;
  return true;
}

void URLRequestJob::DoneReading() {
  // Do nothing.
}

void URLRequestJob::DoneReadingRedirectResponse() {
}

void URLRequestJob::FilteredDataRead(int bytes_read) {
  DCHECK(filter_);
  filter_->FlushStreamBuffer(bytes_read);
}

bool URLRequestJob::ReadFilteredData(int* bytes_read) {
  DCHECK(filter_);
  DCHECK(filtered_read_buffer_);
  DCHECK_GT(filtered_read_buffer_len_, 0);
  DCHECK_LT(filtered_read_buffer_len_, 1000000);  // Sanity check.
  DCHECK(!raw_read_buffer_);

  *bytes_read = 0;
  bool rv = false;

  for (;;) {
    if (is_done())
      return true;

    if (!filter_needs_more_output_space_ && !filter_->stream_data_len()) {
      // We don't have any raw data to work with, so read from the transaction.
      int filtered_data_read;
      if (ReadRawDataForFilter(&filtered_data_read)) {
        if (filtered_data_read > 0) {
          // Give data to filter.
          filter_->FlushStreamBuffer(filtered_data_read);
        } else {
          return true;  // EOF.
        }
      } else {
        return false;  // IO Pending (or error).
      }
    }

    if ((filter_->stream_data_len() || filter_needs_more_output_space_) &&
        !is_done()) {
      // Get filtered data.
      int filtered_data_len = filtered_read_buffer_len_;
      int output_buffer_size = filtered_data_len;
      Filter::FilterStatus status =
          filter_->ReadData(filtered_read_buffer_->data(), &filtered_data_len);

      if (filter_needs_more_output_space_ && !filtered_data_len) {
        // filter_needs_more_output_space_ was mistaken... there are no more
        // bytes and we should have at least tried to fill up the filter's input
        // buffer. Correct the state, and try again.
        filter_needs_more_output_space_ = false;
        continue;
      }
      filter_needs_more_output_space_ =
          (filtered_data_len == output_buffer_size);

      switch (status) {
        case Filter::FILTER_DONE: {
          filter_needs_more_output_space_ = false;
          *bytes_read = filtered_data_len;
          postfilter_bytes_read_ += filtered_data_len;
          rv = true;
          break;
        }
        case Filter::FILTER_NEED_MORE_DATA: {
          // We have finished filtering all data currently in the buffer.
          // There might be some space left in the output buffer. One can
          // consider reading more data from the stream to feed the filter
          // and filling up the output buffer. This leads to more complicated
          // buffer management and data notification mechanisms.
          // We can revisit this issue if there is a real perf need.
          if (filtered_data_len > 0) {
            *bytes_read = filtered_data_len;
            postfilter_bytes_read_ += filtered_data_len;
            rv = true;
          } else {
            // Read again since we haven't received enough data yet (e.g., we
            // may not have a complete gzip header yet).
            continue;
          }
          break;
        }
        case Filter::FILTER_OK: {
          *bytes_read = filtered_data_len;
          postfilter_bytes_read_ += filtered_data_len;
          rv = true;
          break;
        }
        case Filter::FILTER_ERROR: {
          DVLOG(1) << __FUNCTION__ << "() "
                   << "\"" << (request_ ? request_->url().spec() : "???")
                   << "\"" << " Filter Error";
          filter_needs_more_output_space_ = false;
          NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                     ERR_CONTENT_DECODING_FAILED));
          rv = false;
          break;
        }
        default: {
          NOTREACHED();
          filter_needs_more_output_space_ = false;
          rv = false;
          break;
        }
      }

      // If logging all bytes is enabled, log the filtered bytes read.
      if (rv && request() && request()->net_log().IsLoggingBytes() &&
          filtered_data_len > 0) {
        request()->net_log().AddByteTransferEvent(
            NetLog::TYPE_URL_REQUEST_JOB_FILTERED_BYTES_READ,
            filtered_data_len, filtered_read_buffer_->data());
      }
    } else {
      // we are done, or there is no data left.
      rv = true;
    }
    break;
  }

  if (rv) {
    // When we successfully finished a read, we no longer need to save the
    // caller's buffers. Release our reference.
    filtered_read_buffer_ = NULL;
    filtered_read_buffer_len_ = 0;
  }
  return rv;
}

void URLRequestJob::DestroyFilters() {
  filter_.reset();
}

const URLRequestStatus URLRequestJob::GetStatus() {
  if (request_)
    return request_->status();
  // If the request is gone, we must be cancelled.
  return URLRequestStatus(URLRequestStatus::CANCELED,
                          ERR_ABORTED);
}

void URLRequestJob::SetStatus(const URLRequestStatus &status) {
  if (request_)
    request_->set_status(status);
}

void URLRequestJob::SetProxyServer(const HostPortPair& proxy_server) {
  request_->proxy_server_ = proxy_server;
}

bool URLRequestJob::ReadRawDataForFilter(int* bytes_read) {
  bool rv = false;

  DCHECK(bytes_read);
  DCHECK(filter_.get());

  *bytes_read = 0;

  // Get more pre-filtered data if needed.
  // TODO(mbelshe): is it possible that the filter needs *MORE* data
  //    when there is some data already in the buffer?
  if (!filter_->stream_data_len() && !is_done()) {
    IOBuffer* stream_buffer = filter_->stream_buffer();
    int stream_buffer_size = filter_->stream_buffer_size();
    rv = ReadRawDataHelper(stream_buffer, stream_buffer_size, bytes_read);
  }
  return rv;
}

bool URLRequestJob::ReadRawDataHelper(IOBuffer* buf, int buf_size,
                                      int* bytes_read) {
  DCHECK(!request_->status().is_io_pending());
  DCHECK(raw_read_buffer_.get() == NULL);

  // Keep a pointer to the read buffer, so we have access to it in the
  // OnRawReadComplete() callback in the event that the read completes
  // asynchronously.
  raw_read_buffer_ = buf;
  bool rv = ReadRawData(buf, buf_size, bytes_read);

  if (!request_->status().is_io_pending()) {
    // If the read completes synchronously, either success or failure,
    // invoke the OnRawReadComplete callback so we can account for the
    // completed read.
    OnRawReadComplete(*bytes_read);
  }
  return rv;
}

void URLRequestJob::FollowRedirect(const GURL& location, int http_status_code) {
  int rv = request_->Redirect(location, http_status_code);
  if (rv != OK)
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
}

void URLRequestJob::OnRawReadComplete(int bytes_read) {
  DCHECK(raw_read_buffer_.get());
  // If |filter_| is non-NULL, bytes will be logged after it is applied instead.
  if (!filter_.get() && request() && request()->net_log().IsLoggingBytes() &&
      bytes_read > 0) {
    request()->net_log().AddByteTransferEvent(
        NetLog::TYPE_URL_REQUEST_JOB_BYTES_READ,
        bytes_read, raw_read_buffer_->data());
  }

  if (bytes_read > 0) {
    RecordBytesRead(bytes_read);
  }
  raw_read_buffer_ = NULL;
}

void URLRequestJob::RecordBytesRead(int bytes_read) {
  filter_input_byte_count_ += bytes_read;
  prefilter_bytes_read_ += bytes_read;
  if (!filter_.get())
    postfilter_bytes_read_ += bytes_read;
  DVLOG(2) << __FUNCTION__ << "() "
           << "\"" << (request_ ? request_->url().spec() : "???") << "\""
           << " pre bytes read = " << bytes_read
           << " pre total = " << prefilter_bytes_read_
           << " post total = " << postfilter_bytes_read_;
  UpdatePacketReadTimes();  // Facilitate stats recording if it is active.
  if (network_delegate_)
    network_delegate_->NotifyRawBytesRead(*request_, bytes_read);
}

bool URLRequestJob::FilterHasData() {
    return filter_.get() && filter_->stream_data_len();
}

void URLRequestJob::UpdatePacketReadTimes() {
}

}  // namespace net
