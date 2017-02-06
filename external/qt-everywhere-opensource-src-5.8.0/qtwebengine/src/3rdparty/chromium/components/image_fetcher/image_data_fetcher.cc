// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/image_data_fetcher.h"

#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace image_fetcher {

// An active image URL fetcher request. The struct contains the related requests
// state.
struct ImageDataFetcher::ImageDataFetcherRequest {
  ImageDataFetcherRequest(const ImageDataFetcherCallback& callback,
                          std::unique_ptr<net::URLFetcher> url_fetcher)
    : callback(callback),
      url_fetcher(std::move(url_fetcher)) {}

  ~ImageDataFetcherRequest() {}

  // The callback to run after the image data was fetched. The callback will
  // be run even if the image data could not be fetched successfully.
  ImageDataFetcherCallback callback;

  std::unique_ptr<net::URLFetcher> url_fetcher;
};

ImageDataFetcher::ImageDataFetcher(
    net::URLRequestContextGetter* url_request_context_getter)
    : url_request_context_getter_(url_request_context_getter),
      next_url_fetcher_id_(0) {}

ImageDataFetcher::~ImageDataFetcher() {}

void ImageDataFetcher::FetchImageData(
    const GURL& url, const ImageDataFetcherCallback& callback) {
  std::unique_ptr<net::URLFetcher> url_fetcher =
      net::URLFetcher::Create(
          next_url_fetcher_id_++, url, net::URLFetcher::GET, this);

  std::unique_ptr<ImageDataFetcherRequest> request(
      new ImageDataFetcherRequest(callback, std::move(url_fetcher)));
  request->url_fetcher->SetRequestContext(url_request_context_getter_.get());
  request->url_fetcher->Start();

  pending_requests_[request->url_fetcher.get()] = std::move(request);
}

void ImageDataFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  auto request_iter = pending_requests_.find(source);
  DCHECK(request_iter != pending_requests_.end());

  std::string image_data;
  if (source->GetStatus().status() == net::URLRequestStatus::SUCCESS) {
    source->GetResponseAsString(&image_data);
  }
  request_iter->second->callback.Run(image_data);

  // Remove the finished request.
  pending_requests_.erase(request_iter);
}

}  // namespace image_fetcher
