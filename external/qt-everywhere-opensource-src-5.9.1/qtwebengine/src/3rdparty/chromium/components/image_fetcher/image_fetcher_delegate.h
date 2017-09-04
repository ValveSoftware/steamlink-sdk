// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_DELEGATE_H_
#define COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_DELEGATE_H_

#include <string>

#include "base/macros.h"

namespace gfx {
class Image;
}

namespace image_fetcher {

class ImageFetcherDelegate {
 public:
  ImageFetcherDelegate() {}

  // Called when the data for an image was fetched. |id| is an identifier for
  // the fetch (as passed to ImageFetcher::StartOrQueueNetworkRequest); |data|
  // stores (generally compressed) image data owned by the caller, and can be
  // empty if the fetch failed.
  virtual void OnImageDataFetched(const std::string& id,
                                  const std::string& data) {};

  // Called when an image was fetched and decoded. |id| is an identifier for the
  // fetch (as passed to ImageFetcher::StartOrQueueNetworkRequest); |image|
  // stores image data owned by the caller, and can be an empty gfx::Image.
  virtual void OnImageFetched(const std::string& id,
                              const gfx::Image& image) {};

 protected:
  virtual ~ImageFetcherDelegate() {}

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherDelegate);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_DELEGATE_H_
