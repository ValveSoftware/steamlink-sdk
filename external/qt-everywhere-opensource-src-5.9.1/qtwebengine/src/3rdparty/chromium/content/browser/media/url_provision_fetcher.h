// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_URL_PROVISION_FETCHER_H_
#define CONTENT_BROWSER_MEDIA_URL_PROVISION_FETCHER_H_

#include "base/macros.h"
#include "media/base/provision_fetcher.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace content {

// The ProvisionFetcher that retrieves the data by HTTP POST request.

class URLProvisionFetcher : public media::ProvisionFetcher,
                            public net::URLFetcherDelegate {
 public:
  explicit URLProvisionFetcher(net::URLRequestContextGetter* context_getter);
  ~URLProvisionFetcher() override;

  // media::ProvisionFetcher implementation.
  void Retrieve(const std::string& default_url,
                const std::string& request_data,
                const ProvisionFetcher::ResponseCB& response_cb) override;

 private:
  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  net::URLRequestContextGetter* context_getter_;
  std::unique_ptr<net::URLFetcher> request_;
  media::ProvisionFetcher::ResponseCB response_cb_;

  DISALLOW_COPY_AND_ASSIGN(URLProvisionFetcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_URL_PROVISION_FETCHER_H_
