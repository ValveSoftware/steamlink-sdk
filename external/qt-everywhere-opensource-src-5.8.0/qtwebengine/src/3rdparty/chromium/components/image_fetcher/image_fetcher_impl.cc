// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/image_fetcher_impl.h"

#include <string>

#include "base/bind.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"

namespace image_fetcher {

ImageFetcherImpl::ImageFetcherImpl(
    std::unique_ptr<ImageDecoder> image_decoder,
    net::URLRequestContextGetter* url_request_context)
    : delegate_(nullptr), url_request_context_(url_request_context),
      image_decoder_(std::move(image_decoder)),
      image_data_fetcher_(new ImageDataFetcher(url_request_context_.get())) {
}

ImageFetcherImpl::~ImageFetcherImpl() {}

ImageFetcherImpl::ImageRequest::ImageRequest() {}

ImageFetcherImpl::ImageRequest::ImageRequest(const ImageRequest& other) =
    default;

ImageFetcherImpl::ImageRequest::~ImageRequest() { }

void ImageFetcherImpl::SetImageFetcherDelegate(ImageFetcherDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void ImageFetcherImpl::StartOrQueueNetworkRequest(
    const std::string& id,
    const GURL& image_url,
    base::Callback<void(const std::string&, const gfx::Image&)> callback) {
  // Before starting to fetch the image. Look for a request in progress for
  // |image_url|, and queue if appropriate.
  ImageRequestMap::iterator it = pending_net_requests_.find(image_url);
  if (it == pending_net_requests_.end()) {
    ImageRequest request;
    request.id = id;
    request.callbacks.push_back(callback);
    pending_net_requests_[image_url].swap(&request);

    image_data_fetcher_->FetchImageData(
        image_url,
        base::Bind(&ImageFetcherImpl::OnImageURLFetched,
                   base::Unretained(this), image_url));
  } else {
    // Request in progress. Register as an interested callback.
    it->second.callbacks.push_back(callback);
  }
}

void ImageFetcherImpl::OnImageURLFetched(const GURL& image_url,
                                         const std::string& image_data) {
  // Inform the ImageFetcherDelegate.
  if (delegate_) {
    auto it = pending_net_requests_.find(image_url);
    DCHECK(it != pending_net_requests_.end());
    delegate_->OnImageDataFetched(it->second.id, image_data);
  }

  image_decoder_->DecodeImage(
      image_data,
      base::Bind(&ImageFetcherImpl::OnImageDecoded,
                 base::Unretained(this), image_url));
}

void ImageFetcherImpl::OnImageDecoded(const GURL& image_url,
                                      const gfx::Image& image) {
  // Get request for the given image_url from the request queue.
  ImageRequestMap::iterator image_iter = pending_net_requests_.find(image_url);
  DCHECK(image_iter != pending_net_requests_.end());
  ImageRequest* request = &image_iter->second;

  // Run all callbacks
  for (const auto& callback : request->callbacks) {
    callback.Run(request->id, image);
  }

  // Inform the ImageFetcherDelegate.
  if (delegate_) {
    delegate_->OnImageFetched(request->id, image);
  }

  // Erase the completed ImageRequest.
  pending_net_requests_.erase(image_iter);
}

}  // namespace image_fetcher
