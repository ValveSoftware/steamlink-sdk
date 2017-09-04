// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_url_fetcher.h"

#include "components/data_use_measurement/core/data_use_user_data.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using net::URLFetcher;

namespace dom_distiller {

DistillerURLFetcherFactory::DistillerURLFetcherFactory(
    net::URLRequestContextGetter* context_getter)
  : context_getter_(context_getter) {
}

DistillerURLFetcher*
DistillerURLFetcherFactory::CreateDistillerURLFetcher() const {
  return new DistillerURLFetcher(context_getter_);
}


DistillerURLFetcher::DistillerURLFetcher(
    net::URLRequestContextGetter* context_getter)
  : context_getter_(context_getter) {
}

DistillerURLFetcher::~DistillerURLFetcher() {
}

void DistillerURLFetcher::FetchURL(const std::string& url,
                                   const URLFetcherCallback& callback) {
  // Don't allow a fetch if one is pending.
  DCHECK(!url_fetcher_ || !url_fetcher_->GetStatus().is_io_pending());
  callback_ = callback;
  url_fetcher_ = CreateURLFetcher(context_getter_, url);
  url_fetcher_->Start();
}

std::unique_ptr<URLFetcher> DistillerURLFetcher::CreateURLFetcher(
    net::URLRequestContextGetter* context_getter,
    const std::string& url) {
  std::unique_ptr<net::URLFetcher> fetcher =
      URLFetcher::Create(GURL(url), URLFetcher::GET, this);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher.get(), data_use_measurement::DataUseUserData::DOM_DISTILLER);
  fetcher->SetRequestContext(context_getter);
  static const int kMaxRetries = 5;
  fetcher->SetMaxRetriesOn5xx(kMaxRetries);
  return fetcher;
}

void DistillerURLFetcher::OnURLFetchComplete(
    const URLFetcher* source) {
  std::string response;
  if (source && source->GetStatus().is_success() &&
      source->GetResponseCode() == net::HTTP_OK) {
    // Only copy over the data if the request was successful. Insert
    // an empty string into the proto otherwise.
    source->GetResponseAsString(&response);
  }
  callback_.Run(response);
}

}  // namespace dom_distiller
