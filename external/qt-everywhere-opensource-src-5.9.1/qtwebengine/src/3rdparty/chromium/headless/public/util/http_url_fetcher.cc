// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/http_url_fetcher.h"

#include "net/base/io_buffer.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace headless {

class HttpURLFetcher::Delegate : public net::URLRequest::Delegate {
 public:
  Delegate(const GURL& rewritten_url,
           const std::string& method,
           const net::HttpRequestHeaders& request_headers,
           const net::URLRequestContext* url_request_context,
           ResultListener* result_listener);
  ~Delegate() override;

  // URLRequest::Delegate methods:
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* auth_info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int num_bytes) override;

 private:
  enum { kBufSize = 4096 };

  bool ConsumeBytesRead(net::URLRequest* request, int num_bytes);
  void ReadBody(net::URLRequest* request);
  void OnResponseCompleted(net::URLRequest* request, int net_error);

  // Holds the error condition that was hit by the request, or OK.
  int result_code_;

  // Buffer that URLRequest writes into.
  scoped_refptr<net::IOBuffer> buf_;

  // The HTTP fetch.
  std::unique_ptr<net::URLRequest> request_;

  // Holds the bytes read so far.
  std::string bytes_read_so_far_;

  // The interface kn which to report any results.
  ResultListener* result_listener_;  // NOT OWNED.
};

HttpURLFetcher::Delegate::Delegate(
    const GURL& rewritten_url,
    const std::string& method,
    const net::HttpRequestHeaders& request_headers,
    const net::URLRequestContext* url_request_context,
    ResultListener* result_listener)
    : result_code_(net::OK),
      buf_(new net::IOBuffer(kBufSize)),
      request_(url_request_context->CreateRequest(rewritten_url,
                                                  net::DEFAULT_PRIORITY,
                                                  this)),
      result_listener_(result_listener) {
  request_->set_method(method);
  request_->SetExtraRequestHeaders(request_headers);
  request_->Start();
}

HttpURLFetcher::Delegate::~Delegate() {}

void HttpURLFetcher::Delegate::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  DCHECK_EQ(request, request_.get());
  LOG(WARNING) << "Auth required to fetch URL, aborting.";
  result_code_ = net::ERR_NOT_IMPLEMENTED;
  request->CancelAuth();
}

void HttpURLFetcher::Delegate::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DCHECK_EQ(request, request_.get());

  // Revocation check failures are not fatal.
  if (net::IsCertStatusMinorError(ssl_info.cert_status)) {
    request->ContinueDespiteLastError();
    return;
  }
  LOG(WARNING) << "SSL certificate error, aborting.";

  // Certificate errors are in same space as net errors.
  result_code_ = net::MapCertStatusToNetError(ssl_info.cert_status);
  request->Cancel();
}

void HttpURLFetcher::Delegate::OnResponseStarted(net::URLRequest* request,
                                                 int net_error) {
  DCHECK_EQ(request, request_.get());
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (net_error != net::OK) {
    OnResponseCompleted(request, net_error);
    return;
  }

  ReadBody(request);
}

void HttpURLFetcher::Delegate::OnReadCompleted(net::URLRequest* request,
                                               int num_bytes) {
  DCHECK_EQ(request, request_.get());
  DCHECK_NE(net::ERR_IO_PENDING, num_bytes);

  if (ConsumeBytesRead(request, num_bytes)) {
    // Keep reading.
    ReadBody(request);
  }
}

void HttpURLFetcher::Delegate::ReadBody(net::URLRequest* request) {
  // Read as many bytes as are available synchronously.
  while (true) {
    int num_bytes = request->Read(buf_.get(), kBufSize);
    if (num_bytes == net::ERR_IO_PENDING)
      return;

    if (num_bytes < 0) {
      OnResponseCompleted(request, num_bytes);
      return;
    }

    if (!ConsumeBytesRead(request, num_bytes))
      return;
  }
}

bool HttpURLFetcher::Delegate::ConsumeBytesRead(net::URLRequest* request,
                                                int num_bytes) {
  if (num_bytes <= 0) {
    // Error while reading, or EOF.
    OnResponseCompleted(request, num_bytes);
    return false;
  }

  bytes_read_so_far_.append(buf_->data(), num_bytes);
  return true;
}

void HttpURLFetcher::Delegate::OnResponseCompleted(net::URLRequest* request,
                                                   int net_error) {
  DCHECK_EQ(request, request_.get());

  if (result_code_ != net::OK) {
    result_listener_->OnFetchStartError(static_cast<net::Error>(result_code_));
    return;
  }

  if (net_error != net::OK) {
    result_listener_->OnFetchStartError(static_cast<net::Error>(net_error));
    return;
  }

  // TODO(alexclarke) apart from the headers there's a lot of stuff in
  // |request->response_info()| that we drop here.  Find a way to pipe it
  // through.
  result_listener_->OnFetchComplete(
      request->url(), request->GetResponseCode(),
      request->response_info().headers,
      bytes_read_so_far_.c_str(), bytes_read_so_far_.size());
}

HttpURLFetcher::HttpURLFetcher(
    const net::URLRequestContext* url_request_context)
    : url_request_context_(url_request_context) {}

HttpURLFetcher::~HttpURLFetcher() {}

void HttpURLFetcher::StartFetch(const GURL& rewritten_url,
                                const std::string& method,
                                const net::HttpRequestHeaders& request_headers,
                                ResultListener* result_listener) {
  delegate_.reset(new Delegate(rewritten_url, method, request_headers,
                               url_request_context_, result_listener));
}

}  // namespace headless
