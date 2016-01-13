// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_JOB_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_observer.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/base/upload_progress.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace net {

class AuthChallengeInfo;
class AuthCredentials;
class CookieOptions;
class CookieStore;
class Filter;
class HttpRequestHeaders;
class HttpResponseInfo;
class IOBuffer;
struct LoadTimingInfo;
class NetworkDelegate;
class SSLCertRequestInfo;
class SSLInfo;
class URLRequest;
class UploadDataStream;
class URLRequestStatus;
class X509Certificate;

class NET_EXPORT URLRequestJob
    : public base::RefCounted<URLRequestJob>,
      public base::PowerObserver {
 public:
  explicit URLRequestJob(URLRequest* request,
                         NetworkDelegate* network_delegate);

  // Returns the request that owns this job. THIS POINTER MAY BE NULL if the
  // request was destroyed.
  URLRequest* request() const {
    return request_;
  }

  // Sets the upload data, most requests have no upload data, so this is a NOP.
  // Job types supporting upload data will override this.
  virtual void SetUpload(UploadDataStream* upload_data_stream);

  // Sets extra request headers for Job types that support request
  // headers. Called once before Start() is called.
  virtual void SetExtraRequestHeaders(const HttpRequestHeaders& headers);

  // Sets the priority of the job. Called once before Start() is
  // called, but also when the priority of the parent request changes.
  virtual void SetPriority(RequestPriority priority);

  // If any error occurs while starting the Job, NotifyStartError should be
  // called.
  // This helps ensure that all errors follow more similar notification code
  // paths, which should simplify testing.
  virtual void Start() = 0;

  // This function MUST somehow call NotifyDone/NotifyCanceled or some requests
  // will get leaked. Certain callers use that message to know when they can
  // delete their URLRequest object, even when doing a cancel. The default
  // Kill implementation calls NotifyCanceled, so it is recommended that
  // subclasses call URLRequestJob::Kill() after doing any additional work.
  //
  // The job should endeavor to stop working as soon as is convenient, but must
  // not send and complete notifications from inside this function. Instead,
  // complete notifications (including "canceled") should be sent from a
  // callback run from the message loop.
  //
  // The job is not obliged to immediately stop sending data in response to
  // this call, nor is it obliged to fail with "canceled" unless not all data
  // was sent as a result. A typical case would be where the job is almost
  // complete and can succeed before the canceled notification can be
  // dispatched (from the message loop).
  //
  // The job should be prepared to receive multiple calls to kill it, but only
  // one notification must be issued.
  virtual void Kill();

  // Called to detach the request from this Job.  Results in the Job being
  // killed off eventually. The job must not use the request pointer any more.
  void DetachRequest();

  // Called to read post-filtered data from this Job, returning the number of
  // bytes read, 0 when there is no more data, or -1 if there was an error.
  // This is just the backend for URLRequest::Read, see that function for
  // more info.
  bool Read(IOBuffer* buf, int buf_size, int* bytes_read);

  // Stops further caching of this request, if any. For more info, see
  // URLRequest::StopCaching().
  virtual void StopCaching();

  virtual bool GetFullRequestHeaders(HttpRequestHeaders* headers) const;

  // Get the number of bytes received from network.
  virtual int64 GetTotalReceivedBytes() const;

  // Called to fetch the current load state for the job.
  virtual LoadState GetLoadState() const;

  // Called to get the upload progress in bytes.
  virtual UploadProgress GetUploadProgress() const;

  // Called to fetch the charset for this request.  Only makes sense for some
  // types of requests. Returns true on success.  Calling this on a type that
  // doesn't have a charset will return false.
  virtual bool GetCharset(std::string* charset);

  // Called to get response info.
  virtual void GetResponseInfo(HttpResponseInfo* info);

  // This returns the times when events actually occurred, rather than the time
  // each event blocked the request.  See FixupLoadTimingInfo in url_request.h
  // for more information on the difference.
  virtual void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const;

  // Returns the cookie values included in the response, if applicable.
  // Returns true if applicable.
  // NOTE: This removes the cookies from the job, so it will only return
  //       useful results once per job.
  virtual bool GetResponseCookies(std::vector<std::string>* cookies);

  // Called to setup a stream filter for this request. An example of filter is
  // content encoding/decoding.
  // Subclasses should return the appropriate Filter, or NULL for no Filter.
  // This class takes ownership of the returned Filter.
  //
  // The default implementation returns NULL.
  virtual Filter* SetupFilter() const;

  // Called to determine if this response is a redirect.  Only makes sense
  // for some types of requests.  This method returns true if the response
  // is a redirect, and fills in the location param with the URL of the
  // redirect.  The HTTP status code (e.g., 302) is filled into
  // |*http_status_code| to signify the type of redirect.
  //
  // The caller is responsible for following the redirect by setting up an
  // appropriate replacement Job. Note that the redirected location may be
  // invalid, the caller should be sure it can handle this.
  //
  // The default implementation inspects the response_info_.
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

  // Called to determine if it is okay to copy the reference fragment from the
  // original URL (if existent) to the redirection target when the redirection
  // target has no reference fragment.
  //
  // The default implementation returns true.
  virtual bool CopyFragmentOnRedirect(const GURL& location) const;

  // Called to determine if it is okay to redirect this job to the specified
  // location.  This may be used to implement protocol-specific restrictions.
  // If this function returns false, then the URLRequest will fail
  // reporting ERR_UNSAFE_REDIRECT.
  virtual bool IsSafeRedirect(const GURL& location);

  // Called to determine if this response is asking for authentication.  Only
  // makes sense for some types of requests.  The caller is responsible for
  // obtaining the credentials passing them to SetAuth.
  virtual bool NeedsAuth();

  // Fills the authentication info with the server's response.
  virtual void GetAuthChallengeInfo(
      scoped_refptr<AuthChallengeInfo>* auth_info);

  // Resend the request with authentication credentials.
  virtual void SetAuth(const AuthCredentials& credentials);

  // Display the error page without asking for credentials again.
  virtual void CancelAuth();

  virtual void ContinueWithCertificate(X509Certificate* client_cert);

  // Continue processing the request ignoring the last error.
  virtual void ContinueDespiteLastError();

  // Continue with the network request.
  virtual void ResumeNetworkStart();

  void FollowDeferredRedirect();

  // Returns true if the Job is done producing response data and has called
  // NotifyDone on the request.
  bool is_done() const { return done_; }

  // Get/Set expected content size
  int64 expected_content_size() const { return expected_content_size_; }
  void set_expected_content_size(const int64& size) {
    expected_content_size_ = size;
  }

  // Whether we have processed the response for that request yet.
  bool has_response_started() const { return has_handled_response_; }

  // These methods are not applicable to all connections.
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual int GetResponseCode() const;

  // Returns the socket address for the connection.
  // See url_request.h for details.
  virtual HostPortPair GetSocketAddress() const;

  // base::PowerObserver methods:
  // We invoke URLRequestJob::Kill on suspend (crbug.com/4606).
  virtual void OnSuspend() OVERRIDE;

  // Called after a NetworkDelegate has been informed that the URLRequest
  // will be destroyed. This is used to track that no pending callbacks
  // exist at destruction time of the URLRequestJob, unless they have been
  // canceled by an explicit NetworkDelegate::NotifyURLRequestDestroyed() call.
  virtual void NotifyURLRequestDestroyed();

 protected:
  friend class base::RefCounted<URLRequestJob>;
  virtual ~URLRequestJob();

  // Notifies the job that a certificate is requested.
  void NotifyCertificateRequested(SSLCertRequestInfo* cert_request_info);

  // Notifies the job about an SSL certificate error.
  void NotifySSLCertificateError(const SSLInfo& ssl_info, bool fatal);

  // Delegates to URLRequest::Delegate.
  bool CanGetCookies(const CookieList& cookie_list) const;

  // Delegates to URLRequest::Delegate.
  bool CanSetCookie(const std::string& cookie_line,
                    CookieOptions* options) const;

  // Delegates to URLRequest::Delegate.
  bool CanEnablePrivacyMode() const;

  // Returns the cookie store to be used for the request.
  CookieStore* GetCookieStore() const;

  // Notifies the job that the network is about to be used.
  void NotifyBeforeNetworkStart(bool* defer);

  // Notifies the job that headers have been received.
  void NotifyHeadersComplete();

  // Notifies the request that the job has completed a Read operation.
  void NotifyReadComplete(int bytes_read);

  // Notifies the request that a start error has occurred.
  void NotifyStartError(const URLRequestStatus& status);

  // NotifyDone marks when we are done with a request.  It is really
  // a glorified set_status, but also does internal state checking and
  // job tracking.  It should be called once per request, when the job is
  // finished doing all IO.
  void NotifyDone(const URLRequestStatus& status);

  // Some work performed by NotifyDone must be completed on a separate task
  // so as to avoid re-entering the delegate.  This method exists to perform
  // that work.
  void CompleteNotifyDone();

  // Used as an asynchronous callback for Kill to notify the URLRequest
  // that we were canceled.
  void NotifyCanceled();

  // Notifies the job the request should be restarted.
  // Should only be called if the job has not started a response.
  void NotifyRestartRequired();

  // See corresponding functions in url_request.h.
  void OnCallToDelegate();
  void OnCallToDelegateComplete();

  // Called to read raw (pre-filtered) data from this Job.
  // If returning true, data was read from the job.  buf will contain
  // the data, and bytes_read will receive the number of bytes read.
  // If returning true, and bytes_read is returned as 0, there is no
  // additional data to be read.
  // If returning false, an error occurred or an async IO is now pending.
  // If async IO is pending, the status of the request will be
  // URLRequestStatus::IO_PENDING, and buf must remain available until the
  // operation is completed.  See comments on URLRequest::Read for more
  // info.
  virtual bool ReadRawData(IOBuffer* buf, int buf_size, int *bytes_read);

  // Called to tell the job that a filter has successfully reached the end of
  // the stream.
  virtual void DoneReading();

  // Called to tell the job that the body won't be read because it's a redirect.
  // This is needed so that redirect headers can be cached even though their
  // bodies are never read.
  virtual void DoneReadingRedirectResponse();

  // Informs the filter that data has been read into its buffer
  void FilteredDataRead(int bytes_read);

  // Reads filtered data from the request.  Returns true if successful,
  // false otherwise.  Note, if there is not enough data received to
  // return data, this call can issue a new async IO request under
  // the hood.
  bool ReadFilteredData(int *bytes_read);

  // Whether the response is being filtered in this job.
  // Only valid after NotifyHeadersComplete() has been called.
  bool HasFilter() { return filter_ != NULL; }

  // At or near destruction time, a derived class may request that the filters
  // be destroyed so that statistics can be gathered while the derived class is
  // still present to assist in calculations.  This is used by URLRequestHttpJob
  // to get SDCH to emit stats.
  void DestroyFilters();

  // Provides derived classes with access to the request's network delegate.
  NetworkDelegate* network_delegate() { return network_delegate_; }

  // The status of the job.
  const URLRequestStatus GetStatus();

  // Set the status of the job.
  void SetStatus(const URLRequestStatus& status);

  // Set the proxy server that was used, if any.
  void SetProxyServer(const HostPortPair& proxy_server);

  // The number of bytes read before passing to the filter.
  int prefilter_bytes_read() const { return prefilter_bytes_read_; }

  // The number of bytes read after passing through the filter.
  int postfilter_bytes_read() const { return postfilter_bytes_read_; }

  // Total number of bytes read from network (or cache) and typically handed
  // to filter to process.  Used to histogram compression ratios, and error
  // recovery scenarios in filters.
  int64 filter_input_byte_count() const { return filter_input_byte_count_; }

  // The request that initiated this job. This value MAY BE NULL if the
  // request was released by DetachRequest().
  URLRequest* request_;

 private:
  // When data filtering is enabled, this function is used to read data
  // for the filter.  Returns true if raw data was read.  Returns false if
  // an error occurred (or we are waiting for IO to complete).
  bool ReadRawDataForFilter(int *bytes_read);

  // Invokes ReadRawData and records bytes read if the read completes
  // synchronously.
  bool ReadRawDataHelper(IOBuffer* buf, int buf_size, int* bytes_read);

  // Called in response to a redirect that was not canceled to follow the
  // redirect. The current job will be replaced with a new job loading the
  // given redirect destination.
  void FollowRedirect(const GURL& location, int http_status_code);

  // Called after every raw read. If |bytes_read| is > 0, this indicates
  // a successful read of |bytes_read| unfiltered bytes. If |bytes_read|
  // is 0, this indicates that there is no additional data to read. If
  // |bytes_read| is < 0, an error occurred and no bytes were read.
  void OnRawReadComplete(int bytes_read);

  // Updates the profiling info and notifies observers that an additional
  // |bytes_read| unfiltered bytes have been read for this job.
  void RecordBytesRead(int bytes_read);

  // Called to query whether there is data available in the filter to be read
  // out.
  bool FilterHasData();

  // Subclasses may implement this method to record packet arrival times.
  // The default implementation does nothing.
  virtual void UpdatePacketReadTimes();

  // Indicates that the job is done producing data, either it has completed
  // all the data or an error has been encountered. Set exclusively by
  // NotifyDone so that it is kept in sync with the request.
  bool done_;

  int prefilter_bytes_read_;
  int postfilter_bytes_read_;
  int64 filter_input_byte_count_;

  // The data stream filter which is enabled on demand.
  scoped_ptr<Filter> filter_;

  // If the filter filled its output buffer, then there is a change that it
  // still has internal data to emit, and this flag is set.
  bool filter_needs_more_output_space_;

  // When we filter data, we receive data into the filter buffers.  After
  // processing the filtered data, we return the data in the caller's buffer.
  // While the async IO is in progress, we save the user buffer here, and
  // when the IO completes, we fill this in.
  scoped_refptr<IOBuffer> filtered_read_buffer_;
  int filtered_read_buffer_len_;

  // We keep a pointer to the read buffer while asynchronous reads are
  // in progress, so we are able to pass those bytes to job observers.
  scoped_refptr<IOBuffer> raw_read_buffer_;

  // Used by HandleResponseIfNecessary to track whether we've sent the
  // OnResponseStarted callback and potentially redirect callbacks as well.
  bool has_handled_response_;

  // Expected content size
  int64 expected_content_size_;

  // Set when a redirect is deferred.
  GURL deferred_redirect_url_;
  int deferred_redirect_status_code_;

  // The network delegate to use with this request, if any.
  NetworkDelegate* network_delegate_;

  base::WeakPtrFactory<URLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_JOB_H_
