// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_GENERIC_URL_REQUEST_JOB_H_
#define HEADLESS_PUBLIC_UTIL_GENERIC_URL_REQUEST_JOB_H_

#include <stddef.h>
#include <functional>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "headless/public/util/managed_dispatch_url_request_job.h"
#include "headless/public/util/url_fetcher.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
class IOBuffer;
}  // namespace net

namespace headless {

class URLRequestDispatcher;

// Intended for use in a protocol handler, this ManagedDispatchURLRequestJob has
// the following features:
//
// 1. The delegate can extension observe / cancel and redirect requests
// 2. The delegate can optionally provide the results, otherwise the specifed
//    fetcher is invoked.
class GenericURLRequestJob : public ManagedDispatchURLRequestJob,
                             public URLFetcher::ResultListener {
 public:
  enum class RewriteResult { kAllow, kDeny, kFailure };
  using RewriteCallback = std::function<
      void(RewriteResult result, const GURL& url, const std::string& method)>;

  struct HttpResponse {
    GURL final_url;
    int http_response_code;

    // The HTTP headers and response body. Note the lifetime of |response_data|
    // is expected to outlive the GenericURLRequestJob.
    const char* response_data;  // NOT OWNED
    size_t response_data_size;
  };

  class Delegate {
   public:
    // Allows the delegate to rewrite the URL for a given request. Return true
    // to signal that the rewrite is in progress and |callback| will be called
    // with the result, or false to indicate that no rewriting is necessary.
    // Called on an arbitrary thread.
    virtual bool BlockOrRewriteRequest(const GURL& url,
                                       const std::string& method,
                                       const std::string& referrer,
                                       RewriteCallback callback) = 0;

    // Allows the delegate to synchronously fulfill a request with a reply.
    // Called on an arbitrary thread.
    virtual const HttpResponse* MaybeMatchResource(
        const GURL& url,
        const std::string& method,
        const net::HttpRequestHeaders& request_headers) = 0;

    // Signals that a resource load has finished. Called on an arbitrary thread.
    virtual void OnResourceLoadComplete(const GURL& final_url,
                                        const std::string& mime_type,
                                        int http_response_code) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // NOTE |url_request_dispatcher| and |delegate| must outlive the
  // GenericURLRequestJob.
  GenericURLRequestJob(net::URLRequest* request,
                       net::NetworkDelegate* network_delegate,
                       URLRequestDispatcher* url_request_dispatcher,
                       std::unique_ptr<URLFetcher> url_fetcher,
                       Delegate* delegate);
  ~GenericURLRequestJob() override;

  // net::URLRequestJob implementation:
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  void Start() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  int GetResponseCode() const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  bool GetMimeType(std::string* mime_type) const override;
  bool GetCharset(std::string* charset) override;
  void GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;

  // URLFetcher::FetchResultListener implementation:
  void OnFetchStartError(net::Error error) override;
  void OnFetchComplete(const GURL& final_url,
                       int http_response_code,
                       scoped_refptr<net::HttpResponseHeaders> response_headers,
                       const char* body,
                       size_t body_size) override;

 private:
  void PrepareCookies(const GURL& rewritten_url,
                      const std::string& method,
                      const url::Origin& site_for_cookies);

  void OnCookiesAvailable(const GURL& rewritten_url,
                          const std::string& method,
                          const net::CookieList& cookie_list);

  std::unique_ptr<URLFetcher> url_fetcher_;
  net::HttpRequestHeaders extra_request_headers_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  Delegate* delegate_;          // Not owned.
  const char* body_ = nullptr;  // Not owned.
  int http_response_code_ = 0;
  size_t body_size_ = 0;
  size_t read_offset_ = 0;
  base::TimeTicks response_time_;

  base::WeakPtrFactory<GenericURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GenericURLRequestJob);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_GENERIC_URL_REQUEST_JOB_H_
