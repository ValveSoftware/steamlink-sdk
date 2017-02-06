// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_IMPL_H_
#define COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/image_fetcher/image_data_fetcher.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}

namespace net {
class URLRequestContextGetter;
}

namespace image_fetcher {

// TODO(markusheintz): Once the iOS implementation of the ImageFetcher is
// removed merge the two classes ImageFetcher and ImageFetcherImpl.
class ImageFetcherImpl : public image_fetcher::ImageFetcher {
 public:
  ImageFetcherImpl(
      std::unique_ptr<ImageDecoder> image_decoder,
      net::URLRequestContextGetter* url_request_context);
  ~ImageFetcherImpl() override;

  // Sets the |delegate| of the ImageFetcherImpl. The |delegate| has to be alive
  // during the lifetime of the ImageFetcherImpl object. It is the caller's
  // responsibility to ensure this.
  void SetImageFetcherDelegate(ImageFetcherDelegate* delegate) override;

  void StartOrQueueNetworkRequest(
      const std::string& id,
      const GURL& image_url,
      base::Callback<void(const std::string&, const gfx::Image&)> callback)
      override;

 private:
  using CallbackVector =
      std::vector<base::Callback<void(const std::string&, const gfx::Image&)>>;

  // State related to an image fetch (id, pending callbacks).
  struct ImageRequest {
    ImageRequest();
    ImageRequest(const ImageRequest& other);
    ~ImageRequest();

    void swap(ImageRequest* other) {
      std::swap(id, other->id);
      std::swap(callbacks, other->callbacks);
    }

    std::string id;
    // Queue for pending callbacks, which may accumulate while the request is in
    // flight.
    CallbackVector callbacks;
  };

  using ImageRequestMap = std::map<const GURL, ImageRequest>;

  // Processes image URL fetched events. This is the continuation method used
  // for creating callbacks that are passed to the ImageDataFetcher.
  void OnImageURLFetched(const GURL& image_url, const std::string& image_data);

  // Processes image decoded events. This is the continuation method used for
  // creating callbacks that are passed to the ImageDecoder.
  void OnImageDecoded(const GURL& image_url, const gfx::Image& image);


  ImageFetcherDelegate* delegate_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_;

  std::unique_ptr<ImageDecoder> image_decoder_;

  std::unique_ptr<ImageDataFetcher> image_data_fetcher_;

  // Map from each image URL to the request information (associated website
  // url, fetcher, pending callbacks).
  ImageRequestMap pending_net_requests_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherImpl);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_IMPL_H_
