// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_H_
#define NET_URL_REQUEST_URL_REQUEST_H_

#include <string>
#include <vector>

#include "base/debug/leak_tracker.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/network_delegate.h"
#include "net/base/request_priority.h"
#include "net/base/upload_progress.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace base {
class Value;

namespace debug {
class StackTrace;
}  // namespace debug
}  // namespace base

// Temporary layering violation to allow existing users of a deprecated
// interface.
namespace content {
class AppCacheInterceptor;
}

namespace net {

class CookieOptions;
class HostPortPair;
class IOBuffer;
struct LoadTimingInfo;
class SSLCertRequestInfo;
class SSLInfo;
class UploadDataStream;
class URLRequestContext;
class URLRequestJob;
class X509Certificate;

// This stores the values of the Set-Cookie headers received during the request.
// Each item in the vector corresponds to a Set-Cookie: line received,
// excluding the "Set-Cookie:" part.
typedef std::vector<std::string> ResponseCookies;

//-----------------------------------------------------------------------------
// A class  representing the asynchronous load of a data stream from an URL.
//
// The lifetime of an instance of this class is completely controlled by the
// consumer, and the instance is not required to live on the heap or be
// allocated in any special way.  It is also valid to delete an URLRequest
// object during the handling of a callback to its delegate.  Of course, once
// the URLRequest is deleted, no further callbacks to its delegate will occur.
//
// NOTE: All usage of all instances of this class should be on the same thread.
//
class NET_EXPORT URLRequest : NON_EXPORTED_BASE(public base::NonThreadSafe),
                              public base::SupportsUserData {
 public:
  // Callback function implemented by protocol handlers to create new jobs.
  // The factory may return NULL to indicate an error, which will cause other
  // factories to be queried.  If no factory handles the request, then the
  // default job will be used.
  typedef URLRequestJob* (ProtocolFactory)(URLRequest* request,
                                           NetworkDelegate* network_delegate,
                                           const std::string& scheme);

  // HTTP request/response header IDs (via some preprocessor fun) for use with
  // SetRequestHeaderById and GetResponseHeaderById.
  enum {
#define HTTP_ATOM(x) HTTP_ ## x,
#include "net/http/http_atom_list.h"
#undef HTTP_ATOM
  };

  // Referrer policies (see set_referrer_policy): During server redirects, the
  // referrer header might be cleared, if the protocol changes from HTTPS to
  // HTTP. This is the default behavior of URLRequest, corresponding to
  // CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE. Alternatively, the
  // referrer policy can be set to never change the referrer header. This
  // behavior corresponds to NEVER_CLEAR_REFERRER. Embedders will want to use
  // NEVER_CLEAR_REFERRER when implementing the meta-referrer support
  // (http://wiki.whatwg.org/wiki/Meta_referrer) and sending requests with a
  // non-default referrer policy. Only the default referrer policy requires
  // the referrer to be cleared on transitions from HTTPS to HTTP.
  enum ReferrerPolicy {
    CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
    NEVER_CLEAR_REFERRER,
  };

  // This class handles network interception.  Use with
  // (Un)RegisterRequestInterceptor.
  class NET_EXPORT Interceptor {
   public:
    virtual ~Interceptor() {}

    // Called for every request made.  Should return a new job to handle the
    // request if it should be intercepted, or NULL to allow the request to
    // be handled in the normal manner.
    virtual URLRequestJob* MaybeIntercept(
        URLRequest* request, NetworkDelegate* network_delegate) = 0;

    // Called after having received a redirect response, but prior to the
    // the request delegate being informed of the redirect. Can return a new
    // job to replace the existing job if it should be intercepted, or NULL
    // to allow the normal handling to continue. If a new job is provided,
    // the delegate never sees the original redirect response, instead the
    // response produced by the intercept job will be returned.
    virtual URLRequestJob* MaybeInterceptRedirect(
        URLRequest* request,
        NetworkDelegate* network_delegate,
        const GURL& location);

    // Called after having received a final response, but prior to the
    // the request delegate being informed of the response. This is also
    // called when there is no server response at all to allow interception
    // on dns or network errors. Can return a new job to replace the existing
    // job if it should be intercepted, or NULL to allow the normal handling to
    // continue. If a new job is provided, the delegate never sees the original
    // response, instead the response produced by the intercept job will be
    // returned.
    virtual URLRequestJob* MaybeInterceptResponse(
        URLRequest* request, NetworkDelegate* network_delegate);
  };

  // Deprecated interfaces in net::URLRequest. They have been moved to
  // URLRequest's private section to prevent new uses. Existing uses are
  // explicitly friended here and should be removed over time.
  class NET_EXPORT Deprecated {
   private:
    // TODO(willchan): Kill off these friend declarations.
    friend class TestInterceptor;
    friend class content::AppCacheInterceptor;

    // TODO(pauljensen): Remove this when AppCacheInterceptor is a
    // ProtocolHandler, see crbug.com/161547.
    static void RegisterRequestInterceptor(Interceptor* interceptor);
    static void UnregisterRequestInterceptor(Interceptor* interceptor);

    DISALLOW_IMPLICIT_CONSTRUCTORS(Deprecated);
  };

  // The delegate's methods are called from the message loop of the thread
  // on which the request's Start() method is called. See above for the
  // ordering of callbacks.
  //
  // The callbacks will be called in the following order:
  //   Start()
  //    - OnCertificateRequested* (zero or more calls, if the SSL server and/or
  //      SSL proxy requests a client certificate for authentication)
  //    - OnSSLCertificateError* (zero or one call, if the SSL server's
  //      certificate has an error)
  //    - OnReceivedRedirect* (zero or more calls, for the number of redirects)
  //    - OnAuthRequired* (zero or more calls, for the number of
  //      authentication failures)
  //    - OnResponseStarted
  //   Read() initiated by delegate
  //    - OnReadCompleted* (zero or more calls until all data is read)
  //
  // Read() must be called at least once. Read() returns true when it completed
  // immediately, and false if an IO is pending or if there is an error.  When
  // Read() returns false, the caller can check the Request's status() to see
  // if an error occurred, or if the IO is just pending.  When Read() returns
  // true with zero bytes read, it indicates the end of the response.
  //
  class NET_EXPORT Delegate {
   public:
    // Called upon a server-initiated redirect.  The delegate may call the
    // request's Cancel method to prevent the redirect from being followed.
    // Since there may be multiple chained redirects, there may also be more
    // than one redirect call.
    //
    // When this function is called, the request will still contain the
    // original URL, the destination of the redirect is provided in 'new_url'.
    // If the delegate does not cancel the request and |*defer_redirect| is
    // false, then the redirect will be followed, and the request's URL will be
    // changed to the new URL.  Otherwise if the delegate does not cancel the
    // request and |*defer_redirect| is true, then the redirect will be
    // followed once FollowDeferredRedirect is called on the URLRequest.
    //
    // The caller must set |*defer_redirect| to false, so that delegates do not
    // need to set it if they are happy with the default behavior of not
    // deferring redirect.
    virtual void OnReceivedRedirect(URLRequest* request,
                                    const GURL& new_url,
                                    bool* defer_redirect);

    // Called when we receive an authentication failure.  The delegate should
    // call request->SetAuth() with the user's credentials once it obtains them,
    // or request->CancelAuth() to cancel the login and display the error page.
    // When it does so, the request will be reissued, restarting the sequence
    // of On* callbacks.
    virtual void OnAuthRequired(URLRequest* request,
                                AuthChallengeInfo* auth_info);

    // Called when we receive an SSL CertificateRequest message for client
    // authentication.  The delegate should call
    // request->ContinueWithCertificate() with the client certificate the user
    // selected, or request->ContinueWithCertificate(NULL) to continue the SSL
    // handshake without a client certificate.
    virtual void OnCertificateRequested(
        URLRequest* request,
        SSLCertRequestInfo* cert_request_info);

    // Called when using SSL and the server responds with a certificate with
    // an error, for example, whose common name does not match the common name
    // we were expecting for that host.  The delegate should either do the
    // safe thing and Cancel() the request or decide to proceed by calling
    // ContinueDespiteLastError().  cert_error is a ERR_* error code
    // indicating what's wrong with the certificate.
    // If |fatal| is true then the host in question demands a higher level
    // of security (due e.g. to HTTP Strict Transport Security, user
    // preference, or built-in policy). In this case, errors must not be
    // bypassable by the user.
    virtual void OnSSLCertificateError(URLRequest* request,
                                       const SSLInfo& ssl_info,
                                       bool fatal);

    // Called to notify that the request must use the network to complete the
    // request and is about to do so. This is called at most once per
    // URLRequest, and by default does not defer. If deferred, call
    // ResumeNetworkStart() to continue or Cancel() to cancel.
    virtual void OnBeforeNetworkStart(URLRequest* request, bool* defer);

    // After calling Start(), the delegate will receive an OnResponseStarted
    // callback when the request has completed.  If an error occurred, the
    // request->status() will be set.  On success, all redirects have been
    // followed and the final response is beginning to arrive.  At this point,
    // meta data about the response is available, including for example HTTP
    // response headers if this is a request for a HTTP resource.
    virtual void OnResponseStarted(URLRequest* request) = 0;

    // Called when the a Read of the response body is completed after an
    // IO_PENDING status from a Read() call.
    // The data read is filled into the buffer which the caller passed
    // to Read() previously.
    //
    // If an error occurred, request->status() will contain the error,
    // and bytes read will be -1.
    virtual void OnReadCompleted(URLRequest* request, int bytes_read) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // TODO(tburkard): we should get rid of this constructor, and have each
  // creator of a URLRequest specifically list the cookie store to be used.
  // For now, this constructor will use the cookie store in |context|.
  URLRequest(const GURL& url,
             RequestPriority priority,
             Delegate* delegate,
             const URLRequestContext* context);

  URLRequest(const GURL& url,
             RequestPriority priority,
             Delegate* delegate,
             const URLRequestContext* context,
             CookieStore* cookie_store);

  // If destroyed after Start() has been called but while IO is pending,
  // then the request will be effectively canceled and the delegate
  // will not have any more of its methods called.
  virtual ~URLRequest();

  // Changes the default cookie policy from allowing all cookies to blocking all
  // cookies. Embedders that want to implement a more flexible policy should
  // change the default to blocking all cookies, and provide a NetworkDelegate
  // with the URLRequestContext that maintains the CookieStore.
  // The cookie policy default has to be set before the first URLRequest is
  // started. Once it was set to block all cookies, it cannot be changed back.
  static void SetDefaultCookiePolicyToBlock();

  // Returns true if the scheme can be handled by URLRequest. False otherwise.
  static bool IsHandledProtocol(const std::string& scheme);

  // Returns true if the url can be handled by URLRequest. False otherwise.
  // The function returns true for invalid urls because URLRequest knows how
  // to handle those.
  // NOTE: This will also return true for URLs that are handled by
  // ProtocolFactories that only work for requests that are scoped to a
  // Profile.
  static bool IsHandledURL(const GURL& url);

  // The original url is the url used to initialize the request, and it may
  // differ from the url if the request was redirected.
  const GURL& original_url() const { return url_chain_.front(); }
  // The chain of urls traversed by this request.  If the request had no
  // redirects, this vector will contain one element.
  const std::vector<GURL>& url_chain() const { return url_chain_; }
  const GURL& url() const { return url_chain_.back(); }

  // The URL that should be consulted for the third-party cookie blocking
  // policy.
  //
  // WARNING: This URL must only be used for the third-party cookie blocking
  //          policy. It MUST NEVER be used for any kind of SECURITY check.
  //
  //          For example, if a top-level navigation is redirected, the
  //          first-party for cookies will be the URL of the first URL in the
  //          redirect chain throughout the whole redirect. If it was used for
  //          a security check, an attacker might try to get around this check
  //          by starting from some page that redirects to the
  //          host-to-be-attacked.
  const GURL& first_party_for_cookies() const {
    return first_party_for_cookies_;
  }
  // This method may be called before Start() or FollowDeferredRedirect() is
  // called.
  void set_first_party_for_cookies(const GURL& first_party_for_cookies);

  // The request method, as an uppercase string.  "GET" is the default value.
  // The request method may only be changed before Start() is called and
  // should only be assigned an uppercase value.
  const std::string& method() const { return method_; }
  void set_method(const std::string& method);

  // Determines the new method of the request afer following a redirect.
  // |method| is the method used to arrive at the redirect,
  // |http_status_code| is the status code associated with the redirect.
  static std::string ComputeMethodForRedirect(const std::string& method,
                                              int http_status_code);

  // The referrer URL for the request.  This header may actually be suppressed
  // from the underlying network request for security reasons (e.g., a HTTPS
  // URL will not be sent as the referrer for a HTTP request).  The referrer
  // may only be changed before Start() is called.
  const std::string& referrer() const { return referrer_; }
  // Referrer is sanitized to remove URL fragment, user name and password.
  void SetReferrer(const std::string& referrer);

  // The referrer policy to apply when updating the referrer during redirects.
  // The referrer policy may only be changed before Start() is called.
  void set_referrer_policy(ReferrerPolicy referrer_policy);

  // Sets the delegate of the request.  This value may be changed at any time,
  // and it is permissible for it to be null.
  void set_delegate(Delegate* delegate);

  // Indicates that the request body should be sent using chunked transfer
  // encoding. This method may only be called before Start() is called.
  void EnableChunkedUpload();

  // Appends the given bytes to the request's upload data to be sent
  // immediately via chunked transfer encoding. When all data has been sent,
  // call MarkEndOfChunks() to indicate the end of upload data.
  //
  // This method may be called only after calling EnableChunkedUpload().
  void AppendChunkToUpload(const char* bytes,
                           int bytes_len,
                           bool is_last_chunk);

  // Sets the upload data.
  void set_upload(scoped_ptr<UploadDataStream> upload);

  // Gets the upload data.
  const UploadDataStream* get_upload() const;

  // Returns true if the request has a non-empty message body to upload.
  bool has_upload() const;

  // Set an extra request header by ID or name, or remove one by name.  These
  // methods may only be called before Start() is called, or before a new
  // redirect in the request chain.
  void SetExtraRequestHeaderById(int header_id, const std::string& value,
                                 bool overwrite);
  void SetExtraRequestHeaderByName(const std::string& name,
                                   const std::string& value, bool overwrite);
  void RemoveRequestHeaderByName(const std::string& name);

  // Sets all extra request headers.  Any extra request headers set by other
  // methods are overwritten by this method.  This method may only be called
  // before Start() is called.  It is an error to call it later.
  void SetExtraRequestHeaders(const HttpRequestHeaders& headers);

  const HttpRequestHeaders& extra_request_headers() const {
    return extra_request_headers_;
  }

  // Gets the full request headers sent to the server.
  //
  // Return true and overwrites headers if it can get the request headers;
  // otherwise, returns false and does not modify headers.  (Always returns
  // false for request types that don't have headers, like file requests.)
  //
  // This is guaranteed to succeed if:
  //
  // 1. A redirect or auth callback is currently running.  Once it ends, the
  //    headers may become unavailable as a new request with the new address
  //    or credentials is made.
  //
  // 2. The OnResponseStarted callback is currently running or has run.
  bool GetFullRequestHeaders(HttpRequestHeaders* headers) const;

  // Gets the total amount of data received from network after SSL decoding and
  // proxy handling.
  int64 GetTotalReceivedBytes() const;

  // Returns the current load state for the request. The returned value's
  // |param| field is an optional parameter describing details related to the
  // load state. Not all load states have a parameter.
  LoadStateWithParam GetLoadState() const;

  // Returns a partial representation of the request's state as a value, for
  // debugging.  Caller takes ownership of returned value.
  base::Value* GetStateAsValue() const;

  // Logs information about the what external object currently blocking the
  // request.  LogUnblocked must be called before resuming the request.  This
  // can be called multiple times in a row either with or without calling
  // LogUnblocked between calls.  |blocked_by| must not be NULL or have length
  // 0.
  void LogBlockedBy(const char* blocked_by);

  // Just like LogBlockedBy, but also makes GetLoadState return source as the
  // |param| in the value returned by GetLoadState.  Calling LogUnblocked or
  // LogBlockedBy will clear the load param.  |blocked_by| must not be NULL or
  // have length 0.
  void LogAndReportBlockedBy(const char* blocked_by);

  // Logs that the request is no longer blocked by the last caller to
  // LogBlockedBy.
  void LogUnblocked();

  // Returns the current upload progress in bytes. When the upload data is
  // chunked, size is set to zero, but position will not be.
  UploadProgress GetUploadProgress() const;

  // Get response header(s) by ID or name.  These methods may only be called
  // once the delegate's OnResponseStarted method has been called.  Headers
  // that appear more than once in the response are coalesced, with values
  // separated by commas (per RFC 2616). This will not work with cookies since
  // comma can be used in cookie values.
  // TODO(darin): add API to enumerate response headers.
  void GetResponseHeaderById(int header_id, std::string* value);
  void GetResponseHeaderByName(const std::string& name, std::string* value);

  // Get all response headers, \n-delimited and \n\0-terminated.  This includes
  // the response status line.  Restrictions on GetResponseHeaders apply.
  void GetAllResponseHeaders(std::string* headers);

  // The time when |this| was constructed.
  base::TimeTicks creation_time() const { return creation_time_; }

  // The time at which the returned response was requested.  For cached
  // responses, this is the last time the cache entry was validated.
  const base::Time& request_time() const {
    return response_info_.request_time;
  }

  // The time at which the returned response was generated.  For cached
  // responses, this is the last time the cache entry was validated.
  const base::Time& response_time() const {
    return response_info_.response_time;
  }

  // Indicate if this response was fetched from disk cache.
  bool was_cached() const { return response_info_.was_cached; }

  // Returns true if the URLRequest was delivered through a proxy.
  bool was_fetched_via_proxy() const {
    return response_info_.was_fetched_via_proxy;
  }

  // Returns true if the URLRequest was delivered over SPDY.
  bool was_fetched_via_spdy() const {
    return response_info_.was_fetched_via_spdy;
  }

  // Returns the host and port that the content was fetched from.  See
  // http_response_info.h for caveats relating to cached content.
  HostPortPair GetSocketAddress() const;

  // Get all response headers, as a HttpResponseHeaders object.  See comments
  // in HttpResponseHeaders class as to the format of the data.
  HttpResponseHeaders* response_headers() const;

  // Get the SSL connection info.
  const SSLInfo& ssl_info() const {
    return response_info_.ssl_info;
  }

  // Gets timing information related to the request.  Events that have not yet
  // occurred are left uninitialized.  After a second request starts, due to
  // a redirect or authentication, values will be reset.
  //
  // LoadTimingInfo only contains ConnectTiming information and socket IDs for
  // non-cached HTTP responses.
  void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const;

  // Returns the cookie values included in the response, if the request is one
  // that can have cookies.  Returns true if the request is a cookie-bearing
  // type, false otherwise.  This method may only be called once the
  // delegate's OnResponseStarted method has been called.
  bool GetResponseCookies(ResponseCookies* cookies);

  // Get the mime type.  This method may only be called once the delegate's
  // OnResponseStarted method has been called.
  void GetMimeType(std::string* mime_type);

  // Get the charset (character encoding).  This method may only be called once
  // the delegate's OnResponseStarted method has been called.
  void GetCharset(std::string* charset);

  // Returns the HTTP response code (e.g., 200, 404, and so on).  This method
  // may only be called once the delegate's OnResponseStarted method has been
  // called.  For non-HTTP requests, this method returns -1.
  int GetResponseCode() const;

  // Get the HTTP response info in its entirety.
  const HttpResponseInfo& response_info() const { return response_info_; }

  // Access the LOAD_* flags modifying this request (see load_flags.h).
  int load_flags() const { return load_flags_; }

  // The new flags may change the IGNORE_LIMITS flag only when called
  // before Start() is called, it must only set the flag, and if set,
  // the priority of this request must already be MAXIMUM_PRIORITY.
  void SetLoadFlags(int flags);

  // Returns true if the request is "pending" (i.e., if Start() has been called,
  // and the response has not yet been called).
  bool is_pending() const { return is_pending_; }

  // Returns true if the request is in the process of redirecting to a new
  // URL but has not yet initiated the new request.
  bool is_redirecting() const { return is_redirecting_; }

  // Returns the error status of the request.
  const URLRequestStatus& status() const { return status_; }

  // Returns a globally unique identifier for this request.
  uint64 identifier() const { return identifier_; }

  // This method is called to start the request.  The delegate will receive
  // a OnResponseStarted callback when the request is started.
  void Start();

  // This method may be called at any time after Start() has been called to
  // cancel the request.  This method may be called many times, and it has
  // no effect once the response has completed.  It is guaranteed that no
  // methods of the delegate will be called after the request has been
  // cancelled, except that this may call the delegate's OnReadCompleted()
  // during the call to Cancel itself.
  void Cancel();

  // Cancels the request and sets the error to |error| (see net_error_list.h
  // for values).
  void CancelWithError(int error);

  // Cancels the request and sets the error to |error| (see net_error_list.h
  // for values) and attaches |ssl_info| as the SSLInfo for that request.  This
  // is useful to attach a certificate and certificate error to a canceled
  // request.
  void CancelWithSSLError(int error, const SSLInfo& ssl_info);

  // Read initiates an asynchronous read from the response, and must only
  // be called after the OnResponseStarted callback is received with a
  // successful status.
  // If data is available, Read will return true, and the data and length will
  // be returned immediately.  If data is not available, Read returns false,
  // and an asynchronous Read is initiated.  The Read is finished when
  // the caller receives the OnReadComplete callback.  Unless the request was
  // cancelled, OnReadComplete will always be called, even if the read failed.
  //
  // The buf parameter is a buffer to receive the data.  If the operation
  // completes asynchronously, the implementation will reference the buffer
  // until OnReadComplete is called.  The buffer must be at least max_bytes in
  // length.
  //
  // The max_bytes parameter is the maximum number of bytes to read.
  //
  // The bytes_read parameter is an output parameter containing the
  // the number of bytes read.  A value of 0 indicates that there is no
  // more data available to read from the stream.
  //
  // If a read error occurs, Read returns false and the request->status
  // will be set to an error.
  bool Read(IOBuffer* buf, int max_bytes, int* bytes_read);

  // If this request is being cached by the HTTP cache, stop subsequent caching.
  // Note that this method has no effect on other (simultaneous or not) requests
  // for the same resource. The typical example is a request that results in
  // the data being stored to disk (downloaded instead of rendered) so we don't
  // want to store it twice.
  void StopCaching();

  // This method may be called to follow a redirect that was deferred in
  // response to an OnReceivedRedirect call.
  void FollowDeferredRedirect();

  // This method must be called to resume network communications that were
  // deferred in response to an OnBeforeNetworkStart call.
  void ResumeNetworkStart();

  // One of the following two methods should be called in response to an
  // OnAuthRequired() callback (and only then).
  // SetAuth will reissue the request with the given credentials.
  // CancelAuth will give up and display the error page.
  void SetAuth(const AuthCredentials& credentials);
  void CancelAuth();

  // This method can be called after the user selects a client certificate to
  // instruct this URLRequest to continue with the request with the
  // certificate.  Pass NULL if the user doesn't have a client certificate.
  void ContinueWithCertificate(X509Certificate* client_cert);

  // This method can be called after some error notifications to instruct this
  // URLRequest to ignore the current error and continue with the request.  To
  // cancel the request instead, call Cancel().
  void ContinueDespiteLastError();

  // Used to specify the context (cookie store, cache) for this request.
  const URLRequestContext* context() const;

  const BoundNetLog& net_log() const { return net_log_; }

  // Returns the expected content size if available
  int64 GetExpectedContentSize() const;

  // Returns the priority level for this request.
  RequestPriority priority() const { return priority_; }

  // Sets the priority level for this request and any related
  // jobs. Must not change the priority to anything other than
  // MAXIMUM_PRIORITY if the IGNORE_LIMITS load flag is set.
  void SetPriority(RequestPriority priority);

  // Returns true iff this request would be internally redirected to HTTPS
  // due to HSTS. If so, |redirect_url| is rewritten to the new HTTPS URL.
  bool GetHSTSRedirect(GURL* redirect_url) const;

  // TODO(willchan): Undo this. Only temporarily public.
  bool has_delegate() const { return delegate_ != NULL; }

  // NOTE(willchan): This is just temporary for debugging
  // http://crbug.com/90971.
  // Allows to setting debug info into the URLRequest.
  void set_stack_trace(const base::debug::StackTrace& stack_trace);
  const base::debug::StackTrace* stack_trace() const;

  void set_received_response_content_length(int64 received_content_length) {
    received_response_content_length_ = received_content_length;
  }
  int64 received_response_content_length() {
    return received_response_content_length_;
  }

  // Available at NetworkDelegate::NotifyHeadersReceived() time, which is before
  // the more general response_info() is available, even though it is a subset.
  const HostPortPair& proxy_server() const {
    return proxy_server_;
  }

 protected:
  // Allow the URLRequestJob class to control the is_pending() flag.
  void set_is_pending(bool value) { is_pending_ = value; }

  // Allow the URLRequestJob class to set our status too
  void set_status(const URLRequestStatus& value) { status_ = value; }

  CookieStore* cookie_store() const { return cookie_store_; }

  // Allow the URLRequestJob to redirect this request.  Returns OK if
  // successful, otherwise an error code is returned.
  int Redirect(const GURL& location, int http_status_code);

  // Called by URLRequestJob to allow interception when a redirect occurs.
  void NotifyReceivedRedirect(const GURL& location, bool* defer_redirect);

  // Called by URLRequestHttpJob (note, only HTTP(S) jobs will call this) to
  // allow deferral of network initialization.
  void NotifyBeforeNetworkStart(bool* defer);

  // Allow an interceptor's URLRequestJob to restart this request.
  // Should only be called if the original job has not started a response.
  void Restart();

 private:
  friend class URLRequestJob;

  // Registers or unregisters a network interception class.
  static void RegisterRequestInterceptor(Interceptor* interceptor);
  static void UnregisterRequestInterceptor(Interceptor* interceptor);

  // Initializes the URLRequest. Code shared between the two constructors.
  // TODO(tburkard): This can ultimately be folded into a single constructor
  // again.
  void Init(const GURL& url,
            RequestPriority priotity,
            Delegate* delegate,
            const URLRequestContext* context,
            CookieStore* cookie_store);

  // Resumes or blocks a request paused by the NetworkDelegate::OnBeforeRequest
  // handler. If |blocked| is true, the request is blocked and an error page is
  // returned indicating so. This should only be called after Start is called
  // and OnBeforeRequest returns true (signalling that the request should be
  // paused).
  void BeforeRequestComplete(int error);

  void StartJob(URLRequestJob* job);

  // Restarting involves replacing the current job with a new one such as what
  // happens when following a HTTP redirect.
  void RestartWithJob(URLRequestJob* job);
  void PrepareToRestart();

  // Detaches the job from this request in preparation for this object going
  // away or the job being replaced. The job will not call us back when it has
  // been orphaned.
  void OrphanJob();

  // Cancels the request and set the error and ssl info for this request to the
  // passed values.
  void DoCancel(int error, const SSLInfo& ssl_info);

  // Called by the URLRequestJob when the headers are received, before any other
  // method, to allow caching of load timing information.
  void OnHeadersComplete();

  // Notifies the network delegate that the request has been completed.
  // This does not imply a successful completion. Also a canceled request is
  // considered completed.
  void NotifyRequestCompleted();

  // Called by URLRequestJob to allow interception when the final response
  // occurs.
  void NotifyResponseStarted();

  // These functions delegate to |delegate_| and may only be used if
  // |delegate_| is not NULL. See URLRequest::Delegate for the meaning
  // of these functions.
  void NotifyAuthRequired(AuthChallengeInfo* auth_info);
  void NotifyAuthRequiredComplete(NetworkDelegate::AuthRequiredResponse result);
  void NotifyCertificateRequested(SSLCertRequestInfo* cert_request_info);
  void NotifySSLCertificateError(const SSLInfo& ssl_info, bool fatal);
  void NotifyReadCompleted(int bytes_read);

  // These functions delegate to |network_delegate_| if it is not NULL.
  // If |network_delegate_| is NULL, cookies can be used unless
  // SetDefaultCookiePolicyToBlock() has been called.
  bool CanGetCookies(const CookieList& cookie_list) const;
  bool CanSetCookie(const std::string& cookie_line,
                    CookieOptions* options) const;
  bool CanEnablePrivacyMode() const;

  // Called just before calling a delegate that may block a request.
  void OnCallToDelegate();
  // Called when the delegate lets a request continue.  Also called on
  // cancellation.
  void OnCallToDelegateComplete();

  // Contextual information used for this request. Cannot be NULL. This contains
  // most of the dependencies which are shared between requests (disk cache,
  // cookie store, socket pool, etc.)
  const URLRequestContext* context_;

  NetworkDelegate* network_delegate_;

  // Tracks the time spent in various load states throughout this request.
  BoundNetLog net_log_;

  scoped_refptr<URLRequestJob> job_;
  scoped_ptr<UploadDataStream> upload_data_stream_;
  std::vector<GURL> url_chain_;
  GURL first_party_for_cookies_;
  GURL delegate_redirect_url_;
  std::string method_;  // "GET", "POST", etc. Should be all uppercase.
  std::string referrer_;
  ReferrerPolicy referrer_policy_;
  HttpRequestHeaders extra_request_headers_;
  int load_flags_;  // Flags indicating the request type for the load;
                    // expected values are LOAD_* enums above.

  // Never access methods of the |delegate_| directly. Always use the
  // Notify... methods for this.
  Delegate* delegate_;

  // Current error status of the job. When no error has been encountered, this
  // will be SUCCESS. If multiple errors have been encountered, this will be
  // the first non-SUCCESS status seen.
  URLRequestStatus status_;

  // The HTTP response info, lazily initialized.
  HttpResponseInfo response_info_;

  // Tells us whether the job is outstanding. This is true from the time
  // Start() is called to the time we dispatch RequestComplete and indicates
  // whether the job is active.
  bool is_pending_;

  // Indicates if the request is in the process of redirecting to a new
  // location.  It is true from the time the headers complete until a
  // new request begins.
  bool is_redirecting_;

  // Number of times we're willing to redirect.  Used to guard against
  // infinite redirects.
  int redirect_limit_;

  // Cached value for use after we've orphaned the job handling the
  // first transaction in a request involving redirects.
  UploadProgress final_upload_progress_;

  // The priority level for this request.  Objects like
  // ClientSocketPool use this to determine which URLRequest to
  // allocate sockets to first.
  RequestPriority priority_;

  // TODO(battre): The only consumer of the identifier_ is currently the
  // web request API. We need to match identifiers of requests between the
  // web request API and the web navigation API. As the URLRequest does not
  // exist when the web navigation API is triggered, the tracking probably
  // needs to be done outside of the URLRequest anyway. Therefore, this
  // identifier should be deleted here. http://crbug.com/89321
  // A globally unique identifier for this request.
  const uint64 identifier_;

  // True if this request is currently calling a delegate, or is blocked waiting
  // for the URL request or network delegate to resume it.
  bool calling_delegate_;

  // An optional parameter that provides additional information about what
  // |this| is currently being blocked by.
  std::string blocked_by_;
  bool use_blocked_by_as_load_param_;

  base::debug::LeakTracker<URLRequest> leak_tracker_;

  // Callback passed to the network delegate to notify us when a blocked request
  // is ready to be resumed or canceled.
  CompletionCallback before_request_callback_;

  // Safe-guard to ensure that we do not send multiple "I am completed"
  // messages to network delegate.
  // TODO(battre): Remove this. http://crbug.com/89049
  bool has_notified_completion_;

  // Authentication data used by the NetworkDelegate for this request,
  // if one is present. |auth_credentials_| may be filled in when calling
  // |NotifyAuthRequired| on the NetworkDelegate. |auth_info_| holds
  // the authentication challenge being handled by |NotifyAuthRequired|.
  AuthCredentials auth_credentials_;
  scoped_refptr<AuthChallengeInfo> auth_info_;

  int64 received_response_content_length_;

  base::TimeTicks creation_time_;

  // Timing information for the most recent request.  Its start times are
  // populated during Start(), and the rest are populated in OnResponseReceived.
  LoadTimingInfo load_timing_info_;

  scoped_ptr<const base::debug::StackTrace> stack_trace_;

  // Keeps track of whether or not OnBeforeNetworkStart has been called yet.
  bool notified_before_network_start_;

  // The cookie store to be used for this request.
  scoped_refptr<CookieStore> cookie_store_;

  // The proxy server used for this request, if any.
  HostPortPair proxy_server_;

  DISALLOW_COPY_AND_ASSIGN(URLRequest);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_H_
