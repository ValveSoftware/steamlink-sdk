// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher.h"

#include "net/url_request/url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"

namespace net {

URLFetcher::~URLFetcher() {}

// static
URLFetcher* net::URLFetcher::Create(
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d) {
  return URLFetcher::Create(0, url, request_type, d);
}

// static
URLFetcher* net::URLFetcher::Create(
    int id,
    const GURL& url,
    URLFetcher::RequestType request_type,
    URLFetcherDelegate* d) {
  URLFetcherFactory* factory = URLFetcherImpl::factory();
  return factory ? factory->CreateURLFetcher(id, url, request_type, d)
                 : new URLFetcherImpl(url, request_type, d);
}

// static
void net::URLFetcher::CancelAll() {
  URLFetcherImpl::CancelAll();
}

// static
void net::URLFetcher::SetEnableInterceptionForTests(bool enabled) {
  URLFetcherImpl::SetEnableInterceptionForTests(enabled);
}

// static
void net::URLFetcher::SetIgnoreCertificateRequests(bool ignored) {
  URLFetcherImpl::SetIgnoreCertificateRequests(ignored);
}

}  // namespace net
