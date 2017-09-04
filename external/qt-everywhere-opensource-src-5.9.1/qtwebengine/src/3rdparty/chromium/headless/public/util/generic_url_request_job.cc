// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/generic_url_request_job.h"

#include <string.h>
#include <algorithm>

#include "base/logging.h"
#include "headless/public/util/url_request_dispatcher.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"

namespace headless {
namespace {

// True if the request method is "safe" (per section 4.2.1 of RFC 7231).
bool IsMethodSafe(const std::string& method) {
  return method == "GET" || method == "HEAD" || method == "OPTIONS" ||
         method == "TRACE";
}

}  // namespace

GenericURLRequestJob::GenericURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    URLRequestDispatcher* url_request_dispatcher,
    std::unique_ptr<URLFetcher> url_fetcher,
    Delegate* delegate)
    : ManagedDispatchURLRequestJob(request,
                                   network_delegate,
                                   url_request_dispatcher),
      url_fetcher_(std::move(url_fetcher)),
      delegate_(delegate),
      weak_factory_(this) {}

GenericURLRequestJob::~GenericURLRequestJob() = default;

void GenericURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  extra_request_headers_ = headers;
}

void GenericURLRequestJob::Start() {
  auto callback = [this](RewriteResult result, const GURL& url,
                         const std::string& method) {
    switch (result) {
      case RewriteResult::kAllow:
        // Note that we use the rewritten url for selecting cookies.
        // Also, rewriting does not affect the request initiator.
        PrepareCookies(url, method, url::Origin(url));
        break;
      case RewriteResult::kDeny:
        DispatchStartError(net::ERR_FILE_NOT_FOUND);
        break;
      case RewriteResult::kFailure:
        DispatchStartError(net::ERR_UNEXPECTED);
        break;
      default:
        DCHECK(false);
    }
  };

  if (!delegate_->BlockOrRewriteRequest(request_->url(), request_->method(),
                                        request_->referrer(), callback)) {
    PrepareCookies(request_->url(), request_->method(),
                   url::Origin(request_->first_party_for_cookies()));
  }
}

void GenericURLRequestJob::PrepareCookies(const GURL& rewritten_url,
                                          const std::string& method,
                                          const url::Origin& site_for_cookies) {
  net::CookieStore* cookie_store = request_->context()->cookie_store();
  net::CookieOptions options;
  options.set_include_httponly();

  // See net::URLRequestHttpJob::AddCookieHeaderAndStart().
  url::Origin requested_origin(rewritten_url);
  if (net::registry_controlled_domains::SameDomainOrHost(
          requested_origin, site_for_cookies,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    if (net::registry_controlled_domains::SameDomainOrHost(
            requested_origin, request_->initiator(),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      options.set_same_site_cookie_mode(
          net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
    } else if (IsMethodSafe(request_->method())) {
      options.set_same_site_cookie_mode(
          net::CookieOptions::SameSiteCookieMode::INCLUDE_LAX);
    }
  }

  cookie_store->GetCookieListWithOptionsAsync(
      rewritten_url, options,
      base::Bind(&GenericURLRequestJob::OnCookiesAvailable,
                 weak_factory_.GetWeakPtr(), rewritten_url, method));
}

void GenericURLRequestJob::OnCookiesAvailable(
    const GURL& rewritten_url,
    const std::string& method,
    const net::CookieList& cookie_list) {
  // TODO(alexclarke): Set user agent.
  // Pass cookies, the referrer and any extra headers into the fetch request.
  extra_request_headers_.SetHeader(
      net::HttpRequestHeaders::kCookie,
      net::CookieStore::BuildCookieLine(cookie_list));

  extra_request_headers_.SetHeader(net::HttpRequestHeaders::kReferer,
                                   request_->referrer());

  // The resource may have been supplied in the request.
  const HttpResponse* matched_resource = delegate_->MaybeMatchResource(
      rewritten_url, method, extra_request_headers_);

  if (matched_resource) {
    OnFetchCompleteExtractHeaders(
        matched_resource->final_url, matched_resource->http_response_code,
        matched_resource->response_data, matched_resource->response_data_size);
  } else {
    url_fetcher_->StartFetch(rewritten_url, method, extra_request_headers_,
                             this);
  }
}

void GenericURLRequestJob::OnFetchStartError(net::Error error) {
  DispatchStartError(error);
}

void GenericURLRequestJob::OnFetchComplete(
    const GURL& final_url,
    int http_response_code,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const char* body,
    size_t body_size) {
  response_time_ = base::TimeTicks::Now();
  http_response_code_ = http_response_code;
  response_headers_ = response_headers;
  body_ = body;
  body_size_ = body_size;

  DispatchHeadersComplete();

  std::string mime_type;
  GetMimeType(&mime_type);

  delegate_->OnResourceLoadComplete(final_url, mime_type, http_response_code);
}

int GenericURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  // TODO(skyostil): Implement ranged fetches.
  // TODO(alexclarke): Add coverage for all the cases below.
  size_t bytes_available = body_size_ - read_offset_;
  size_t bytes_to_copy =
      std::min(static_cast<size_t>(buf_size), bytes_available);
  if (bytes_to_copy) {
    std::memcpy(buf->data(), &body_[read_offset_], bytes_to_copy);
    read_offset_ += bytes_to_copy;
  }
  return bytes_to_copy;
}

int GenericURLRequestJob::GetResponseCode() const {
  return http_response_code_;
}

void GenericURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  info->headers = response_headers_;
}

bool GenericURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!response_headers_)
    return false;
  return response_headers_->GetMimeType(mime_type);
}

bool GenericURLRequestJob::GetCharset(std::string* charset) {
  if (!response_headers_)
    return false;
  return response_headers_->GetCharset(charset);
}

void GenericURLRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  // TODO(alexclarke): Investigate setting the other members too where possible.
  load_timing_info->receive_headers_end = response_time_;
}

}  // namespace headless
