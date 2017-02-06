// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}

namespace image_fetcher {

// A class used to fetch server images. It can be called from any thread and the
// callback will be called on the thread which initiated the fetch.
class ImageFetcher {
 public:
  ImageFetcher() {}
  virtual ~ImageFetcher() {}

  virtual void SetImageFetcherDelegate(ImageFetcherDelegate* delegate) = 0;

  // An empty gfx::Image will be returned to the callback in case the image
  // could not be fetched.
  virtual void StartOrQueueNetworkRequest(
      const std::string& id,
      const GURL& image_url,
      base::Callback<void(const std::string&, const gfx::Image&)> callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageFetcher);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_H_
